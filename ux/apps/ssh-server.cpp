#include <ssh/ssh.hpp>

using namespace ion;

int main(int argc, char **argv) {
    /// parse args with defaults; print when not enough given
    map defs {
        {"ssh", str("ssh://ar-visions.com:10022")}
    };
    map config { args::parse(argc, argv, defs) };
    if    (!config) return args::defaults(defs);
    
    SSHService service;
    return Services(config, [&](Services &app) {
        return Array<node> {
            SSHService {
                { "id",      "ssh" },
                { "bind",    app["ssh"] },
                { "ref",     lambda<void(mx)>([&](mx obj) {
                    service = obj.hold();
                })},
                { "on-auth", lambda<bool(str, str, str)>([&](str id, str user, str pass) { return user == "admin" && pass == "admin"; })},
                { "on-peer", lambda<void(SSHPeer)>      ([&](SSHPeer peer)               { console.log("peer connected: {0}", { peer->id }); })},
                { "on-recv", lambda<void(SSHPeer, str)> ([&](SSHPeer peer, str msg) {
                    /// relay clients message back
                    console.log(msg);
                    service.send_message(peer, msg);
                })}
            }
        };
    });
}