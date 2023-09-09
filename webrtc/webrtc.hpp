#pragma once
#include <net/net.hpp>
#include <composer/composer.hpp>
#include <rtc/rtc.hpp>
#include <webrtc/streamer/dispatchqueue.hpp>

namespace ion {

struct ClientTrackData {
    std::shared_ptr<rtc::Track> track;
    std::shared_ptr<rtc::RtcpSrReporter> sender;
    ClientTrackData(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::RtcpSrReporter> sender);
};

struct Stream;

struct Client:mx {
    enum State {
        Waiting,
        WaitingForVideo,
        WaitingForAudio,
        Ready
    };

    struct iClient {
        memory*     stream;
        State       state = State::Waiting;
        str         id;
        uint32_t    rtpStartTimestamp = 0;

        std::shared_mutex _mutex;
        std::optional<std::shared_ptr<ClientTrackData>> video;
        std::optional<std::shared_ptr<ClientTrackData>> audio;
        std::optional<std::shared_ptr<rtc::DataChannel>> dataChannel;
        std::shared_ptr<rtc::PeerConnection> peerConnection;

        type_register(iClient);
    };

    mx_object(Client, mx, iClient);

    Client(std::shared_ptr<rtc::PeerConnection> pc, str id) : Client() {
        data->peerConnection = pc;
        data->id = id;
    }
};

struct ClientTrack {
    std::string id;
    std::shared_ptr<ClientTrackData> trackData;
    ClientTrack(std::string id, std::shared_ptr<ClientTrackData> trackData);
};

uint64_t currentTimeInMicroSeconds();

enums(StreamType, Audio,
        "Audio, Video",
         Audio, Video);

struct Source:mx {
    
    struct iSource {
        StreamType              type;
        lambda<uint64_t()>      getSampleTime_us;
        lambda<uint64_t()>      getSampleDuration_us;
        lambda<rtc::binary()>   getSample;
        lambda<void()>          loadNextSample;
        lambda<void()>          start;
        lambda<void()>          stop;
        type_register(iSource);
    };

    mx_object(Source, mx, iSource);
};

struct Stream:mx {
    struct iStream {
        uint64_t startTime = 0;
        std::mutex mutex;
        webrtc::DispatchQueue dispatchQueue = webrtc::DispatchQueue("StreamQueue");
        //ion::lambda<mx()> read; /// use this instead of the file parsing, by letting file parser become user of this
        bool    _isRunning = false;
        bool    has_data;
        Source  audio;
        Source  video;
        rtc::synchronized_callback<StreamType, uint64_t, rtc::binary> sampleHandler;

        type_register(iStream);

        void onSample(std::function<void (StreamType, uint64_t, rtc::binary)> handler) {
            sampleHandler = handler;
        }

        void start() {
            std::lock_guard lock(mutex);
            if (_isRunning) {
                return;
            }
            _isRunning = true;
            startTime = currentTimeInMicroSeconds();
            audio->start();
            video->start();
            dispatchQueue.dispatch([this]() {
                this->sendSample();
            });
        }

        void stop() {
            std::lock_guard lock(mutex);
            if (!_isRunning) {
                return;
            }
            _isRunning = false;
            dispatchQueue.removePending();
            audio->stop();
            video->stop();
        }

        ~iStream() {
            stop();
        }

        std::pair<Source, StreamType> unsafePrepareForSample() {
            Source ss;
            StreamType sst;
            uint64_t nextTime;

            if (audio->getSampleTime_us() < video->getSampleTime_us()) {
                ss = audio;
                sst = StreamType::Audio;
                nextTime = audio->getSampleTime_us();
            } else {
                ss = video;
                sst = StreamType::Video;
                nextTime = video->getSampleTime_us();
            }

            auto currentTime = currentTimeInMicroSeconds();

            auto elapsed = currentTime - startTime;
            if (nextTime > elapsed) {
                auto waitTime = nextTime - elapsed;
                mutex.unlock();
                usleep(waitTime);
                mutex.lock();
            }
            return {ss, sst};
        }

        void sendSample() {
            std::lock_guard lock(mutex);
            if (!_isRunning) {
                return;
            }
            auto ssSST = unsafePrepareForSample();
            auto ss = ssSST.first;
            auto sst = ssSST.second;
            auto sample = ss->getSample();
            sampleHandler(sst, ss->getSampleTime_us(), sample);
            ss->loadNextSample();
            dispatchQueue.dispatch([this]() {
                this->sendSample();
            });
        }
    };

    mx_object(Stream, mx, iStream);

    Stream(Source video, Source audio) : Stream() {
        data->video = video;
        data->audio = audio;
        data->has_data = true;
    }

    operator bool() {
        return data->has_data;
    }

    bool operator!() {
        return !(operator bool());
    }
};



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

using StreamSelect = lambda<Stream(Client)>;

/// this should look for a video sink
struct VideoStream: node {
	struct props {
		/// arg|css bound states
		uri   source;  /// url could be a vulkan device?
		
		/// will need to explain this thing
		ion::map<Client> clients;

        doubly<Stream> avStreams;

		/// internal states
		async service; /// if props are changed, the service must be restarted

		/// ideally we want to express this in node tree!
		/// this is a foothold into a graple on this

        lambda<std::shared_ptr<ClientTrackData>(
            std::shared_ptr<rtc::PeerConnection> pc, const uint8_t payloadType,
            uint32_t ssrc, std::string cname, std::string msid,
            lambda<void(void)>)> addVideo;

        lambda<std::shared_ptr<ClientTrackData>(
            std::shared_ptr<rtc::PeerConnection>, uint8_t, uint32_t, std::string, std::string, lambda<void (void)>
        )> addAudio;

        lambda<Client
            (rtc::Configuration&, std::weak_ptr<rtc::WebSocket>, str)>
                createPeerConnection;

        lambda<void(Client)> startStream;

        lambda<void(var, rtc::Configuration, std::shared_ptr<rtc::WebSocket>)> wsOnMessage;

		//state->stream = createStream(const string h264Samples, const unsigned fps, const string opusSamples)
		lambda<Stream (std::string, unsigned, std::string)> createStream;

		lambda<void(Stream , std::shared_ptr<ClientTrackData> video)> sendInitialNalus;

		lambda<void(Client, bool)> addToStream;

        /// make mx objects out of webrtc client and stream
        StreamSelect stream_select; 

        lambda<int(Stream )> client_count;

		Stream avStream;

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


namespace ion {
ion::async service(ion::uri uri, ion::lambda<ion::message(ion::message)> fn_process);
}

