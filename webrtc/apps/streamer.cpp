#include <webrtc/webrtc.hpp>
#include <media/media.hpp>

/// 
#include <webrtc/streamer/fileparser.hpp>
#include <webrtc/streamer/h264fileparser.hpp>
#include <webrtc/streamer/opusfileparser.hpp>

using namespace std;
using namespace ion;
using namespace webrtc;

int main(int argc, char **argv) try {
    ion::map<mx> defs {
        {"audio",   str("opus")},
        {"video",   str("h264")},
        {"ip",      str("127.0.0.1")},
        {"port",    int(8000)}
    };

    /// parse args with mx parser in map type
    ion::map<mx> config = args::parse(argc, argv, defs);
    if (!config) return args::defaults(defs);

    ///
    const str localId   = "server";
    const uri ws_signal = fmt { "ws://{0}:{1}/{2}", { config["ip"], config["port"], localId }};
    const uri https_res = "https://ar-visions.com:10022";

    /// VideoStream's input types are:
    /// -----------------------------
    /// Element - runtime app
    /// path    - remote executable app
    /// uri     - remote stream (would be nice if we could pass-through)
    /// str     - stream identifier
    /// Stream  - stream instance

    return Services([&](Services &app) {
        return ion::array<node> {
            VideoStream {
                { "id",             "streamer" },
                { "source",         "" },
                { "stream-select",  StreamSelect([](Client client) -> Stream {
                    auto video  = H264FileParser("h264", 30, true);

                    uint64_t a = video.Source::data->getSampleTime_us();

                    auto audio  = OPUSFileParser("opus", true);
                    auto stream = Stream(video, audio);

                    /// we must run an app here
                    return stream;
                    /// --------------------------------
                    /// if you return a path with executable, i wonder if it could run & stream (use winrt & dx11 on windows)
                    /// apps can hold spawn/close multiple windows simultaneously so you would
                    ///  need to actually be able to support that (by not right away)
                    /// its just a data message to indicate new ones and streamable uri's from those idents
                    /// --------------------------------
                })}
            },

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
                if(!s_query || s_query[s_query.len() - 1] == '/')
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
