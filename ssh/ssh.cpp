#include <net/net.hpp>

#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>  // Optional, for additional features
#else
#include <arpa/inet.h>
#endif

#define SSH_IMPL
#include <ssh/ssh.hpp>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#endif

using namespace ion;

void SSHPeer::pdata::disconnect() {
    ssh_disconnect(session);
    ssh_free(session);
    session = null;
}

bool SSHService::ssh_init() {
    str     host     = state->bind.host();
    str     addr     = state->bind.addr();
    int     port     = state->bind.port();
    path    file_pri  = fmt { "ssl/{0}.ssh",     { host }};
    path    file_pub  = fmt { "ssl/{0}.ssh.pub", { host }};
    path    ssl { "ssl" };

    ::ssh_init();
    if (!ssl.exists())
        ssl.make_dir();

    if (file_pri.exists() && file_pub.exists()) {
        printf("ssh: found keys\n");
    } else {
        file_pri.remove(); /// the command prompts the user if the file exists
        file_pub.remove();
        exec cmd = fmt { "ssh-keygen -t rsa -b 4096 -f {0} -q -N \"\"", { file_pri.cs() }};
        async(cmd).sync(); /// run command, generating keys
    }

    state->sshbind = ssh_bind_new();
    ssh_bind_options_set(state->sshbind, SSH_BIND_OPTIONS_HOSTKEY, file_pri.cs());
    ssh_bind_options_set(state->sshbind, SSH_BIND_OPTIONS_BINDADDR, addr.cs());
    ssh_bind_options_set(state->sshbind, SSH_BIND_OPTIONS_BINDPORT, &port);

    if (ssh_bind_listen(state->sshbind) < 0) {
        console.log("Error listening to socket: {0}", { ssh_get_error(state->sshbind) });
        return 1;
    }

    state->running = true;
    return true;
}

int SSHService::auth_none(ssh_session session,
                    const char *user,
                    void *userdata)
{
    SSHPeer::pdata*   peer = (SSHPeer::pdata *)userdata;
    SSHService::props *ssh = (SSHService::props *)peer->service;

    ssh_set_auth_methods(session, SSH_AUTH_METHOD_PASSWORD);
    if (ssh->banner) {
        ssh_string banner = ssh_string_from_char(ssh->banner.cs());
        if (banner != NULL) {
            ssh_send_issue_banner(session, banner);
        }
        ssh_string_free(banner);
    }
    return SSH_AUTH_DENIED;
}

int SSHService::auth_password(ssh_session session, const char *user, const char *password, void *userdata){
    SSHPeer::pdata*   peer = (SSHPeer::pdata *)userdata;
    SSHService::props *ssh = (SSHService::props *)peer->service;
    
    if (ssh->on_auth(peer->id, str(user), str(password))) {
        peer->is_auth = true;
        printf("authenticated\n");
        return SSH_AUTH_SUCCESS;
    }

    if (peer->tries++ > ssh->max_attempts) {
        printf("authentication fail\n");
        ssh_disconnect(session);
        return SSH_AUTH_DENIED;
    }
    
    return SSH_AUTH_DENIED;
}

int SSHService::pty_request(ssh_session session, ssh_channel channel, const char *term,
        int x,int y, int px, int py, void *userdata) {
    printf("Allocated terminal\n");
    return 0;
}

int SSHService::shell_request(ssh_session session, ssh_channel channel, void *userdata) {
    printf("Allocated shell\n");
    return 0;
}

ssh_channel SSHService::new_session_channel(ssh_session session, void *userdata){
    /// static just in case it doesnt copy
    static struct ssh_channel_callbacks_struct channel_cb = {
        .channel_pty_request_function   = pty_request,
        .channel_shell_request_function = shell_request
    };
    SSHPeer::pdata *peer = (SSHPeer::pdata *)userdata;
    printf("Allocated session channel\n");
    peer->chan = ssh_channel_new(session);
    ssh_callbacks_init(&channel_cb);
    ssh_set_channel_callbacks(peer->chan, &channel_cb);
    return peer->chan;
}

