#include <webrtc/webrtc.hpp>
#include <media/media.hpp>
#include <ux/ux.hpp>

#ifdef _WIN32
#include <webrtc/win/capture.h>
#endif

#include <webrtc/streamer/stream.hpp>
#include <webrtc/rtc/peerconnection.hpp>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <string>
#include <vector>
#include <shared_mutex>
#include <optional>
#include <fstream>
#include <ctime>
#include <shared_mutex>
/// 
#include <webrtc/streamer/fileparser.hpp>
#include <webrtc/streamer/h264fileparser.hpp>
#include <webrtc/streamer/opusfileparser.hpp>

#ifdef _WIN32
int gettimeofday(struct timeval *tv, struct timezone *tz) {
	if (tv) {
		FILETIME filetime; /* 64bit: 100-nanosecond intervals since January 1, 1601 00:00 UTC */
		ULARGE_INTEGER x;
		ULONGLONG usec;
		static const ULONGLONG epoch_offset_us =
		    11644473600000000ULL; /* microseconds betweeen Jan 1,1601 and Jan 1,1970 */

#if _WIN32_WINNT >= _WIN32_WINNT_WIN8
		GetSystemTimePreciseAsFileTime(&filetime);
#else
		GetSystemTimeAsFileTime(&filetime);
#endif
		x.LowPart = filetime.dwLowDateTime;
		x.HighPart = filetime.dwHighDateTime;
		usec = x.QuadPart / 10 - epoch_offset_us;
		tv->tv_sec = time_t(usec / 1000000ULL);
		tv->tv_usec = long(usec % 1000000ULL);
	}
	if (tz) {
		TIME_ZONE_INFORMATION timezone;
		GetTimeZoneInformation(&timezone);
		tz->tz_minuteswest = timezone.Bias;
		tz->tz_dsttime = 0;
	}
	return 0;
}
#endif

using namespace ion;
using namespace std;
using namespace rtc;

