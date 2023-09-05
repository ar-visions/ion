#pragma once
#include <net/net.hpp>
#include <rtc/rtc.hpp>
#include <webrtc/streamer/stream.hpp> // has client data

#include <composer/composer.hpp>

//using namespace std::chrono_literals;
using std::shared_ptr;
using std::weak_ptr;
template <class T> weak_ptr<T> make_weak_ptr(shared_ptr<T> ptr) { return ptr; }


/// pre-compiled headers are good place to put in shimmable utility declaration and implementation
#ifdef _WIN32
#include <winsock2.h> /// must do this if you include windows
#include <windows.h>

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);

#else
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#endif


namespace webrtc {
/// webrtc -> service
ion::async service(ion::uri uri, ion::lambda<ion::message(ion::message)> fn_process);
}

namespace ion {

using VideoSink = lambda<void(mx)>;

struct Service: node {
	struct props {
		ion::async 				  service; /// not exposed in meta; private state var
		uri 					  url;
		lambda<message(message&)> on_message;
		///
		type_register(props);
		///
		doubly<prop> meta() {
			return {
				prop { "url", url },
				prop { "on-message", on_message }
			};
		}
	};

    void mounted();

	component(Service, node, props);
};

/// these should be read by the context api; although, the parent chain will not go this far back (nor should it?)
/// todo: set composer as parent to first node!
/// todo: scratch that, not type safe.
struct Services:composer {
    struct sdata {
        bool                    running;
        lambda<node(Services&)> service_fn;
		VideoSink				video_sink;
		///
		doubly<prop> meta() {
			return {
				prop { "video_sink", video_sink }
			};
		}
		///
        type_register(sdata);
    };
    mx_object(Services, composer, sdata);

    Services(lambda<node(Services&)> service_fn) : Services() {
        data->service_fn = service_fn;
    }

    /// wait for all services to return
    operator int() {
        return run();
    }

    int run();
};

/// this should look for a video sink
struct VideoStream: node {
	struct props {
		/// arg|css bound states
		uri   source;  /// url could be a vulkan device?
		
		/// internal states
		async service; /// if props are changed, the service must be restarted

        lambda<std::shared_ptr<webrtc::ClientTrackData>(
            std::shared_ptr<rtc::impl::PeerConnection> pc, const uint8_t payloadType,
            uint32_t ssrc, std::string cname, std::string msid,
            lambda<void(void)>)> addVideo;

        lambda<std::shared_ptr<webrtc::ClientTrackData>(
            shared_ptr<rtc::impl::PeerConnection>, uint8_t, uint32_t, std::string, std::string, lambda<void (void)>
        )> addAudio;

        lambda<std::shared_ptr<webrtc::Client>
            (rtc::Configuration&, weak_ptr<rtc::WebSocket>, std::string)>
                createPeerConnection;

        lambda<void(void)> startStream;

        lambda<void(var, rtc::Configuration, std::shared_ptr<rtc::WebSocket>)> wsOnMessage;

		//state->stream = createStream(const string h264Samples, const unsigned fps, const string opusSamples)
		lambda<std::shared_ptr<webrtc::Stream>(std::string, unsigned, std::string)> createStream;

		doubly<prop> meta() {
			return {
				prop { "source", source }
			};
		}
		type_register(props);
	};

	component(VideoStream, node, props);
	
	void mounted();
};

/// a node-controlled service that we express with for-each
/// do this!
struct Streamer: node {
	struct props {
		uri   source; /// can a uri be a type and properties, or instance of one?
		async service;

		// yes i want it set here and that is better to express pipeline such as this, and others
		//doubly<Client> clients;
		std::unordered_map<std::string, shared_ptr<webrtc::Client>> clients;

        struct events {
			// (var message, Configuration config, shared_ptr<WebSocket> ws)

            dispatch on_enter;
			dispatch on_leave;
			dispatch on_stats; /// from here the user can request context
        } ev;

		doubly<prop> meta() {
			return {
				prop { "on-enter", ev.on_enter },
				prop { "on-leave", ev.on_leave },
				prop { "on-stats", ev.on_stats }
			};
		}

		type_register(props);
	};

	void mounted() {
		console.log("abc");
	}
};

}
