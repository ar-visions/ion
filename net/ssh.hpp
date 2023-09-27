#pragma once
#include <net/net.hpp>

namespace ion {

#ifndef SSH_IMPL
struct ssh_session_;
struct ssh_channel_;
struct ssh_event_;
struct ssh_bind_;

typedef struct ssh_channel_* ssh_channel;
typedef struct ssh_session_* ssh_session;
typedef struct ssh_event_  * ssh_event;
typedef struct ssh_bind_   * ssh_bind;

#endif

struct SSHPeer:mx {
    struct pdata {
        str                         id;
        str                         address;
        int                         port;
        void*                       service;
        ssh_session                 session;
        bool                        connected;
        bool                        is_auth;
        int                         tries;
        ssh_channel                 chan;
        ssh_event                   mainloop;

        type_register(pdata);

        void disconnect();
    };
    mx_object(SSHPeer, mx, pdata);

    operator bool() {
        return data->connected;
    }
    bool operator!() {
        return !data->connected;
    }
};

struct SSHService:node {
    struct props {
        bool                        running;
        ssh_bind                    sshbind;

        /// meta
        lambda<bool(str, str, str)> on_auth; // user-id, user, pass
        lambda<void(SSHPeer)>       on_peer;
        lambda<void(SSHPeer,str)>   on_recv;
        lambda<void(SSHPeer)>       on_disconnect;

        str                         no_auth;
        uri                         bind;
        str                         banner = "default message here\n";
        int                         max_attempts = 3;

        type_register(props);

        doubly<prop> meta() const {
            return {
                prop { "on-auth",       on_auth       },
                prop { "on-peer",       on_peer       },
                prop { "on-disconnect", on_disconnect },
                prop { "on-recv",       on_recv       },
                prop { "no-auth",       no_auth       },
                prop { "bind",          bind          }
            };
        }
    };

    component(SSHService, node, props);

    private:
    
    static int auth_none        (ssh_session session, const char *user, void *userdata);
    static int auth_password    (ssh_session session, const char *user, const char *password, void *userdata);
    static int pty_request      (ssh_session session, ssh_channel channel, const char *term, int x, int y, int px, int py, void *userdata);
    static int shell_request    (ssh_session session, ssh_channel channel, void *userdata);

    static ssh_channel new_session_channel(ssh_session session, void *userdata);
    
    bool    ssh_init();
    SSHPeer accept();

    public:

    bool send_message(SSHPeer peer, str msg);
    void mounted();
};
}