SSHPeer SSHService::accept() {
    const char *name = state->bind.host().cs();
    int         port = state->bind.port();
    SSHPeer     peer;
    
    peer->service = (raw_t)state;

    /// loop until we are to return a peer
    for (;;) {
        peer->session = ssh_new();
        int r = ssh_bind_accept(state->sshbind, peer->session);
        if (r == SSH_ERROR) {
            printf("error accepting a connection : %s\n",ssh_get_error(state->sshbind));
            break;
        }

        int fd_session = ssh_get_fd(peer->session);
        struct sockaddr_storage tmp;
        socklen_t len = sizeof(tmp);
        getpeername(fd_session, (struct sockaddr*)&tmp, &len);
        struct sockaddr_in *sock = (struct sockaddr_in*)&tmp;
        char ip_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sock->sin_addr, ip_addr, INET_ADDRSTRLEN);

        printf("IP address of the session: %s\n", ip_addr);
        peer->address = ip_addr;
        peer->port    = ntohs(sock->sin_port);
        peer->id      = fmt { "{0}:{1}", { peer->address, peer->port }};

        /// set user data to peer, which has service data
        ssh_server_callbacks_struct cb {
            .userdata                              = (void*)peer.data,
            .auth_password_function                = auth_password,
            .auth_none_function                    = auth_none,
            .channel_open_request_session_function = new_session_channel
        };

        /// callback registration
        ssh_callbacks_init(&cb);
        ssh_set_server_callbacks(peer->session, &cb);
        if (ssh_handle_key_exchange(peer->session)) {
            printf("ssh_handle_key_exchange: %s\n", ssh_get_error(peer->session));
            continue;
        }
        ssh_set_auth_methods(peer->session, SSH_AUTH_METHOD_PASSWORD);
        peer->mainloop = ssh_event_new();


        ssh_event_add_session(peer->mainloop, peer->session);

        /// authenticate
        bool error = false;
        while (!(peer->is_auth && peer->chan != NULL)){
            r = ssh_event_dopoll(peer->mainloop, -1);
            if (r == SSH_ERROR) {
                printf("Error : %s\n",ssh_get_error(peer->session));
                peer->disconnect();
                error = true;
                break;
            }
        }
        
        /// the above may need to be in its own thread
        if (error) {
            printf("error during handshake, resuming next accept\n");
            continue;
        }

        printf("Authenticated and got a channel\n");
        peer->connected = true;
        if (state->on_peer)
            state->on_peer(peer);

        /// message receive loop; might need a mutex
        async(1, [this, state=state, peer](runtime *rt, int i) -> mx {
            SSHPeer &p = (SSHPeer&)peer;
            int n_bytes = 0;
            doubly<char> chars;
            int          index = 0;
            do {
                char buf[2048];
                n_bytes = ssh_channel_read(p->chan, buf, sizeof(buf), 0);
                if (n_bytes > 0) {
                    str msg { buf, size_t(n_bytes) };
                    if (state->on_recv)
                        state->on_recv(p, msg);
                    if (msg[0] == 13) {
                        char lf[2] = { 10, 0 };
                        send_message(peer, lf);
                    } else if (msg[0] == 0x08 || msg[0] == 0x7F) {          /// Added: Backspace key
                        char backspaceSequence[] = { 0x08, ' ', 0x08, 0 };  /// Move cursor back, overwrite with space, move cursor back again
                        send_message(peer, backspaceSequence);
                    }
                }
            } while (n_bytes > 0);
            p->disconnect();
            if (state->on_disconnect)
                state->on_disconnect(p);
            return true;
        });
        break;
    }

    return peer;
}

/// if we pass a null parameter here it effectively tells us we are broadcasting
bool SSHService::send_message(SSHPeer peer, str msg) {
    printf("%s", msg.cs());
    bool err   = false;
    auto write = [&](SSHPeer &peer) { err |= ssh_channel_write(peer->chan, msg.cs(), msg.len()) != SSH_ERROR; };
    if (peer.is_broadcast()) {
        for (SSHPeer &peer: state->peers)
            write(peer);
    } else
        write(peer);
    return err;
}

void SSHService::mounted() {
    if (state->bind) {
        ssh_init();
        async(1, [this](runtime *rt, int i) -> mx {
            while (state->running) {
                accept();
            }
            ssh_bind_free(state->sshbind);
            //composer::cmdata *composer = ((node*)this)->data->composer;
            ssh_finalize(); /// not until app shutdown?
            return false;
        });
    }
}

