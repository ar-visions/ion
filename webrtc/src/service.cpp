#include <webrtc/webrtc.hpp>
#include <webrtc/streamer/stream.hpp>
#include <webrtc/streamer/fileparser.hpp>
#include <webrtc/streamer/h264fileparser.hpp>
#include <webrtc/streamer/opusfileparser.hpp>

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

using namespace ion;
using namespace std;
using namespace rtc;

/// args for this context
static ion::map<mx> args;

static std::unordered_map<std::string, shared_ptr<Client>> clients;


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
        condition.wait(lock, [this]{
            return (queue.size() || quit);
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


/// Incomming message handler for websocket
/// @param message Incoming message
/// @param config Configuration
/// @param ws Websocket
void wsOnMessage(var message, Configuration config, shared_ptr<WebSocket> ws) {
    mx *it = message.get("id");
    if (!it) return;
    str id = it->grab();
    it = message.get("type");
    if (!it) return;
    str type = it->grab();

    if (type == "request") {
        clients.emplace(id, createPeerConnection(config, make_weak_ptr(ws), id));
    } else if (type == "answer") {
        /// set and guard
        if (auto jt = clients.find(id); jt != clients.end()) {
            auto pc = jt->second->peerConnection;
            str sdp = message["sdp"];
            auto description = Description(sdp, type);
            pc->setRemoteDescription(description);
        }
    }
}

/// Main dispatch queue (this is main for webrtc module, its own abstract)
DispatchQueue MainThread("Main");

static shared_ptr<Stream> avStream = null;

/// service is a better name for it because its data as well as video.  its any stream

namespace webrtc {

/// general service function to facilitate https or web sockets messages
ion::async service(uri url, lambda<message(message)> fn_process) {

    /// web socket protocol is a connector protocol unless a method of listen is explicitly called elsewhere
    if (url->proto == protocol::ws) {
        return ion::async {1, [url, fn_process](runtime *rt, int i) -> mx {
            console.log("async test");

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

            ws->onOpen   ([ ]() {
                cout << "WebSocket connected, signaling ready" << endl;
            });

            ws->onClosed ([ ]() {
                cout << "WebSocket closed" << endl;
            });

            ws->onError  ([ ](const string &error) {
                cout << "WebSocket failed: " << error << endl;
            });

            ws->onMessage([&](variant<binary, string> data) {
                if (!holds_alternative<string>(data))
                    return;
                string s = get<string>(data);
                var message = var::parse((cstr)s.c_str());
                MainThread.dispatch([message, config, ws]() {
                    wsOnMessage(message, config, ws);
                });
            });

            str s_url = (uri&)url;
            std::string s_wsurl = s_url.cs();
            console.log("websocket is {0}", {s_wsurl.c_str() });
            ws->open(s_wsurl);

            console.log("ws connecting to signaling server: {0}", { url });

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

    /// https is all we support for a listening service, no raw protocols without encryption in this stack per design
    if (url->proto == protocol::https)
        return sock::listen(url, [fn_process](sock &sc) -> bool {
            bool close = false;
            for (close = false; !close;) {
                close  = true;
                message request(sc);
                if (!request)
                    break;
                message response(fn_process(request));
                response.write(sc);
                close = request.header("Connection") == "close";
            }
            return close;
        });

    console.fault("unsupported url: {0}", { url });
    return {};
};

}


shared_ptr<ClientTrackData> addVideo(const shared_ptr<PeerConnection> pc, const uint8_t payloadType, const uint32_t ssrc, const string cname, const string msid, const function<void (void)> onOpen) {
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
}

shared_ptr<ClientTrackData> addAudio(const shared_ptr<PeerConnection> pc, const uint8_t payloadType, const uint32_t ssrc, const string cname, const string msid, const function<void (void)> onOpen) {
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
}

// Create and setup a PeerConnection
shared_ptr<Client> createPeerConnection(const Configuration &config,
                                                weak_ptr<WebSocket> wws,
                                                string id) {
    auto pc = make_shared<PeerConnection>(config);
    auto client = make_shared<Client>(pc);

    pc->onStateChange([id](PeerConnection::State state) {
        cout << "State: " << state << endl;
        if (state == PeerConnection::State::Disconnected ||
            state == PeerConnection::State::Failed ||
            state == PeerConnection::State::Closed) {
            // remove disconnected client
            MainThread.dispatch([id]() {
                clients.erase(id);
            });
        }
    });

    pc->onGatheringStateChange(
        [wpc = make_weak_ptr(pc), id, wws](PeerConnection::GatheringState state) {
        cout << "Gathering State: " << state << endl;
        if (state == PeerConnection::GatheringState::Complete) {
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

    client->video = addVideo(pc, 102, 1, "video-stream", "stream1", [id, wc = make_weak_ptr(client)]() {
        MainThread.dispatch([wc]() {
            if (auto c = wc.lock()) {
                addToStream(c, true);
            }
        });
        cout << "Video from " << id << " opened" << endl;
    });

    client->audio = addAudio(pc, 111, 2, "audio-stream", "stream1", [id, wc = make_weak_ptr(client)]() {
        MainThread.dispatch([wc]() {
            if (auto c = wc.lock()) {
                addToStream(c, false);
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

/// Create stream
shared_ptr<Stream> createStream(const string h264Samples, const unsigned fps, const string opusSamples) {
    // video source
    auto video = make_shared<H264FileParser>(h264Samples, fps, true);
    // audio source
    auto audio = make_shared<OPUSFileParser>(opusSamples, true);

    auto stream = make_shared<Stream>(video, audio);
    // set callback responsible for sample sending
    stream->onSample([ws = make_weak_ptr(stream)](Stream::StreamSourceType type, uint64_t sampleTime, rtc::binary sample) {
        vector<ClientTrack> tracks{};
        string streamType = type == Stream::StreamSourceType::Video ? "video" : "audio";
        // get track for given type
        function<optional<shared_ptr<ClientTrackData>> (shared_ptr<Client>)> getTrackData = [type](shared_ptr<Client> client) {
            return type == Stream::StreamSourceType::Video ? client->video : client->audio;
        };
        // get all clients with Ready state
        for(auto id_client: clients) {
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

                cout << "Sending " << streamType << " sample with size: " << to_string(sample.size()) << " to " << client << endl;
                try {
                    // send sample
                    trackData->track->send(sample);
                } catch (const std::exception &e) {
                    cerr << "Unable to send "<< streamType << " packet: " << e.what() << endl;
                }
            }
        }
        MainThread.dispatch([ws]() {
            if (clients.empty()) {
                // we have no clients, stop the stream
                if (auto stream = ws.lock()) {
                    stream->stop();
                }
            }
        });
    });
    return stream;
}

/// Start stream
void startStream() {
    shared_ptr<Stream> stream;
    if (avStream) {
        stream = avStream;
        if (stream->isRunning) {
            // stream is already running
            return;
        }
    } else {
        stream = createStream("h264", 30, "opus");
        avStream = stream;
    }
    stream->start();
}

/// Send previous key frame so browser can show something to user
/// @param stream Stream
/// @param video Video track data
void sendInitialNalus(shared_ptr<Stream> stream, shared_ptr<ClientTrackData> video) {
    auto h264 = dynamic_cast<H264FileParser *>(stream->video.get());
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
}

/// Add client to stream
/// @param client Client
/// @param adding_video True if adding video
void addToStream(shared_ptr<Client> client, bool isAddingVideo) {
    if (client->getState() == Client::State::Waiting) {
        client->setState(isAddingVideo ? Client::State::WaitingForAudio : Client::State::WaitingForVideo);
    } else if ((client->getState() == Client::State::WaitingForAudio && !isAddingVideo)
               || (client->getState() == Client::State::WaitingForVideo && isAddingVideo)) {

        // Audio and video tracks are collected now
        assert(client->video.has_value() && client->audio.has_value());
        auto video = client->video.value();

        if (avStream) {
            sendInitialNalus(avStream, video);
        }

        client->setState(Client::State::Ready);
    }
    if (client->getState() == Client::State::Ready) {
        startStream();
    }
}