/// service is a better name for it because its data as well as video.  its any stream
namespace ion {

void FileParser::init(string directory, string extension, uint32_t samplesPerSecond, bool loop) {
    data->directory         = directory;
    data->extension         = extension;
    data->loop              = loop;
    data->sampleDuration_us = 1000 * 1000 / samplesPerSecond;
    Source::intern     *base = Source::data;
    FileParser::intern *fp   = FileParser::data;

    base->getSampleTime_us = [fp](StreamType type) {
        return fp->sampleTime_us;
    };

    base->getSampleDuration_us = [fp](StreamType type) {
        return fp->sampleDuration_us;
    };

    base->start = [base, fp](StreamType type) {
        fp->sampleTime_us = std::numeric_limits<uint64_t>::max() - fp->sampleDuration_us + 1;
        base->loadNextSample(type);
    };

    base->stop = [base, fp](StreamType type) {
        fp->sample = {};
        fp->sampleTime_us = 0;
        fp->counter = -1;
    };

    base->getSample = [fp](StreamType type) {
        return fp->sample;
    };

    base->loadNextSample = [base, fp](StreamType type) {
        string frame_id = std::to_string(++fp->counter);
        string url = fp->directory + "/sample-" + frame_id + fp->extension;
        ifstream source(url, ios_base::binary);
        if (!source) {
            if (fp->loop && fp->counter > 0) {
                fp->loopTimestampOffset = fp->sampleTime_us;
                fp->counter = -1;
                base->loadNextSample(type);
                return;
            }
            fp->sample = {};
            return;
        }
        vector<uint8_t> fileContents((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
        fp->sample = *reinterpret_cast<vector<byte> *>(&fileContents);
        fp->sampleTime_us += fp->sampleDuration_us;
    };
}

FileParser::FileParser(string directory, string extension, uint32_t samplesPerSecond, bool loop) : FileParser() {
    init(directory, extension, samplesPerSecond, loop);
}


H264FileParser::H264FileParser(string directory, uint32_t fps, bool loop): H264FileParser() {
    FileParser::init(directory, ".h264", fps, loop);
    auto base =         Source::data;
    auto fp   =     FileParser::data;
    auto h264 = H264FileParser::data;

    lambda<void()> base_load = base->loadNextSample;

    base->loadNextSample = [fp, base, h264, base_load](StreamType stype) { // overwrite lambda, and stack base method
        base_load();
        size_t i = 0;
        while (i < fp->sample.size()) {
            assert(i + 4 < fp->sample.size());
            auto lengthPtr = (uint32_t *) (fp->sample.data() + i);
            uint32_t length = ntohl(*lengthPtr);
            auto naluStartIndex = i + 4;
            auto naluEndIndex = naluStartIndex + length;
            assert(naluEndIndex <= fp->sample.size());
            auto header = reinterpret_cast<rtc::NalUnitHeader *>(fp->sample.data() + naluStartIndex);
            auto type = header->unitType();
            switch (type) {
                case 7:
                    h264->previousUnitType7 = {fp->sample.begin() + i, fp->sample.begin() + naluEndIndex};
                    break;
                case 8:
                    h264->previousUnitType8 = {fp->sample.begin() + i, fp->sample.begin() + naluEndIndex};
                    break;
                case 5:
                    h264->previousUnitType5 = {fp->sample.begin() + i, fp->sample.begin() + naluEndIndex};
                    break;
            }
            i = naluEndIndex;
        }
    };
}


OPUSFileParser::OPUSFileParser(string directory, bool loop, uint32_t samplesPerSecond): FileParser(directory, ".opus", samplesPerSecond, loop) { }

}


namespace webrtc {
DispatchQueue::DispatchQueue(std::string name, size_t threadCount) :
    name{std::move(name)}, threads(threadCount) {
    for(size_t i = 0; i < threads.size(); i++)
    {
        threads[i] = std::thread(&DispatchQueue::dispatchThreadHandler, this);
    }
}

DispatchQueue::~DispatchQueue() {
    // Signal to dispatch threads that it's time to wrap up
    std::unique_lock<std::mutex> lock(lockMutex);
    quit = true;
    lock.unlock();
    condition.notify_all();

    // Wait for threads to finish before we exit
    for(size_t i = 0; i < threads.size(); i++)
    {
        if(threads[i].joinable())
        {
            threads[i].join();
        }
    }
}

void DispatchQueue::removePending() {
    std::unique_lock<std::mutex> lock(lockMutex);
    queue = {};
}

void DispatchQueue::dispatch(const fp_t& op) {
    std::unique_lock<std::mutex> lock(lockMutex);
    queue.push(op);

    // Manual unlocking is done before notifying, to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
    lock.unlock();
    condition.notify_one();
}

void DispatchQueue::dispatch(fp_t&& op) {
    std::unique_lock<std::mutex> lock(lockMutex);
    queue.push(std::move(op));

    // Manual unlocking is done before notifying, to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
    lock.unlock();
    condition.notify_one();
}

void DispatchQueue::dispatchThreadHandler(void) {
    std::unique_lock<std::mutex> lock(lockMutex);
    do {
        //Wait until we have data or a quit signal
        auto *a = this;
        condition.wait(lock, [a]() -> bool {
            return (a->queue.size() || a->quit);
        });

        //after wait, we own the lock
        if(!quit && queue.size())
        {
            auto op = std::move(queue.front());
            queue.pop();

            //unlock now that we're done messing with the queue
            lock.unlock();

            op();

            lock.lock();
        }
    } while (!quit);
}

}

namespace ion {

ClientTrackData::ClientTrackData(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::RtcpSrReporter> sender) {
	this->track = track;
	this->sender = sender;
}

ClientTrack::ClientTrack(string id, shared_ptr<ClientTrackData> trackData) {
	this->id = id;
	this->trackData = trackData;
}

/// use Capture directly here

Stream app_stream(App app) {
    Stream stream;
    stream->has_data = true;
    bool init = false;
    ///
    /// output frame rate
    async { 1, [&init, stream, _app=app](runtime* rt, int index) -> mx {
        App &app = (App&)_app;
        
        /// set this data on the app stream as TEST
        /// todo: must encode pcm as opus direct from global pcm; prefer app-specific but not possible currently
        
        bool              close = false;
        Stream::iStream  *base  = stream.get<Stream::iStream>(0);
        base->audio = {}; // OPUSFileParser("opus", true);

        Source::iSource  *vsrc  = base->video.get<Source::iSource>(0);
        Source::iSource  *asrc  = base->audio.get<Source::iSource>(0); /// these are 'optional' still, not sure how i am getting this handle
        std::vector<byte> sample[2];
        uint64_t        time[2] = { };
        mutex             mtx;

        time[1] = time[0];
        sample[0] = {};
        sample[1] = {};

        auto get_time = [&](StreamType type) -> uint64_t {
            mtx.lock();
            uint64_t t = time[type.value];
            return t;
        };

        Capture *capture = null;

        vsrc->getSampleTime_us      = [&](StreamType type) -> uint64_t { return time[type.value]; };
        vsrc->getSampleDuration_us  = [ ](StreamType type) -> uint64_t { return 1000 * 1000 / 60; };
        vsrc->start                 = [&](StreamType type) { vsrc->loadNextSample(type); };
        vsrc->stop                  = [&](StreamType type) { close = true; };

        ///
        vsrc->getSample = [&](StreamType type) {
            int i = type.value;
            mtx.lock();
            std::vector<byte> s { sample[i] }; /// this one is a bit odd and i dont quite like the bit copy
            sample[i] = {};
            mtx.unlock();
            return s;
        };

        vsrc->loadNextSample = [&](StreamType type) { /// waits for frame loop, h264 push, h264 fetch and subsequent data
            uint64_t t = get_time(type);
            int i = type.value;
            mtx.unlock();
            for (;;) {
                mtx.lock();
                if (t != time[i] || close) {
                    mtx.unlock();
                    break;
                }
                mtx.unlock();
                usleep(1000);
            }
        };

        /*
        testing with opus data from RR
        asrc->getSampleTime_us      = vsrc->getSampleTime_us;
        asrc->getSampleDuration_us  = vsrc->getSampleDuration_us;
        asrc->getSample             = vsrc->getSample;
        asrc->start                 = vsrc->start;
        asrc->stop                  = vsrc->stop;
        asrc->loadNextSample        = vsrc->loadNextSample;
        */

        /// register app loop
        ((App&)app)->loop_fn = [&](App &app) -> bool {
            if (close)
                return false;

            /// start capture when window is first seen
            if (!capture) {
                capture = new Capture { (GLFWwindow*)app->e->window,
                    Capture::OnData([&](u64 frame_time, mx data) -> bool {
                        if (close)
                            return false; /// network session in sync with encoding
                        
                        /// protected copy to stream sources
                        mtx.lock();
                        int  v  = StreamType::Video;
                        time[v] = frame_time - base->startTime;
                        time[0] = time[v];
                        u8 *bytes  = data.get<u8>(0);
                        size_t len = data.count();
                        assert(bytes && len);
                        sample[v] = std::vector<std::byte>(len);
                        memcpy((u8*)sample[v].data(), (u8*)bytes, len);
                        sample[v].resize(len); /// this sets 'size', the reserve size is separate stat; will not change data if its <= len
                        mtx.unlock();

                        /// sync in audio right here.
                        /// WSAPIAPSIAPISAPI should work.
                        return true;
                    }),
                    Capture::OnClosed([](iCapture *handle) -> void {
                        console.log("capture closed");
                    })
                };
            }
            
            return true;
        };

        init = true;
        return app.run();
    }};

    while (!init) {
        usleep(1000);
    }
    return stream;
}

/// contextual class instances are safely allocated until the data is not;
/// we dont mount and then free the context leaving the data, thats possible but we leave the context open with data
void VideoStream::mounted() {
    using PeerConnection = rtc::PeerConnection;
    using Configuration  = rtc::Configuration;
	//VideoSink *video_sink = context<VideoSink>("video_sink"); /// send to nearest video sink, we think.
	//assert(video_sink);

    VideoStream::props *state = this->state;

    /// all props are set prior to mount call (args & style)
	state->service = async {1, [state](runtime *proc, int i) -> mx {
        /// Main dispatch queue (this is main for webrtc module, its own abstract)
        /// use async method
        state->MainThread = new webrtc::DispatchQueue("Main");
        
        state->addVideo = [](shared_ptr<PeerConnection> pc, uint8_t payloadType, H264RtpPacketizer::Separator frame_std, uint32_t ssrc, string cname, string msid, function<void (void)> onOpen) {
            auto video = Description::Video(cname);
            video.addH264Codec(payloadType);
            video.addSSRC(ssrc, cname, msid, cname);
            auto track = pc->addTrack(video);
            // create RTP configuration
            auto rtpConfig = make_shared<RtpPacketizationConfig>(ssrc, cname, payloadType, H264RtpPacketizer::defaultClockRate);

            /// NAL unit separator
            //enum class Separator {
            //    Length = RTC_NAL_SEPARATOR_LENGTH, // first 4 bytes are NAL unit length
            //    LongStartSequence = RTC_NAL_SEPARATOR_LONG_START_SEQUENCE,   // 0x00, 0x00, 0x00, 0x01
            //    ShortStartSequence = RTC_NAL_SEPARATOR_SHORT_START_SEQUENCE, // 0x00, 0x00, 0x01
            //    StartSequence = RTC_NAL_SEPARATOR_START_SEQUENCE, // LongStartSequence or ShortStartSequence
            //};

            // create packetizer (we cannot adapt based on the data?)
            auto packetizer = make_shared<H264RtpPacketizer>(frame_std, rtpConfig);
            // create H264 handler
            auto h264Handler = make_shared<H264PacketizationHandler>(packetizer);
            // add RTCP SR handler
            auto srReporter = make_shared<RtcpSrReporter>(rtpConfig);
            h264Handler->addToChain(srReporter);
            // add RTCP NACK handler
            auto nackResponder = make_shared<RtcpNackResponder>();
            h264Handler->addToChain(nackResponder);
            // set handler
            track->setMediaHandler(h264Handler);
            track->onOpen(onOpen);
            auto trackData = make_shared<ClientTrackData>(track, srReporter);
            return trackData;
        };

        state->addAudio = [](shared_ptr<PeerConnection> pc, uint8_t payloadType, uint32_t ssrc, string cname, string msid, function<void (void)> onOpen) {
            auto audio = Description::Audio(cname);
            audio.addOpusCodec(payloadType);
            audio.addSSRC(ssrc, cname, msid, cname);
            auto track = pc->addTrack(audio);
            // create RTP configuration
            auto rtpConfig = make_shared<RtpPacketizationConfig>(ssrc, cname, payloadType, OpusRtpPacketizer::defaultClockRate);
            // create packetizer
            auto packetizer = make_shared<OpusRtpPacketizer>(rtpConfig);
            // create opus handler
            auto opusHandler = make_shared<OpusPacketizationHandler>(packetizer);
            // add RTCP SR handler
            auto srReporter = make_shared<RtcpSrReporter>(rtpConfig);
            opusHandler->addToChain(srReporter);
            // add RTCP NACK handler
            auto nackResponder = make_shared<RtcpNackResponder>();
            opusHandler->addToChain(nackResponder);
            // set handler
            track->setMediaHandler(opusHandler);
            track->onOpen(onOpen);
            auto trackData = make_shared<ClientTrackData>(track, srReporter);
            return trackData;
        };

        // Create and setup a PeerConnection
        state->createPeerConnection = [state](Configuration &config, weak_ptr<WebSocket> wws, str id) -> Client {
            auto pc = make_shared<PeerConnection>(config);
            auto client = Client(pc, id);

            pc->onStateChange([state, id](PeerConnection::State peer) {
                cout << "State: " << int(peer) << endl;
                if (peer == PeerConnection::State::Disconnected ||
                    peer == PeerConnection::State::Failed ||
                    peer == PeerConnection::State::Closed) {
                    // remove disconnected client
                    state->MainThread->dispatch([state, id]() {
                        state->clients->remove((mx&)id);
                    });
                }
            });

            pc->onGatheringStateChange(
                [wpc = make_weak_ptr(pc), id, wws](PeerConnection::GatheringState peer) {
                std::cout << "Gathering State: " << int(peer) << endl;
                if (peer == PeerConnection::GatheringState::Complete) {
                    if(auto pc = wpc.lock()) {
                        auto description = pc->localDescription();
                        var message = ion::map<mx> {
                            {"id", id},
                            {"type", description->typeString()},
                            {"sdp", string(description.value())}
                        };
                        // Gathering complete, send answer
                        if (auto ws = wws.lock()) {
                            str s_msg = message.stringify();
                            ws->send(s_msg);
                        }
                    }
                }
            });

            bool use_annex_B = true;

            client->video = state->addVideo(pc, 102,
                use_annex_B ? rtc::H264RtpPacketizer::Separator::LongStartSequence :
                              rtc::H264RtpPacketizer::Separator::Length,
                1, "video-stream", "stream1", [state, id, client]() {
                state->MainThread->dispatch([state, client]() {
                    state->addToStream(client, true);
                });
                cout << "Video from " << id << " opened" << endl;
            });
            /*
            client->audio = state->addAudio(pc, 111, 2, "audio-stream", "stream1", [state, id, client]() {
                state->MainThread->dispatch([state, client]() {
                    state->addToStream(client, false);
                });
                cout << "Audio from " << id << " opened" << endl;
            });
            */
            auto dc = pc->createDataChannel("ping-pong");
            dc->onOpen([id, wdc = make_weak_ptr(dc)]() {
                if (auto dc = wdc.lock()) {
                    dc->send("Ping");
                }
            });

            dc->onMessage(nullptr, [id, wdc = make_weak_ptr(dc)](string msg) {
                cout << "Message from " << id << " received: " << msg << endl;
                if (auto dc = wdc.lock()) {
                    dc->send("Ping");
                }
            });
            client->dataChannel = dc;

            pc->setLocalDescription();
            return client;
        };

        /// take the Client and any negotiated uri channel into account
        state->startStream = [state](Client client) {
            /// this is a generic mx call, we hold the memory
            mx   istream = state->stream_select(client).hold();
            if (istream.type() == typeof(null_t))
                return;

            Stream stream;
            type_t type = istream.type();
            if (type == typeof(Stream))
                stream = istream.hold();
            else if (type == typeof(App)) {
                stream = app_stream(istream.hold());
            }

            client->stream = stream.mem; // simply weak reference

            // set callback responsible for sample sending
            stream->onSample(
                    [state, stream](StreamType type, uint64_t sampleTime, rtc::binary sample) {
                vector<ClientTrack> tracks{};
                string streamType = type == StreamType::Video ? "video" : "audio";
                // get track for given type
                function<optional<shared_ptr<ClientTrackData>> (Client)> getTrackData = [type](Client client) {
                    return type.value == StreamType::Video ? client->video : client->audio;
                };
                // get all clients with Ready state
                // this needs to go only to the clients with this stream attached
                // makes sense to go have stream -> clients
                for(field<Client> &id_client: state->clients) {
                    str id         = id_client.key.hold();
                    Client &client = id_client.value;
                    auto optTrackData = getTrackData(client);
                    if (client->state == Client::State::Ready && optTrackData.has_value()) {
                        auto trackData = optTrackData.value();
                        tracks.push_back(ClientTrack(id, trackData));
                    }
                }
                if (!tracks.empty()) {
                    for (auto clientTrack: tracks) {
                        auto client = clientTrack.id;
                        auto trackData = clientTrack.trackData;
                        auto rtpConfig = trackData->sender->rtpConfig;

                        // sample time is in us, we need to convert it to seconds
                        auto elapsedSeconds = double(sampleTime) / (1000 * 1000);
                        // get elapsed time in clock rate
                        uint32_t elapsedTimestamp = rtpConfig->secondsToTimestamp(elapsedSeconds);
                        // set new timestamp
                        rtpConfig->timestamp = rtpConfig->startTimestamp + elapsedTimestamp;

                        // get elapsed time in clock rate from last RTCP sender report
                        auto reportElapsedTimestamp = rtpConfig->timestamp - trackData->sender->lastReportedTimestamp();
                        // check if last report was at least 1 second ago
                        if (rtpConfig->timestampToSeconds(reportElapsedTimestamp) > 1) {
                            trackData->sender->setNeedsToReport();
                        }

                        cout << "Sending " << streamType << " sample with size: " << std::to_string(sample.size()) << " to " << client << endl;
                        try {
                            // send sample
                            trackData->track->send(sample);
                        } catch (const std::exception &e) {
                            cerr << "Unable to send "<< streamType << " packet: " << e.what() << endl;
                        }
                    }
                }

                /// should be able to count them more direct by means of client->stream
                state->MainThread->dispatch([state, stream]() {
                    if (state->client_count(stream) == 0) {
                        ((Stream &)stream)->stop();
                    }
                });
            });


            bool found = false;
            for (auto &s: state->avStreams) {
                if (stream.mem == s.mem) {
                    found = true;
                    break;
                }
            }
            if (!found)
                state->avStreams->push(stream); /// we need to deal with this single instance
            if (!stream->_isRunning)
                stream->start();
        };

        state->addToStream = [state](Client client, bool isAddingVideo) {
            if (client->state == Client::State::Waiting) {
                client->state = isAddingVideo ? Client::State::WaitingForAudio : Client::State::WaitingForVideo;
            //} else if ((client->state == Client::State::WaitingForAudio && !isAddingVideo)
            //        || (client->state == Client::State::WaitingForVideo && isAddingVideo)) {

                // Audio and video tracks are collected now
                //assert(client->video.has_value() && client->audio.has_value());

                //auto video = client->video.value();
                /// this is Optional and not advised here because we havent called the stream_select
                //if (state->avStream) {
                //    state->sendInitialNalus(state->avStream, video);
                //}

                client->state = Client::State::Ready;
            }
            
            if (client->state == Client::State::Ready) {
                state->startStream(client);
            }
        };

        state->client_count = [state](Stream stream) -> int {
            int r = 0;
            memory *mem = stream.mem;
            for (field<Client> &field: state->clients)
                if (field.value->stream == mem)
                    r++;
            
            return r;
        };

		state->wsOnMessage = [state](var message, Configuration config, shared_ptr<WebSocket> ws) {
			mx *it = message.get("id");
			if (!it) return;
			str id = it->hold();
			it = message.get("type");
			if (!it) return;
			str type = it->hold();

			if (type == "request") {
				state->clients[id] = state->createPeerConnection(config, make_weak_ptr(ws), id);
			} else if (type == "answer") {
                auto f = state->clients->lookup(id);
                if (f) {
                    Client &client = f->value;
					str sdp = message["sdp"];
					auto description = Description(sdp, type);
					client->peerConnection->setRemoteDescription(description);
				}
			}
		};

		/// create web socket for connection to relay service
		auto ws = make_shared<WebSocket>();
		Configuration config;
		string stunServer = "stun:stun.l.google.com:19302";
		cout << "STUN server is " << stunServer << endl;
		config.iceServers.emplace_back(stunServer);
		config.disableAutoNegotiation = true;
		string localId = "server";
		cout << "The local ID is: " << localId << endl;

		ws->onOpen   ([ ]() {
			cout << "WebSocket connected, signaling ready" << endl;
		});

		ws->onClosed ([ ]() {
			cout << "WebSocket closed" << endl;
		});

		ws->onError  ([ ](const string &error) {
			cout << "WebSocket failed: " << error << endl;
		});

		ws->onMessage([state, config, ws](variant<binary, string> data) {
			if (!holds_alternative<string>(data))
				return;
			string sdata = std::get<std::string>(data);
			var message = var::parse((cstr)sdata.c_str());
			state->MainThread->dispatch([state, message, config, ws]() {
				state->wsOnMessage(message, config, ws);
			});
		});

        std::string ws_signal = "ws://127.0.0.1:8000/server";
		ws->open(ws_signal);
		while (!ws->isOpen()) {
			if (ws->isClosed())
				return false;
			this_thread::sleep_for(100ms);
		}

		while (ws->isOpen())
			this_thread::sleep_for(100ms);
		
		return true;
	}};
}
}
