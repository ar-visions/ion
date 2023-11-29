
/*
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
*/

#if 0

#include <mx/mx.hpp>
#include <media/video.hpp>

struct AVFrame;
struct AVCodec;
struct AVCodecContext;
struct AVStream;

struct iVideo {
    AVFormatContext   *format_ctx;
    AVStream          *video_st,    *audio_st;
    AVCodecContext    *video_ctx,   *audio_ctx;
    AVCodec           *video_codec, *audio_codec;
    AVFrame           *frame;
    struct SwsContext *sws_ctx;
    int                width, height, hz;
    int                audio_sample_rate = 48000;
    int                audio_channels = 1;
    bool               stopped;
    int                pts;

    register(iVideo);

    void encode_frame(AVStream *st, AVCodecContext *ctx) {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.stream_index = st->index; // Set the stream index
        pkt.data         = NULL;
        pkt.size         = 0;

        assert(avcodec_send_frame(ctx, frame) >= 0);
        while (avcodec_receive_packet(ctx, &pkt) == 0) {
            assert(av_write_frame(format_ctx, &pkt) >= 0);
            av_packet_unref(&pkt);
        }
    }

    void encode_video_frame() {
        encode_frame(video_st, video_ctx);
    }

    void encode_audio_frame() {
        encode_frame(audio_st, audio_ctx);
    }

    void stop() {
        if (!stopped) {
            av_frame_free(&frame);
            frame = null;
            encode_video_frame();
            encode_audio_frame();
            av_write_trailer(format_ctx);
            stopped = true;
        }
    }

    int write_frame(Frame &f) {
        // convert RGBA image to YUV420P
        const u8 *inData    [1] = { (u8*)f.image.pixels() }; // RGBA
        int       inLinesize[1] = { 4 * width }; // RGBA stride
        sws_scale(sws_ctx, inData, inLinesize, 0, height, frame->data, frame->linesize);
        encode_video_frame();
        encode_audio_frame();
        frame->pts = pts++;
        return 0;
    }

    ~iVideo() {
        stop();
        avcodec_free_context(&video_ctx);
        av_frame_free(&frame);
        sws_freeContext(sws_ctx);
        avformat_free_context(format_ctx);
    }

    void start(path &output) {
        // 1. Allocate format context and set format
        avformat_alloc_output_context2(&format_ctx, NULL, NULL, output.cs());
        assert(format_ctx);

        int      ret, i;

        audio_codec  = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_AAC); // You can use "aac" for AAC encoding
        audio_st     = avformat_new_stream(format_ctx, audio_codec);

        // Set audio parameters (sample format, sample rate, channel layout, etc.)
        audio_st->codecpar->codec_id        = audio_codec->id;
        audio_st->codecpar->codec_type      = AVMEDIA_TYPE_AUDIO;
        audio_st->codecpar->format          = AV_SAMPLE_FMT_FLTP;   // PCM float 32 data
        audio_st->codecpar->sample_rate     = audio_sample_rate;    // Sample rate (adjust as needed)
        audio_st->codecpar->channels        = 1;                    // Number of audio channels (stereo)
        audio_st->codecpar->channel_layout  = AV_CH_LAYOUT_MONO;    // Channel layout
        audio_st->time_base.num             = 1;
        audio_st->time_base.den             = audio_st->codecpar->sample_rate;

        // Configure audio_ctx settings
        audio_ctx   = avcodec_alloc_context3(audio_codec);
        audio_ctx->codec_id                 = audio_codec->id;
        audio_ctx->bit_rate                 = 128000;               // Adjust as needed (bitrate for audio)
        audio_ctx->sample_fmt               = AV_SAMPLE_FMT_FLTP;   // Adjust as needed (sample format)
        audio_ctx->sample_rate              = audio_sample_rate;    // Adjust as needed (sample rate)
        audio_ctx->channels                 = 1;                    // Adjust as needed (number of audio channels)
        audio_ctx->channel_layout           = AV_CH_LAYOUT_MONO;
        // Set other audio_ctx parameters as needed, such as codec-specific options

        // Open the audio codec context
        assert (avcodec_open2(audio_ctx, audio_codec, NULL) >= 0);

        // 2. Find and open video codec
        video_codec = (AVCodec*)avcodec_find_encoder(AV_CODEC_ID_H264);

        video_st    = avformat_new_stream(format_ctx, video_codec);
        video_st->codecpar->codec_id = video_codec->id;
        video_st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        video_st->codecpar->width = width;
        video_st->codecpar->height = height;
        video_st->codecpar->format = AV_PIX_FMT_YUV420P;        // Pixel format (adjust as needed)
        video_st->time_base = (AVRational){1, hz};              // Frame rate (adjust as needed)

        video_ctx   = avcodec_alloc_context3(video_codec);
        video_ctx->codec_id     = video_codec->id;
        video_ctx->bit_rate     = 400000; // Adjust as needed
        video_ctx->width        = width;
        video_ctx->height       = height;
        video_ctx->time_base    = (AVRational){ 1, hz }; // Adjust frame rate as needed
        video_ctx->framerate    = (AVRational){ hz, 1 }; // Adjust frame rate as needed
        video_ctx->gop_size     = 10; // Adjust as needed
        video_ctx->max_b_frames = 1; // Adjust as needed
        video_ctx->pix_fmt      = AV_PIX_FMT_YUV420P;

        /// constant quality mode
        video_ctx->qmin         = 10; // Set a minimum quantization value (adjust as needed)
        video_ctx->qmax         = 51; // Set a maximum quantization value (adjust as needed)
      //video_ctx->crf          = 18; // Set the desired CRF value (adjust as needed)

        assert (avcodec_open2(video_ctx, video_codec, NULL) >= 0);

        frame = av_frame_alloc();
        frame->format = video_ctx->pix_fmt;
        frame->width  = video_ctx->width;
        frame->height = video_ctx->height;

        ret = av_image_alloc(frame->data, frame->linesize, width, height, video_ctx->pix_fmt, 32); /// image holder
        sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL); /// converter
        ret = avformat_write_header(format_ctx, NULL);
        if (ret < 0) {
            char error_message[256];
            av_strerror(ret, error_message, sizeof(error_message));
            fprintf(stderr, "Error writing header: %s\n", error_message);
            exit(1);
        }
    }
};

mx_implement(Video, mx);

Video::Video(int width, int height, int hz, int audio_sample_rate, path output) : Video() {
    data->width  = width;
    data->height = height;
    data->hz     = hz;
    data->audio_sample_rate = audio_sample_rate;
    data->start(output);
}

int Video::write_frame(Frame &f) {
    return data->write_frame(f);
}

void Video::stop() {
    data->stop();
}

#endif