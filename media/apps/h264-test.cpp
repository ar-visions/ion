#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

int main(int argc, char* argv[])
{
    AVFrame*         videoFrame   = nullptr;
    AVCodecContext*  cctx         = nullptr;
    SwsContext*      swsCtx       = nullptr;
    int              frameCounter = 0;
    AVFormatContext* ofctx        = nullptr;
    AVOutputFormat*  oformat      = (AVOutputFormat*)av_guess_format(nullptr, "test.mp4", nullptr);
    int              fps          = 30;
    int              width        = 1920;
    int              height       = 1080;
    int              bitrate      = 2000;

    int err = avformat_alloc_output_context2(&ofctx, oformat, nullptr, "test.mp4");
    const AVCodec* codec = avcodec_find_encoder(oformat->video_codec);
    AVStream* stream = avformat_new_stream(ofctx, codec);
    cctx = avcodec_alloc_context3(codec); /// combined context

    const AVCodec*   audioCodec   = avcodec_find_encoder(AV_CODEC_ID_AAC);
    AVStream*        audioStream  = avformat_new_stream(ofctx, audioCodec);
    audioStream->codecpar->codec_id = AV_CODEC_ID_AAC;
    audioStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    audioStream->codecpar->sample_rate = 44100; /* Choose desired sample rate */
    audioStream->codecpar->channels = 2; /* Choose desired number of channels */
    audioStream->codecpar->bit_rate = 128000; /* Choose desired bitrate */
    audioStream->codecpar->frame_size = 44100 / 30;
    avcodec_parameters_to_context(cctx, audioStream->codecpar);


    auto free_state = [&]() {
        av_frame_free(&videoFrame);
        avcodec_free_context(&cctx);
        avformat_free_context(ofctx);
        sws_freeContext(swsCtx);
    };

    auto finish = [&]() {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;

        for (;;) {
            avcodec_send_frame(cctx, NULL);
            if (avcodec_receive_packet(cctx, &pkt) != 0)
                break;
            av_interleaved_write_frame(ofctx, &pkt);
            av_packet_unref(&pkt);
        }

        av_write_trailer(ofctx);
        int err = avio_close(ofctx->pb);
    };

    auto pushFrame = [&](uint8_t* data) {
        int err;
        if (!videoFrame) {
            videoFrame = av_frame_alloc();
            videoFrame->format = AV_PIX_FMT_YUV420P;
            videoFrame->width = cctx->width;
            videoFrame->height = cctx->height;
            err = av_frame_get_buffer(videoFrame, 32);
        }
        if (!swsCtx) {
            swsCtx = sws_getContext(cctx->width, cctx->height, AV_PIX_FMT_RGB24, cctx->width,
                cctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);
        }
        int inLinesize[1] = { 3 * cctx->width };

        // From RGB to YUV
        sws_scale(swsCtx, (const uint8_t* const*)&data, inLinesize, 0, cctx->height,
            videoFrame->data, videoFrame->linesize);
        videoFrame->pts = (1.0 / 30.0) * 90000 * (frameCounter++);
        std::cout << videoFrame->pts << " " << cctx->time_base.num << " " <<
            cctx->time_base.den << " " << frameCounter << std::endl;
        err = avcodec_send_frame(cctx, videoFrame);
        AV_TIME_BASE;
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        pkt.flags |= AV_PKT_FLAG_KEY;
        if (avcodec_receive_packet(cctx, &pkt) == 0) {
            av_interleaved_write_frame(ofctx, &pkt);
            av_packet_unref(&pkt);
        }
    };

    stream->codecpar->codec_id   = oformat->video_codec;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->width      = width;
    stream->codecpar->height     = height;
    stream->codecpar->format     = AV_PIX_FMT_YUV420P;
    stream->codecpar->bit_rate   = bitrate * 1000;
    avcodec_parameters_to_context(cctx, stream->codecpar);
    cctx->time_base              = {1,1};
    cctx->max_b_frames           = 2;
    cctx->gop_size               = 12;
    cctx->framerate              = { fps, 1 };

    //must remove the following
    //cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    assert (stream->codecpar->codec_id == AV_CODEC_ID_H264);
    av_opt_set(cctx, "preset", "ultrafast", 0);
    avcodec_parameters_from_context(stream->codecpar, cctx);
    err = avcodec_open2(cctx, codec, NULL);
    err = avio_open(&ofctx->pb, "test.mp4", AVIO_FLAG_WRITE);
    err = avformat_write_header(ofctx, NULL);

    av_dump_format(ofctx, 0, "test.mp4", 1);

    uint8_t* frameraw = new uint8_t[1920 * 1080 * 4];


    AVFrame audioFrame;
    audioFrame.format = av_get_sample_fmt("s16le"); // adjust format if needed
    audioFrame.sample_rate = 44100; // adjust sample rate if needed
    audioFrame.nb_samples = audioStream->codecpar->frame_size; // get frame size from stream params
    audioFrame.channels = 2; // adjust number of channels if needed
    av_frame_get_buffer(&audioFrame, 1); // allocate memory for frame data

    for (int i = 0; i < 255; ++i) {
        memset(frameraw, i, 1920 * 1080 * 4);
        pushFrame(frameraw);

        double frequency = 100.0 * (i / 255.0); // Adjust frequency based on frame number
        double phase = 0.0;
        for (int sample = 0; sample < audioFrame.nb_samples; ++sample) {
            double value = sin(2.0 * M_PI * frequency * sample / audioFrame.sample_rate + phase);
            // Scale and convert to audio format (adjust for different formats)
            audioFrame.data[0][sample] = (int16_t)(value * 32767.0);
            audioFrame.data[1][sample] = audioFrame.data[0][sample];
        }
        phase += 2.0 * M_PI * frequency * audioFrame.nb_samples / audioFrame.sample_rate; // Update phase for next frame

        if (avcodec_send_frame(cctx, &audioFrame) == 0) {
            AVPacket pkt;
            av_init_packet(&pkt);
            pkt.data = NULL;
            pkt.size = 0;
            if (avcodec_receive_packet(cctx, &pkt) == 0) {
                av_interleaved_write_frame(ofctx, &pkt);
                av_packet_unref(&pkt);
            }
        }
    }
    delete[] frameraw;
    finish();
    free_state();
    return 0;
}