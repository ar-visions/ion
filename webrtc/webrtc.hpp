#pragma once
#include <net/net.hpp>
#include <rtc/rtc.hpp>
#include <webrtc/streamer/dispatchqueue.hpp>

namespace webrtc {
struct ClientTrackData {
    std::shared_ptr<rtc::Track> track;
    std::shared_ptr<rtc::RtcpSrReporter> sender;

    ClientTrackData(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::RtcpSrReporter> sender);
};

struct Stream;

struct Client {
    enum class State {
        Waiting,
        WaitingForVideo,
        WaitingForAudio,
        Ready
    };
    const std::shared_ptr<rtc::PeerConnection> & peerConnection = _peerConnection;
    Client(std::shared_ptr<rtc::PeerConnection> pc) {
        _peerConnection = pc;
    }
    std::optional<std::shared_ptr<ClientTrackData>> video;
    std::optional<std::shared_ptr<ClientTrackData>> audio;
    std::optional<std::shared_ptr<rtc::DataChannel>> dataChannel;

    void setState(State state);
    State getState();

    uint32_t rtpStartTimestamp = 0;
    Stream* stream;
    State state = State::Waiting;
    std::string id;
    std::shared_mutex _mutex;
    std::shared_ptr<rtc::PeerConnection> _peerConnection;
};

struct ClientTrack {
    std::string id;
    std::shared_ptr<ClientTrackData> trackData;
    ClientTrack(std::string id, std::shared_ptr<ClientTrackData> trackData);
};

uint64_t currentTimeInMicroSeconds();

class StreamSource {
protected:

public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void loadNextSample() = 0;

    virtual uint64_t getSampleTime_us() = 0;
    virtual uint64_t getSampleDuration_us() = 0;
	virtual rtc::binary getSample() = 0;
};

class Stream: std::enable_shared_from_this<Stream> {
    uint64_t startTime = 0;
    std::mutex mutex;
    DispatchQueue dispatchQueue = DispatchQueue("StreamQueue");

    //ion::lambda<mx()> read; /// use this instead of the file parsing, by letting file parser become user of this
    bool _isRunning = false;
public:
    const std::shared_ptr<StreamSource> audio;
    const std::shared_ptr<StreamSource> video;

    Stream(std::shared_ptr<StreamSource> video, std::shared_ptr<StreamSource> audio);
    ~Stream();

    enum class StreamSourceType {
        Audio,
        Video
    };

private:
    rtc::synchronized_callback<StreamSourceType, uint64_t, rtc::binary> sampleHandler;

    std::pair<std::shared_ptr<StreamSource>, StreamSourceType> unsafePrepareForSample();

    void sendSample();

public:
    void onSample(std::function<void (StreamSourceType, uint64_t, rtc::binary)> handler);
    void start();
    void stop();
    const bool & isRunning = _isRunning;
};
}

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

/// webrtc, rtc, rtc::impl -> rtc (the impl's are laid out in modules in ways i wouldnt design)
/// to better understand the protocols i will organize them in simpler ways

using StreamPtr = shared_ptr<webrtc::Stream>;

using StreamSelect = lambda<shared_ptr<webrtc::Stream>(shared_ptr<webrtc::Client>)>;

/// this should look for a video sink
struct VideoStream: node {
	struct props {
		/// arg|css bound states
		uri   source;  /// url could be a vulkan device?
		
		/// will need to explain this thing
		std::unordered_map<std::string, shared_ptr<webrtc::Client>> clients;

        doubly<shared_ptr<webrtc::Stream>> avStreams;

		/// internal states
		async service; /// if props are changed, the service must be restarted

		/// ideally we want to express this in node tree!
		/// this is a foothold into a graple on this

        lambda<std::shared_ptr<webrtc::ClientTrackData>(
            std::shared_ptr<rtc::PeerConnection> pc, const uint8_t payloadType,
            uint32_t ssrc, std::string cname, std::string msid,
            lambda<void(void)>)> addVideo;

        lambda<std::shared_ptr<webrtc::ClientTrackData>(
            shared_ptr<rtc::PeerConnection>, uint8_t, uint32_t, std::string, std::string, lambda<void (void)>
        )> addAudio;

        lambda<std::shared_ptr<webrtc::Client>
            (rtc::Configuration&, weak_ptr<rtc::WebSocket>, std::string)>
                createPeerConnection;

        lambda<void(shared_ptr<webrtc::Client>)> startStream;

        lambda<void(var, rtc::Configuration, std::shared_ptr<rtc::WebSocket>)> wsOnMessage;

		//state->stream = createStream(const string h264Samples, const unsigned fps, const string opusSamples)
		lambda<std::shared_ptr<webrtc::Stream>(std::string, unsigned, std::string)> createStream;

		lambda<void(shared_ptr<webrtc::Stream>, std::shared_ptr<webrtc::ClientTrackData> video)> sendInitialNalus;

		lambda<void(shared_ptr<webrtc::Client>, bool)> addToStream;

        /// make mx objects out of webrtc client and stream
        StreamSelect stream_select; 

        lambda<int(shared_ptr<webrtc::Stream>)> client_count;

		shared_ptr<webrtc::Stream> avStream = null;

		webrtc::DispatchQueue *MainThread;

		doubly<prop> meta() {
			return {
				prop { "source", source },
                prop { "stream-select", stream_select }
			};
		}
		type_register(props);
	};

	component(VideoStream, node, props);
	
	void mounted();
};

}
