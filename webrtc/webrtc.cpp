#include <webrtc/webrtc.hpp>
#include <media/media.hpp>
#include <ux/ux.hpp>

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

    base->getSampleTime_us = [fp]() {
        return fp->sampleTime_us;
    };
    base->getSampleDuration_us = [fp]() {
        return fp->sampleDuration_us;
    };
    base->start = [base, fp]() {
        fp->sampleTime_us = std::numeric_limits<uint64_t>::max() - fp->sampleDuration_us + 1;
        base->loadNextSample();
    };
    base->stop = [base, fp]() {
        fp->sample = {};
        fp->sampleTime_us = 0;
        fp->counter = -1;
    };
    base->getSample = [fp]() {
        return fp->sample;
    };
    base->loadNextSample = [base, fp]() {
        string frame_id = std::to_string(++fp->counter);
        string url = fp->directory + "/sample-" + frame_id + fp->extension;
        ifstream source(url, ios_base::binary);
        if (!source) {
            if (fp->loop && fp->counter > 0) {
                fp->loopTimestampOffset = fp->sampleTime_us;
                fp->counter = -1;
                base->loadNextSample();
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

    base->loadNextSample = [fp, base, h264, base_load]() { // overwrite lambda, and stack base method
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


vector<byte> H264FileParser::initialNALUS() {
    vector<byte> units{};
    if (data->previousUnitType7.has_value()) {
        auto nalu = data->previousUnitType7.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    if (data->previousUnitType8.has_value()) {
        auto nalu = data->previousUnitType8.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    if (data->previousUnitType5.has_value()) {
        auto nalu = data->previousUnitType5.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    return units;
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

/// general service function to facilitate https or web sockets messages
ion::async service(uri url, lambda<message(message)> fn_process) {

    /// web socket protocol is a connector protocol unless a method of listen is explicitly called elsewhere
    if (url->proto == protocol::ws) {
        return ion::async {1, [url, fn_process](runtime *rt, int i) -> mx {
            console.log("async test");
			return null;
        }};
    }

    /// https is all we support for a listening service, no raw protocols without encryption in this stack per design
    if (url->proto == protocol::https)
        return sock::listen(url, [fn_process](sock &sc) -> bool {
            bool close = false;
            for (close = false; !close;) {
                close  = true;
                ///
                message request(sc);
                if (!request)
                    break;
                
                console.log("(https) {0} -> {1}", { request->query->mtype, request->query->query });

                message response(
                    fn_process(request)
                );
                response.write(sc);

                /// default is keep-alive on HTTP/1.1
                const char *F = "Connection";
                close = (request[F] == "close" && !response[F]) ||
                       (response[F] == "close");
            }
            return close;
        });

    console.fault("unsupported url: {0}", { url });
    return {};
};

ClientTrackData::ClientTrackData(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::RtcpSrReporter> sender) {
	this->track = track;
	this->sender = sender;
}

ClientTrack::ClientTrack(string id, shared_ptr<ClientTrackData> trackData) {
	this->id = id;
	this->trackData = trackData;
}

uint64_t currentTimeInMicroSeconds() {
	struct timeval time;
	gettimeofday(&time, NULL);
	return uint64_t(time.tv_sec) * 1000 * 1000 + time.tv_usec;
}



Stream app_stream(lambda<node(App&)> app_fn) {
    Stream stream;

    async { 1, [stream, app_fn](runtime* rt, int index) -> mx {
        App   app(app_fn);
        bool              close = false;
        Source::iSource  *base = stream.get<Source::iSource>(0);
        std::vector<byte> sample;
        mutex             mtx;
        uint64_t          time = currentTimeInMicroSeconds();

        // std::function<void (StreamType, uint64_t, rtc::binary)> handler
        h264e enc {[&](mx data) {
            mtx.lock();
            time = (time + 1000000/30);
            u8 *bytes = data.get<u8>(0);
            size_t len = data.count();
            assert(bytes && len);
            sample = std::vector<std::byte>(len);
            memcpy((u8*)sample.data(), (u8*)bytes, len);
            sample.resize(len);
            mtx.unlock();
            return true;
        }};

        base->getSampleTime_us      = [&]() -> uint64_t { return time; };
        base->getSampleDuration_us  = [ ]() -> uint64_t { return 1000000/30; };
        base->start                 = [&]() { base->loadNextSample(); };
        base->stop                  = [&]() { close = true; };

        base->getSample = [&]() {
            mtx.lock();
            std::vector<byte> s { sample }; /// this one is a bit odd and i dont quite like the bit copy
            sample = {};
            mtx.unlock();
            return s;
        };

        base->loadNextSample = [&]() { /// waits for frame loop, h264 push, h264 fetch and subsequent data
            mtx.lock();
            uint64_t t = time;
            mtx.unlock();
            for (;;) {
                mtx.lock();
                if (t != time || close) {
                    mtx.unlock();
                    break;
                }
                mtx.unlock();
                usleep(1000);
            }
        };

        /// start encoder
        enc.run();

        /// register app loop
        app->loop_fn = [&](App &app) -> bool {
            if (close)
                return false;

            image img { 32, 32 };
            enc.push(img);
            
            /// test pattern here i think.
            return true;
        };
        return app.run();
    }};
    return stream;
}


/// can update in real time 1/hz or through polling, but not needed at the moment
int Services::run() {
    node e = data->service_fn(*this);
    update_all(e);
    for (;;) {
        usleep(10000);
    }
    return 0;
}

void Service::mounted() {	
	state->service = ion::service(state->url, state->on_message);
}

/// contextual class instances are safely allocated until the data is not;
/// we dont mount and then free the context leaving the data, thats possible but we leave the context open with data
void VideoStream::mounted() {
	VideoSink *video_sink = context<VideoSink>("video_sink"); /// send to nearest video sink, we think.
	assert(video_sink);

    VideoStream::props *state = this->state;

    /// all props are set prior to mount call (args & style)
	state->service = async {1, [state, video_sink](runtime *proc, int i) -> mx {

        using PeerConnection = rtc::PeerConnection;
        using Configuration = rtc::Configuration;

        /// Main dispatch queue (this is main for webrtc module, its own abstract)
        /// use async method
        state->MainThread = new webrtc::DispatchQueue("Main");
        
        state->addVideo = [](shared_ptr<PeerConnection> pc, uint8_t payloadType, uint32_t ssrc, string cname, string msid, function<void (void)> onOpen) {
            auto video = Description::Video(cname);
            video.addH264Codec(payloadType);
            video.addSSRC(ssrc, cname, msid, cname);
            auto track = pc->addTrack(video);
            // create RTP configuration
            auto rtpConfig = make_shared<RtpPacketizationConfig>(ssrc, cname, payloadType, H264RtpPacketizer::defaultClockRate);
            // create packetizer
            auto packetizer = make_shared<H264RtpPacketizer>(H264RtpPacketizer::Separator::Length, rtpConfig);
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

        state->addAudio = [](shared_ptr<PeerConnection> pc, uint8_t payloadType, uint32_t ssrc, string cname, string msid, lambda<void (void)> onOpen) {
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

            client->video = state->addVideo(pc, 102, 1, "video-stream", "stream1", [state, id, client]() {
                state->MainThread->dispatch([state, client]() {
                    state->addToStream(client, true);
                });
                cout << "Video from " << id << " opened" << endl;
            });

            client->audio = state->addAudio(pc, 111, 2, "audio-stream", "stream1", [state, id, client]() {
                state->MainThread->dispatch([state, client]() {
                    state->addToStream(client, false);
                });
                cout << "Audio from " << id << " opened" << endl;
            });

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


        /// Start stream (single instance, needs to be improved upon lol)
        /// take the Client and any negotiated uri channel into account
        state->startStream = [state](Client client) {
            /// this is a generic mx call, we grab the memory
            mx   istream = state->stream_select(client).grab();//state->createStream("h264", 30, "opus");
            if (!istream)
                return; /// could return an error to the client here

            Stream stream;
            type_t type = istream.type();
            if (type == typeof(Stream))
                stream = istream.grab();
            else if (type == typeof(lambda<node(App&)>)) {
                stream = app_stream(istream.grab());
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
                    str id         = id_client.key.grab();
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

        state->sendInitialNalus = [](Stream stream, shared_ptr<ClientTrackData> video) {
            auto h264 = H264FileParser(stream->video.grab());
            auto initialNalus = h264.initialNALUS();

            // send previous NALU key frame so users don't have to wait to see stream works
            if (!initialNalus.empty()) {
                const double frameDuration_s = double(h264.Source::data->getSampleDuration_us()) / (1000 * 1000);
                const uint32_t frameTimestampDuration = video->sender->rtpConfig->secondsToTimestamp(frameDuration_s);
                video->sender->rtpConfig->timestamp = video->sender->rtpConfig->startTimestamp - frameTimestampDuration * 2;
                video->track->send(initialNalus);
                video->sender->rtpConfig->timestamp += frameTimestampDuration;
                // Send initial NAL units again to start stream in firefox browser
                video->track->send(initialNalus);
            }
        };

        state->addToStream = [state](Client client, bool isAddingVideo) {
            if (client->state == Client::State::Waiting) {
                client->state = isAddingVideo ? Client::State::WaitingForAudio : Client::State::WaitingForVideo;
            } else if ((client->state == Client::State::WaitingForAudio && !isAddingVideo)
                    || (client->state == Client::State::WaitingForVideo && isAddingVideo)) {

                // Audio and video tracks are collected now
                assert(client->video.has_value() && client->audio.has_value());

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
			str id = it->grab();
			it = message.get("type");
			if (!it) return;
			str type = it->grab();

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
