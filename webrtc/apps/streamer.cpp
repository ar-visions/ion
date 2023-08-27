#include <webrtc/webrtc.hpp>

using namespace ion;

int main(int argc, char **argv) try {
    ion::map<mx> defs {
        {"audio",   str("opus")},
        {"video",   str("h264")},
        {"ip",      str("127.0.0.1")},
        {"port",    int(8000)}
    };

    /// parse args with mx parser in map type
    map<mx> config =       args::parse(argc, argv, defs);
    if    (!config) return args::defaults(defs);

    ///
    const str localId   = "server";
    const uri ws_signal = fmt { "ws://{0}:{1}/{2}", { config["ip"], config["port"], localId }};
    const uri https_res = "https://localhost:10022";

    /// App services composition layer (do we call this services?)
    return Services([&](Services &app) {
        return array<node> {
            
            ion::Service {
                    { "url",     ws_signal},
                    { "message", lambda<message(message&)>([](message &ws_message) -> message {
                ///
                return message {}; /// this should not respond
                ///
            })}},

            Service {{ "url", "https://localhost:10022" },
                     { "message", lambda<message(message&)>([](message &msg) -> message {
                ///
                console.log("message = {0}: {1}", { msg->code, msg->query });
                ///
                path p = msg->query;
                if (p.exists()) {
                    console.log("sending resource: {0}", { p.cs() });
                    return message(p, p.mime_type());
                }
                ///
                return message(404);
            })}}
        };
    });

} catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return -1;
}
