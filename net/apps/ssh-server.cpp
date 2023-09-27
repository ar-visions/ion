

#include <net/ssh.hpp>

using namespace ion;

int main(int argc, char **argv) {
    /// parse args with defaults; print when not enough given
    ion::map<mx> defs {
        {"host",    str("ar-visions.com")},
        {"port",    int(10022)}
    };
    ion::map<mx> config = args::parse(argc, argv, defs);
    if (!config) return   args::defaults(defs);

    const uri bind = fmt {"ssh://{0}:{1}", { config["host"], config["port"] }};
    SSHService service;

    return Services([&](Services &app) {
        return ion::array<node> {
            SSHService {
                { "id",   "ssh" },
                { "bind",  bind },
                { "ref",     lambda<void(mx)>           ([&](mx obj)                     { service = obj.grab(); })},
                { "on-auth", lambda<bool(str, str, str)>([&](str id, str user, str pass) { return user == "admin" && pass == "admin"; })},
                { "on-peer", lambda<void(SSHPeer)>      ([&](SSHPeer peer)               { console.log("peer connected: {0}", { peer->id }); })},
                { "on-recv", lambda<void(SSHPeer, str)> ([&](SSHPeer peer, str msg) {
                    /// relay clients message back
                    service.send_message(peer, msg);
                })}
            }
        };
    });
}