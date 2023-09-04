#include <webrtc/webrtc.hpp>
#include <media/media.hpp>

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
    const uri https_res = "https://ar-visions.com:10022"; /// certs based on this binding name unless configured

    /// take in sample (image, images, or canvas with bitmask backend)

    /// App services composition layer (do we call this services?)
    return Services([&](Services &app) {
        return array<node> {

            VideoStream { /// frames are given here, and the Services api should handle ideal fetching
                { "id",             "streamer" },
                { "source",         "" }
            },

            /// ws-signal connector
            Service {
                    { "id",         "ws-signal" },
                    { "url",        ws_signal },
                    { "on-message", lambda<message(message&)>([](message &ws_message) -> message {
                int test = 0;
                test++;
                return message {}; /// this should not respond
                ///
            })}},

            /// https resource server
            Service {{ "id", "https" },
                     { "url", https_res },
                     { "root", "www" },
                     { "on-message", lambda<message(message&)>([](message &msg) -> message {
                ///
                console.log("message = {0}: {1}", { msg->code, msg->query });
                ///
                str s_query = msg->query->query;
                if (s_query[0] == '/')
                    s_query = s_query.mid(1);
                if(!s_query || s_query[s_query.len() - 1] == '/') /// needs to support /.
                    s_query += "index.html";
                
                /// verify query does not go beyond www
                if (!path::backwards(s_query.cs())) {
                    path p = fmt { "www/{0}", { s_query } };

                    if (p.exists()) {
                        console.log("sending resource: {0}", { p.cs() });
                        return message(p, p.mime_type());
                    }
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
