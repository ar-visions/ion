#pragma once
#include <net/net.hpp>
#include <rtc/rtc.hpp>
#include <composer/composer.hpp>

//using namespace std::chrono_literals;
using std::shared_ptr;
using std::weak_ptr;
template <class T> weak_ptr<T> make_weak_ptr(shared_ptr<T> ptr) { return ptr; }

namespace webrtc {
/// webrtc -> service
ion::async service(ion::uri uri, ion::lambda<ion::message(ion::message)> fn_process);
};

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

/// we need webrtc to have access to 'services' generic; for that we would call it to start and stop the service
/// async needs a signaling method for the stop case.

/// these are read by introspection via prop/type lookup (important to have both)

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
		uri   source;  /// url could be a vulkan device?
		async service; /// if props are changed, the service must be restarted
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

}
