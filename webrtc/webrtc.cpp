#include <webrtc/webrtc.hpp>
#include <media/media.hpp>

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
namespace webrtc {



/// args for this context
static ion::map<mx> args;

Stream::Stream(std::shared_ptr<StreamSource> video, std::shared_ptr<StreamSource> audio):
	std::enable_shared_from_this<Stream>(), video(video), audio(audio) { }

Stream::~Stream() {
    stop();
}

std::pair<std::shared_ptr<StreamSource>, Stream::StreamSourceType> Stream::unsafePrepareForSample() {
    std::shared_ptr<StreamSource> ss;
    StreamSourceType sst;
    uint64_t nextTime;
    if (audio->getSampleTime_us() < video->getSampleTime_us()) {
        ss = audio;
        sst = StreamSourceType::Audio;
        nextTime = audio->getSampleTime_us();
    } else {
        ss = video;
        sst = StreamSourceType::Video;
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

void Stream::sendSample() {
    std::lock_guard lock(mutex);
    if (!isRunning) {
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

void Stream::onSample(std::function<void (StreamSourceType, uint64_t, rtc::binary)> handler) {
    sampleHandler = handler;
}

void Stream::start() {
    std::lock_guard lock(mutex);
    if (isRunning) {
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

void Stream::stop() {
    std::lock_guard lock(mutex);
    if (!isRunning) {
        return;
    }
    _isRunning = false;
    dispatchQueue.removePending();
    audio->stop();
    video->stop();
};

FileParser::FileParser(string directory, string extension, uint32_t samplesPerSecond, bool loop) {
    this->directory = directory;
    this->extension = extension;
    this->loop = loop;
    this->sampleDuration_us = 1000 * 1000 / samplesPerSecond;
}

FileParser::~FileParser() {
	stop();
}

void FileParser::start() {
    sampleTime_us = std::numeric_limits<uint64_t>::max() - sampleDuration_us + 1;
    loadNextSample();
}

void FileParser::stop() {
    sample = {};
    sampleTime_us = 0;
    counter = -1;
}

void FileParser::loadNextSample() {
    string frame_id = to_string(++counter);

    string url = directory + "/sample-" + frame_id + extension;
    ifstream source(url, ios_base::binary);
    if (!source) {
        if (loop && counter > 0) {
            loopTimestampOffset = sampleTime_us;
            counter = -1;
            loadNextSample();
            return;
        }
        sample = {};
        return;
    }

    vector<uint8_t> fileContents((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    sample = *reinterpret_cast<vector<byte> *>(&fileContents);
    sampleTime_us += sampleDuration_us;
}

rtc::binary FileParser::getSample() {
	return sample;
}

uint64_t FileParser::getSampleTime_us() {
	return sampleTime_us;
}

uint64_t FileParser::getSampleDuration_us() {
	return sampleDuration_us;
}

H264FileParser::H264FileParser(string directory, uint32_t fps, bool loop): FileParser(directory, ".h264", fps, loop) { }

void H264FileParser::loadNextSample() {
    FileParser::loadNextSample();

    size_t i = 0;
    while (i < sample.size()) {
        assert(i + 4 < sample.size());
        auto lengthPtr = (uint32_t *) (sample.data() + i);
        uint32_t length = ntohl(*lengthPtr);
        auto naluStartIndex = i + 4;
        auto naluEndIndex = naluStartIndex + length;
        assert(naluEndIndex <= sample.size());
        auto header = reinterpret_cast<rtc::NalUnitHeader *>(sample.data() + naluStartIndex);
        auto type = header->unitType();
        switch (type) {
            case 7:
                previousUnitType7 = {sample.begin() + i, sample.begin() + naluEndIndex};
                break;
            case 8:
                previousUnitType8 = {sample.begin() + i, sample.begin() + naluEndIndex};;
                break;
            case 5:
                previousUnitType5 = {sample.begin() + i, sample.begin() + naluEndIndex};;
                break;
        }
        i = naluEndIndex;
    }
}

vector<byte> H264FileParser::initialNALUS() {
    vector<byte> units{};
    if (previousUnitType7.has_value()) {
        auto nalu = previousUnitType7.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    if (previousUnitType8.has_value()) {
        auto nalu = previousUnitType8.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    if (previousUnitType5.has_value()) {
        auto nalu = previousUnitType5.value();
        units.insert(units.end(), nalu.begin(), nalu.end());
    }
    return units;
}

OPUSFileParser::OPUSFileParser(string directory, bool loop, uint32_t samplesPerSecond): FileParser(directory, ".opus", samplesPerSecond, loop) { }

shared_ptr<Client> createPeerConnection(
    const Configuration &config, weak_ptr<WebSocket> wws, string id);

void addToStream(shared_ptr<Client> client, bool isAddingVideo);

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


ClientTrackData::ClientTrackData(shared_ptr<Track> track, shared_ptr<RtcpSrReporter> sender) {
	this->track = track;
	this->sender = sender;
}

void Client::setState(State state) {
	std::unique_lock lock(_mutex);
	this->state = state;
}

Client::State Client::getState() {
	std::shared_lock lock(_mutex);
	return state;
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

}

namespace ion {
/// can update in real time 1/hz or through polling, but not needed at the moment
int Services::run() {

    data->video_sink = [](mx nalu) {
        console.log("nalu bytes: {0}", { int(nalu.count()) });
        /// we hand off to clients that need their data here
    };
    
    node e = data->service_fn(*this);
    update_all(e);
    for (;;) {
        usleep(10000);
    }
    return 0;
}

void Service::mounted() {	
	state->service = webrtc::service(state->url, state->on_message);
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
        using Client = webrtc::Client;
        using Stream = webrtc::Stream;
        using ClientTrack = webrtc::ClientTrack;
        using ClientTrackData = webrtc::ClientTrackData;
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
        state->createPeerConnection = [state](Configuration &config, weak_ptr<WebSocket> wws, string id) {
            auto pc = make_shared<PeerConnection>(config);
            auto client = make_shared<Client>(pc);

            pc->onStateChange([state, id](PeerConnection::State peer) {
                cout << "State: " << int(peer) << endl;
                if (peer == PeerConnection::State::Disconnected ||
                    peer == PeerConnection::State::Failed ||
                    peer == PeerConnection::State::Closed) {
                    // remove disconnected client
                    state->MainThread->dispatch([state, id]() {
                        state->clients.erase(id);
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

            client->video = state->addVideo(pc, 102, 1, "video-stream", "stream1", [state, id, wc = make_weak_ptr(client)]() {
                state->MainThread->dispatch([state, wc]() {
                    if (auto c = wc.lock()) {
                        state->addToStream(c, true);
                    }
                });
                cout << "Video from " << id << " opened" << endl;
            });

            client->audio = state->addAudio(pc, 111, 2, "audio-stream", "stream1", [state, id, wc = make_weak_ptr(client)]() {
                state->MainThread->dispatch([state, wc]() {
                    if (auto c = wc.lock()) {
                        state->addToStream(c, false);
                    }
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
        state->startStream = [state](shared_ptr<webrtc::Client> client) {
            shared_ptr<webrtc::Stream> stream = state->stream_select(client);//state->createStream("h264", 30, "opus");
            
            if (!stream) {
                return;
            }

            // below should be a generic, no user needs to define onSample

            // set callback responsible for sample sending
            stream->onSample(
                    [state, wstream = make_weak_ptr(stream)](Stream::StreamSourceType type, uint64_t sampleTime, rtc::binary sample) {
                vector<ClientTrack> tracks{};
                string streamType = type == Stream::StreamSourceType::Video ? "video" : "audio";
                // get track for given type
                function<optional<shared_ptr<ClientTrackData>> (shared_ptr<Client>)> getTrackData = [type](shared_ptr<Client> client) {
                    return type == Stream::StreamSourceType::Video ? client->video : client->audio;
                };
                // get all clients with Ready state
                // this needs to go only to the clients with this stream attached
                // makes sense to go have stream -> clients
                for(auto id_client: state->clients) {
                    auto id = id_client.first;
                    auto client = id_client.second;
                    auto optTrackData = getTrackData(client);
                    if (client->getState() == Client::State::Ready && optTrackData.has_value()) {
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
                state->MainThread->dispatch([state, wstream]() {
                    if (auto stream = wstream.lock()) {
                        if (state->client_count(stream) == 0)
                            stream->stop();
                    }
                });
            });


            bool found = false;
            for (auto &s: state->avStreams) {
                if (stream == s) {
                    found = true;
                    break;
                }
            }
            if (!found)
                state->avStreams->push(stream); /// we need to deal with this single instance
            if (!stream->isRunning)
                stream->start();
        };

        /// need to know how required this is; it seems to only happen after the 2nd client of course because the stream 'starts' afterwards
        /// Send previous key frame so browser can show something to user
        /// @param stream Stream
        /// @param video Video track data
        state->sendInitialNalus = [](shared_ptr<Stream> stream, shared_ptr<ClientTrackData> video) {
            auto h264 = dynamic_cast<webrtc::H264FileParser *>(stream->video.get());
            auto initialNalus = h264->initialNALUS();

            // send previous NALU key frame so users don't have to wait to see stream works
            if (!initialNalus.empty()) {
                const double frameDuration_s = double(h264->getSampleDuration_us()) / (1000 * 1000);
                const uint32_t frameTimestampDuration = video->sender->rtpConfig->secondsToTimestamp(frameDuration_s);
                video->sender->rtpConfig->timestamp = video->sender->rtpConfig->startTimestamp - frameTimestampDuration * 2;
                video->track->send(initialNalus);
                video->sender->rtpConfig->timestamp += frameTimestampDuration;
                // Send initial NAL units again to start stream in firefox browser
                video->track->send(initialNalus);
            }
        };

        /// Add client to stream
        /// @param client Client
        /// @param adding_video True if adding video
        state->addToStream = [state](shared_ptr<Client> client, bool isAddingVideo) {
            if (client->getState() == Client::State::Waiting) {
                client->setState(isAddingVideo ? Client::State::WaitingForAudio : Client::State::WaitingForVideo);
            } else if ((client->getState() == Client::State::WaitingForAudio && !isAddingVideo)
                    || (client->getState() == Client::State::WaitingForVideo && isAddingVideo)) {

                // Audio and video tracks are collected now
                assert(client->video.has_value() && client->audio.has_value());

                //auto video = client->video.value();
                /// this is Optional and not advised here because we havent called the stream_select
                //if (state->avStream) {
                //    state->sendInitialNalus(state->avStream, video);
                //}

                client->setState(Client::State::Ready);
            }
            
            if (client->getState() == Client::State::Ready) {
                state->startStream(client);
            }
        };

        state->client_count = [state](shared_ptr<Stream> stream) -> int {
            int r = 0;
            Stream *st = stream.get();
            for (auto &field: state->clients) {
                shared_ptr<Client> &c = field.second;
                if (c->stream == st)
                    r++;
            }
            return r;
        };

		state->wsOnMessage = [state](var message, Configuration config, shared_ptr<WebSocket> ws) {
			mx *it = message.get("id");
			if (!it) return;
			str id = it->grab();
			it = message.get("type");
			if (!it) return;
			str type = it->grab();

			/// integrate this into the context tree

			if (type == "request") {
				state->clients.emplace(id, state->createPeerConnection(config, make_weak_ptr(ws), id));
			} else if (type == "answer") {
				/// set and guard
				if (auto jt = state->clients.find(id); jt != state->clients.end()) {
					auto pc = jt->second->peerConnection;
					str sdp = message["sdp"];
					auto description = Description(sdp, type);
					pc->setRemoteDescription(description);
				}
			}
		};

		/// create web socket for connection to relay service
		/// (should define this in administrative/accounting with data bindings)
		auto ws = make_shared<WebSocket>();
		Configuration config;
		string stunServer = "stun:stun.l.google.com:19302";
		cout << "STUN server is " << stunServer << endl;
		config.iceServers.emplace_back(stunServer);
		config.disableAutoNegotiation = true;
		string localId = "server";
		cout << "The local ID is: " << localId << endl;

        /// server doesnt connect to the session.

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
			
			//console.log("onMessage: {0}", { sdata });
			///
			var message = var::parse((cstr)sdata.c_str());
			//console.log("id: {0}, type: {1}", { message["id"], message["type"] });

			/// lets do without these if possible; async can call on main thread without issue
			state->MainThread->dispatch([state, message, config, ws]() {
				//wsOnMessage(message, config, ws);
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

		/// wait in loop here, needs ability to cancel
		while (ws->isOpen()) {
				this_thread::sleep_for(100ms);
		}
		return true;
	}};
}
}
