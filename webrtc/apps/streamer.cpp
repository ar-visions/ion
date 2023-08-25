
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
    ::args = args::parse(argc, argv, defs);
    if (!::args)
        return args::defaults(defs);

    ///
    str ip_address  =     ::args["ip"];
    int port        = int(::args["port"]);
    ///
    const uri ws_signal = fmt { "ws://{0}:{1}/{2}", { ip_address, port, localId }};
    const uri https_res = "https://localhost:10022";

    /// App services composition layer (do we call this services?)
    return App::services({
        /// service for websocket connection (our session with that two-way data layer)
        service(ws_signaling.cs(), [](message &ws_message) {

        }),
        
        service("https://localhost:10022", [](message &client) -> message {
            console.log("message = {0}: {1}", { msg->code, msg->query });
            ///
            path p = msg->query;
            if (p.exists()) {
                console.log("sending resource: {0}", { p.cs() });
                return message(p, p.mime_type());
            }
            ///
            return message(404);
        })
    });

} catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return -1;
}
