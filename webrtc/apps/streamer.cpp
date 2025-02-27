#include <webrtc/webrtc.hpp>
#include <ux/app.hpp>
#include <media/media.hpp>

/// 
#include <webrtc/streamer/fileparser.hpp>
#include <webrtc/streamer/h264fileparser.hpp>
#include <webrtc/streamer/opusfileparser.hpp>

using namespace std;
using namespace ion;
using namespace webrtc;


struct View:Element {
    struct props {
        int         sample;
        int         sample2;
        callback    clicked;
        ///
        properties meta() {
            return {
                prop { "sample",  sample },
                prop { "sample2", sample2 },
                prop { "clicked", clicked}
            };
        }
    };

    int context_var;

    properties meta() {
        return {
            prop { "context_var", context_var }
        };
    }

    component(View, Element, props);

    void mounted() {
        console.log("mounted");
    }
 
    node update() {
        return ion::Array<node> {
            Edit {
                { "content", "Multiline edit test" }
            }
        };
    }
};

int main(int argc, char **argv) {
    ion::map defs {
        {"audio",   str("opus")},
        {"video",   str("h264")},
        {"ip",      str("127.0.0.1")},
        {"port",    int(8000)}
    };

    /// parse args with mx parser in map type
    ion::map config = args::parse(argc, argv, defs);
    if (!config) return args::defaults(defs);

    ///
    const str localId   = "server";
    const uri ws_signal = fmt { "ws://{0}:{1}/{2}", { config["ip"], config["port"], localId }};
    const uri https_res = "https://ar-visions.com:10022"; // App/h264

    /// VideoStream's input types are:
    /// -----------------------------
    /// Element - runtime app
    /// path    - remote executable app
    /// pid     - process id to running app
    /// uri     - remote stream (pass-through)
    /// Stream  - stream instance

    return Services([&](Services &app) {
        return ion::Array<node> {
            VideoStream {
                { "id",             "streamer" },
                { "source",         "" },
                { "stream-select",  StreamSelect([](Client client) -> mx {
                    bool test = false;
                    if (test) {
                        auto video  = H264FileParser("h264", 30, true);
                        //auto audio  = OPUSFileParser("opus", true);
                        auto stream = Stream(video, {});
                        return stream;
                    }
                    ///
                    return App([](App &ctx) -> node {
                        return View {
                            { "id",      "main" },
                            { "sample",  int(2) },
                            { "sample2", "10"   },
                            { "on-hover", callback([](event e) {
                                console.log("hi");
                            }) },
                            { "clicked", callback([](event e) {
                                printf("test!\n");
                            }) }
                        };
                    });
                })}
            },

            /// https resource server
            Service {{ "id",   "https"   },
                     { "url",  https_res },
                     { "root", "www"     },
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
}