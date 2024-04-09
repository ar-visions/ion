#define MP4V2_USE_STATIC_LIB
#include <mp4v2/mp4v2.h>
#include <fdk-aac/aacenc_lib.h>
#include <fdk-aac/aacdecoder_lib.h>

#include <kissfft/kiss_fft.h>
#include <kissfft/kiss_fftr.h>

#include <media/media.hpp>
#include <media/h264.hpp>
#include <media/video.hpp>

#include <iostream>
#include <random>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdint.h>

extern "C" {
    #include <h264bsd_decoder.h>
    #include <shine/layer3.h>
};

using namespace std;

namespace ion {

#if 1
/// fdk aac requires us to use their frame size; not exactly sure how to increase it either.
/// we pad our allocations to allow for this non divisible number 1024 to overlap our total samples
/// it seems to have multiple arguments and allow for it as long as the numberi s larger; however it doesnt work properly.
bool audio::save_m4a(path dest, i64 bitrate) {
    // AAC encoder initialization (you need to add error checks and configuration)
    HANDLE_AACENCODER encoder;
    int channels = this->channels();
    aacEncOpen(&encoder, 0, channels);

    aacEncoder_SetParam(encoder, AACENC_SAMPLERATE,     48000);
    aacEncoder_SetParam(encoder, AACENC_AOT,            2);
    aacEncoder_SetParam(encoder, AACENC_CHANNELORDER,   0);
    aacEncoder_SetParam(encoder, AACENC_CHANNELMODE,    channels == 1 ? MODE_1 : MODE_2);
    aacEncoder_SetParam(encoder, AACENC_BITRATE,        bitrate);
    aacEncoder_SetParam(encoder, AACENC_SIGNALING_MODE, 0);
    aacEncoder_SetParam(encoder, AACENC_TRANSMUX,       2); /// ADTS mode; this may have been the issue?

    AACENC_BufDesc in_buf        = { 0 }, out_buf = { 0 };
    AACENC_InArgs  in_args       = { 0 };
    AACENC_OutArgs out_args      = { 0 };
    
    aacEncEncode(encoder, NULL, NULL, NULL, NULL); // Initialize encoder
    AACENC_InfoStruct info = {0}; 
    assert(aacEncInfo(encoder, &info) == AACENC_OK);

    int frame_size = info.frameLength; /// this is before we * (PCM short) (2) * channels
    int n_frames   = (int)std::ceil((double)total_samples() / (double)frame_size);

    // Encoding loop
    int            in_identifier = IN_AUDIO_DATA, out_identifier = OUT_BITSTREAM_DATA;
    void          *in_ptr, *out_ptr;
    INT            in_size, in_elem_size, out_size, out_elem_size;
    UCHAR outbuf[20480]; // 20k should be more than enough
    MP4FileHandle mp4file = MP4Create((symbol)dest.cs(), 0);

    MP4TrackId track = MP4AddAudioTrack(
        mp4file, sample_rate(), frame_size, MP4_MPEG4_AAC_LC_AUDIO_TYPE);

    MP4SetTrackIntegerProperty(mp4file, track, "mdia.minf.stbl.stsd.*[0].channels", channels);
    MP4SetAudioProfileLevel(mp4file, 1);

    i16 *samples = this->samples();
    for (int i = 0; i < n_frames; i++) {
        // Set up input buffer
        in_ptr                     = &samples[i * frame_size * channels];
        in_size                    = frame_size * channels * sizeof(short);
        in_elem_size               = sizeof(short);
        in_buf.numBufs             = 1;
        in_buf.bufs                = &in_ptr;
        in_buf.bufferIdentifiers   = &in_identifier;
        in_buf.bufSizes            = &in_size;
        in_buf.bufElSizes          = &in_elem_size;

        // Set up output buffer
        out_ptr                    = outbuf;
        out_size                   = sizeof(outbuf);
        out_elem_size              = 1;
        out_buf.numBufs            = 1;
        out_buf.bufs               = &out_ptr;
        out_buf.bufferIdentifiers  = &out_identifier;
        out_buf.bufSizes           = &out_size;
        out_buf.bufElSizes         = &out_elem_size;

        // Set up in_args
        in_args.numInSamples       = frame_size * channels;
        
        // Encode frame
        if (aacEncEncode(encoder, &in_buf, &out_buf, &in_args, &out_args) != AACENC_OK)
            break;

        if (out_args.numOutBytes == 0)
            continue;

        MP4WriteSample(mp4file, track, (const uint8_t*)out_ptr, out_args.numOutBytes, MP4_INVALID_DURATION, 0, false);
    }

    MP4Close(mp4file, 0);
    aacEncClose(&encoder);
    return true;
}

#else 

/// writing mp3 without incident; sounds exactly the same at 256k
bool audio::save_m4a(path dest, i64 bitrate) {
    MP4FileHandle mp4file = MP4Create((symbol)dest.cs(), 0);
    const size_t samples_per_frame = 1152;
    MP4TrackId track = MP4AddAudioTrack(
        mp4file, data->sample_rate, samples_per_frame, MP4_MP3_AUDIO_TYPE);

    MP4SetTrackIntegerProperty(mp4file, track, "mdia.minf.stbl.stsd.*[0].channels", data->channels);
    MP4SetAudioProfileLevel(mp4file, 1);

    shine_config_t config { };
    shine_t        s;

    /// set defaults
    shine_set_config_mpeg_defaults(&config.mpeg);
    config.wave.channels   = (enum channels)data->channels;
    config.wave.samplerate = data->sample_rate;
    config.mpeg.bitr       = int(bitrate / 1000);
    config.mpeg.emph       = NONE; /// no emphasis
    config.mpeg.original   = 1;    /// original
    config.mpeg.mode       = data->channels == 2 ? STEREO : MONO;

    /// init shine
    if (!(s = shine_initialise(&config))) console.fault("cannot init shine");

    /// read pcm into shine, encode and write to output
    for (i64 i = 0; i < data->total_samples; i += samples_per_frame) {
        int n_bytes = 0;
        u8 *encoded = shine_encode_buffer_interleaved(
            s, &data->samples[i * data->channels], &n_bytes);
        MP4WriteSample(mp4file, track, (const uint8_t*)encoded, n_bytes, MP4_INVALID_DURATION, 0, false);
    }
    /// cleanup
    shine_close(s);
    
    // Finalize encoding
    MP4Close(mp4file, 0);
    return true;
}

#endif

struct Nalu:mx {
    enums(Annex, A, A, B);
    enums(Type, unspecified,
        unspecified  = 0,
        non_idr      = 1,
        part_A       = 2,
        part_B       = 3,
        part_C       = 4,
        idr          = 5,
        sei          = 6,
        sps          = 7,
        pps          = 8,
        aud          = 9,
        end_sequence = 10,
        end_stream   = 11,
        filler       = 12,
        sps_ext      = 13,
        prefix       = 14,
        subset_sps   = 15);

    struct M {
        Annex annex;
        Type  type;
        int   frame_id;
        bool  key_frame;
        mx    payload; /// does not include the type info, we have unframed it; supports either Annex A or B
        mx    origin; /// we hold onto the origin buffer (the buffer given to extract(mx))
        operator bool() {
            return type != Type::unspecified;
        }
    };

    static Nalu find(array nalus, Nalu::Type type) {
        for (Nalu &n: nalus.elements<Nalu>()) if (n->type == type) return n;
        return {};
    }

    static array extract(mx nalus, Annex annex = Annex::A) {
        array res(typeof(Nalu), 8);

        u8    *origin = nalus.origin<u8>();
        size_t count  = nalus.count();

        u8* p = origin;

        if (annex == Annex::A) {
            while (p < origin + count) {
                u8 *p_start = p;
                int nalu_length = 0;

                // Extract length field (1-4 bytes) based on the length field size
                // (Assume a maximum of 4 bytes for this example)
                switch (*p & 0x3F) {  // Mask to get the first 6 bits
                    case 0x00: nalu_length = *p++; break; // 1-byte length field
                    case 0x01: nalu_length = (p[0] << 8) | p[1]; p += 2; break; // 2-byte length field
                    case 0x03: nalu_length = (p[0] << 16) | (p[1] << 8) | p[2]; p += 3; break; // 3-byte length field
                    case 0x07: nalu_length = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4; break; // 4-byte length field
                    default:   // Handle invalid length field
                        // Handle error or skip to the next potential NALU
                        break;
                }

                if (nalu_length > 0) {
                    int nalu_header = *p++;
                    int nalu_type   = nalu_header & 0x1F;  // Extract NALU type
                    res += Nalu {
                        Annex(Annex::A),
                        Type(nalu_type),
                        memory::wrap(
                            typeof(u8), p_start, sz_t(sz_t(p - p_start) + nalu_length)
                        )
                    };
                    p   += nalu_length;  // Skip to the next NALU
                } else {
                    throw str("invalid nalu framing");
                }
            }
        } else {
            Type type_next;
            u8  *start_of_nalu = null;
            while (p < origin + count) {
                u8 *p_start = p;
                if (*p++ == 0 && *p++ == 0 && *p++ == 0 && *p++ == 1) {
                    int nalu_next = int(*p++ & 0x1F);
                    if (start_of_nalu) /// end_of_nalu is valid when this is set
                        res += Nalu {
                            Annex(Annex::B),
                            type_next, memory::wrap(typeof(u8), start_of_nalu, sz_t(p_start - start_of_nalu))
                        };
                    type_next     = Type(nalu_next);
                    start_of_nalu = p_start;
                }
            }
            /// if a type_next was read, we get the size of that compared to 
            sz_t last_sz = type_next ? sz_t(origin + count - start_of_nalu) : 0;
            if (last_sz)
                res += Nalu {
                    Annex(Annex::B), type_next, memory::wrap(typeof(u8), start_of_nalu, last_sz)
                };
        }
        return res;
    }

    Nalu(Annex a, Type t, mx payload):Nalu() {
        data->annex   = a;
        data->type    = t;
        data->payload = payload;
    }

    operator bool() {
        return *data;
    }

    mx_basic(Nalu);
};

struct iVideo {
    ion::path          playback;
    MP4FileHandle      mp4;
    MP4TrackId         video_track;
    MP4TrackId         audio_track;
    HANDLE_AACENCODER  aac_encoder;
    h264               h264_encoder;
    mutex              mtx_seek;

    ion::image         spec;

    i64                audio_timescale;
    i64                video_timescale;
    i64                video_duration;
    i64                video_frame_count;

    int                width, height, hz = 30;
    int                audio_rate        = 48000;
    int                audio_channels    = 1;
    int                audio_frame_size;
    int                pts;

    storage_t          dec;
    i64                current_frame_id = -1;

    AACENC_BufDesc     output_aac;
    AACENC_BufDesc     input_pcm;
    AACENC_InArgs      args { 0, 0 };
    AACENC_OutArgs     out_args { };

    u8*                buffer_aac;
    int                buffer_aac_id = OUT_BITSTREAM_DATA;
    int                buffer_aac_sz;
    int                buffer_aac_el = sizeof(u8);
    int                buffer_pcm_id = IN_AUDIO_DATA;
    int                buffer_pcm_sz;
    int                buffer_pcm_el = sizeof(short);
    Frame*             current = null;
    mutex              mtx_write;
    short             *pcm_buffer;
    int                pcm_buffer_len;
    int                pcm_buffer_sz;
    i64                start_time;
    bool               play_pause;
    ion::image         current_image;
    bool               fetching_frames;
    bool               unloaded;

    /// since iVideo needs to write a certain frame rate, we should wait for the first frame to arrive to set an origin
    /// swap system should come back (in streams)

    operator bool() {
        return mp4;
    }

    void load(ion::path p) {
        playback = p;
        mp4 = MP4Read(p.cs());
        video_track = MP4FindTrackId(mp4, 0, MP4_VIDEO_TRACK_TYPE, 0);
        audio_track = MP4FindTrackId(mp4, 0, MP4_AUDIO_TRACK_TYPE, 0);

        video_duration    = MP4GetTrackDuration (mp4, video_track);
        video_timescale   = MP4GetTrackTimeScale(mp4, video_track);
        video_frame_count = MP4GetTrackNumberOfSamples(mp4, video_track);

        audio_timescale   = MP4GetTrackTimeScale(mp4, audio_track);

        width  = MP4GetTrackVideoWidth (mp4, video_track);
        height = MP4GetTrackVideoHeight(mp4, video_track);

        /// verify duration mod timescale is integral
        assert(video_duration % video_timescale == 0);

        hz         = video_timescale;

        /// we must load the first sample in before it knows the SPS info; lets verify with the mp4 data
        /// its important to note that h264 can resize in mid stream with more SPS/PPS.  thats a cool feature
        h264bsdInit(&dec, 1);
    
        current_image = seek_frame(0);

        spec       = get_audio_spectrum(2, 1, 128, 0.25,
            Array<rgba> {
                {0.0, 0.0, 0.0, 1.0},
                {1.0, 0.0, 0.0, 1.0},
                {0.0, 1.0, 0.0, 1.0},
                {0.0, 0.0, 1.0, 1.0},
                {1.0, 0.0, 1.0, 1.0},
                {1.0, 1.0, 0.0, 1.0},
                {0.0, 1.0, 1.0, 1.0},
                {1.0, 1.0, 1.0, 1.0}
            },
            Array<float> {
                2000.0,
                1000.0 /// this is magnitude needed to get max color; so we interpolate across the cropped spectrum
            });

        spec.save("spec.png");

        fetching_frames = true;
        async([&](runtime *rt, int index) -> mx {
            int dispatch_time = 0;
            while (!unloaded) {
                i64 t = millis();
                if (start_time && current_frame_id < (video_frame_count - 1)) {
                    /// play_pause = true (playing)
                    u64 next_dispatch = (u64(start_time) * u64(1000)) + (i64)((current_frame_id + 1) * (u64(1000000) / u64(hz)));
                    wait_until(next_dispatch);
                    mtx_seek.lock();
                    seek_frame(current_frame_id + 1);
                    mtx_seek.unlock();
                    if (current_frame_id >= (video_frame_count - 1)) {
                        play_pause = false;
                        start_time = 0;
                    }
                } else {
                    /// paused
                    usleep(100000);
                }
            }
            fetching_frames = false;
            return true;
        });
    }

    void play_state(bool state) {
        if (state ^ play_pause) {
            play_pause = state;
            /// set start_time to adjust for origin, we are playing as though we started playing from the start if our current_frame_id is > 0
            /// seeking should update this
            if (state && current_frame_id >= (video_frame_count - 1))
                current_frame_id = 0;
            start_time = state ? millis() - i64(current_frame_id * (1000.0 / hz)) : 0;
        }
    }

    void unload() {
        unloaded = true;
        while (fetching_frames)
            usleep(100);
        
        mtx_write.lock();
        aacEncClose(&aac_encoder);
        MP4Close(mp4);
        mtx_write.unlock();
    }

    void write_audio(mx &audio) {
        /// must be short-based
        assert(audio.type() == typeof(short));
        sz_t len = audio.count();
        assert(pcm_buffer_len + len <= pcm_buffer_sz);

        /// append to pcm_buffer
        short *origin = audio.origin<short>();
        memcpy(&pcm_buffer[pcm_buffer_len], origin, len * sizeof(short));
        pcm_buffer_len += len;

        bool remainder = false;
        sz_t frame_w_channels = audio_frame_size * audio_channels;
        for (int i = 0; i < pcm_buffer_len; i += frame_w_channels) {
            int amount = pcm_buffer_len - i;
            if (amount < frame_w_channels) {
                short *cp = (short*)calloc(sizeof(short), pcm_buffer_sz);
                memcpy(cp, &pcm_buffer[i], amount * sizeof(short));
                pcm_buffer_len = amount;
                free(pcm_buffer);
                pcm_buffer = cp;
                remainder = true;
                break;
            } else {
                short *stack_origin = &pcm_buffer[i];
                input_pcm.bufs = (void**)&stack_origin;
                args.numInSamples = frame_w_channels;
                assert(aacEncEncode(aac_encoder, &input_pcm, &output_aac, &args, &out_args) == AACENC_OK);
                input_pcm.bufs = (void**)0;
                if (out_args.numOutBytes)
                    MP4WriteSample(mp4, audio_track, buffer_aac, out_args.numOutBytes, audio_frame_size, 0, true);
            }
        }

        if (!remainder) {
            memset(pcm_buffer, 0, pcm_buffer_len * sizeof(short));
            pcm_buffer_len = 0;
        }
    }

    void write_video(mx &data) {
        assert(data.type() == typeof(u8));
        MP4WriteSample(mp4, video_track, data.origin<u8>(), data.count());
    }

    void write_image(image &im) {
        ion::array nalus = h264_encoder.encode(im); /// needs a quality setting; this default is very high quality and constant quality too; meant for data science work
        write_video(nalus);
    }

    int write_frame(Frame &frame) {
        if (frame.image) {
            current_image = frame.image;
            write_image(frame.image);
        } else
            write_video(frame.video); /// not handling this case yet.  if we do it might be useful to decode the image
        
        if (frame.audio) /// we need to insert blank audio when this is not set, and we are using audio
            write_audio(frame.audio);
        
        return 1;
    }

    ~iVideo() {
        if (playback)
            h264bsdShutdown(&dec);
        unload();
    }

    void start(ion::path &output) {
        assert(audio_channels == 1 || audio_channels == 2);

        buffer_aac_sz = 128000 / 8 / hz * audio_channels * 10; /// adequate size for encoder
        buffer_pcm_sz = sizeof(short) * audio_channels * audio_rate / hz;
        buffer_aac = (u8*)   calloc(1, buffer_aac_sz);

        input_pcm.numBufs            = 1;
        input_pcm.bufs               = (void**)0;
        input_pcm.bufSizes           = &buffer_pcm_sz;
        input_pcm.bufferIdentifiers  = &buffer_pcm_id;
        input_pcm.bufElSizes         = &buffer_pcm_el;
        output_aac.numBufs           = 1;
        output_aac.bufs              = (void**)&buffer_aac;
        output_aac.bufSizes          = &buffer_aac_sz;
        output_aac.bufferIdentifiers = &buffer_aac_id;
        output_aac.bufElSizes        = &buffer_aac_el;
        
        // init aac encoder
        assert(aacEncOpen(&aac_encoder, 0, 1) == AACENC_OK);
        aacEncoder_SetParam(aac_encoder, AACENC_SAMPLERATE,     48000);
        aacEncoder_SetParam(aac_encoder, AACENC_AOT,            2);
        aacEncoder_SetParam(aac_encoder, AACENC_CHANNELORDER,   0);
        aacEncoder_SetParam(aac_encoder, AACENC_CHANNELMODE,    audio_channels == 1 ? MODE_1 : MODE_2);
        aacEncoder_SetParam(aac_encoder, AACENC_BITRATE,        128000);
        aacEncoder_SetParam(aac_encoder, AACENC_SIGNALING_MODE, 0);
        aacEncoder_SetParam(aac_encoder, AACENC_TRANSMUX,       2); /// ADTS mode; this may have been the issue?
        assert(aacEncEncode(aac_encoder, NULL, NULL, NULL, NULL) == AACENC_OK);
        
        AACENC_InfoStruct aac_info = { 0 };
        assert(aacEncInfo(aac_encoder, &aac_info) == AACENC_OK);
        audio_frame_size = aac_info.frameLength;

        pcm_buffer_sz = audio_frame_size * audio_channels * 64;
        pcm_buffer = (short*)calloc(sizeof(short), pcm_buffer_sz);

        mp4 = MP4Create((symbol)output.cs());
        assert(mp4 != MP4_INVALID_FILE_HANDLE);
        MP4SetAudioProfileLevel(mp4, 1);
        
        video_track = MP4AddH264VideoTrack(mp4,
            hz, 1, width, height, // time base is hz, duration is 1, 1/hz
            0x42, // Baseline Profile (BP)
            0x80, // no-constraints
            0x1F, // level 3.1, commonly used for standard high-definition video, can be indicated with the value 0x1F.
            3);   // 4-1. This value is used in the encoding of the length of the NAL units in the video stream. Typically, this is set to 3, which means the length field size is 4 bytes (since it's zero-based counting).

        assert(audio_rate % hz == 0);
        audio_track = MP4AddAudioTrack(
            mp4, audio_rate, audio_frame_size, MP4_MPEG2_AAC_LC_AUDIO_TYPE);

        MP4SetTrackIntegerProperty(mp4, audio_track, "mdia.minf.stbl.stsd.*[0].channels", audio_channels);
    }

    image get_audio_spectrum(
            int frame_units, int frame_overlap, const int spectral_height, float crop, /// crop value of 0.25 means we are extracting 11khz on 48k
            const Array<rgba> &colors, const Array<float> &scales) { /// scales across the color range is a reasonable idea, so one can make use of frequencies often maxed out (by raising scale), and vice versa.
        u8           *sample_data  = null;
        u32           sample_len   = 0;
        bool          is_key_frame = false;
        //MP4Timestamp ts     = MP4ConvertToTrackTimestamp(mp4, audio_track, 0, audio_timescale);
        MP4SampleId  sample = 0;//MP4GetSampleIdFromEditTime(mp4, audio_track, ts, null, null);
        HANDLE_AACDECODER dec = aacDecoder_Open(TT_MP4_ADTS, 1);
        assert(dec);
        assert(frame_overlap <= (frame_units / 2));

        const size_t fft_size = 1024 * frame_units; /// needs to be model attribute here (probably stream this data in; with this function adapting the decoding to pcm to a fft)
        const size_t overlap  = 1024 * frame_overlap; /// cannot exceed 50%
        kiss_fft_cfg cfg      = kiss_fft_alloc(fft_size, 0, NULL, NULL);
        short        *fft_window = new short[fft_size];
        kiss_fft_cpx *f_in       = new kiss_fft_cpx[fft_size];
        kiss_fft_cpx *f_out      = new kiss_fft_cpx[fft_size];
        size_t       fft_cur = 0;

        memset(fft_window, 0, sizeof(short) * fft_size);
        fft_cur = overlap;

        u64 total_audio_samples = MP4GetTrackNumberOfSamples(mp4, audio_track);

        int result_count = total_audio_samples; //(total_audio_samples - frame_units) / (frame_units - frame_overlap);

        image res(size { spectral_height, result_count }, null);
        num x_image = 0;

        while (MP4ReadSample(
                mp4, audio_track, ++sample, &sample_data, &sample_len,
                null, null, null, &is_key_frame)) {

            u8* input[1]    = { sample_data };
            u32 isz         = sample_len;
            u32 isz_valid   = sample_len;
            AAC_DECODER_ERROR  err = aacDecoder_Fill(dec, input, &isz, &isz_valid);
            assert(err == AAC_DEC_OK);

            // Decode the filled data
            short output[16 * 1024]; // Adjust buffer size as needed
            err = aacDecoder_DecodeFrame(dec, output, sizeof(output) / sizeof(short), 0);
            assert(err == AAC_DEC_OK);

            CStreamInfo *info = aacDecoder_GetStreamInfo(dec);
            assert(info);
            assert(info->numChannels == 1);
            assert(fft_size % info->frameSize == 0);

            //
            memcpy(&fft_window[fft_cur], output, sizeof(short) * info->frameSize);
            fft_cur += info->frameSize;

            // todo: reduce
            if (fft_cur == fft_size) {
                for (int i = 0; i < fft_size; i++) {
                    f_in[i].r = float(fft_window[i]);
                    f_in[i].i = 0.0f;
                }
                kiss_fft(cfg, f_in, f_out);

                /// average the spectrum (must perform cropping here too)
                for (int j = 0; j < spectral_height; j++) {
                    int n_bucket = fft_size / 2 * crop / spectral_height; /// div 2 to crop the negative frequencies

                    double r = 0.0;
                    for (int i = j * n_bucket; i < (j + 1) * n_bucket; i++) {
                        kiss_fft_cpx &o = f_out[i];
                        r += sqrt(o.r * o.r + o.i * o.i);
                    }
                    r /= n_bucket;
                    r = sqrt(r);

                    float       fj = float(j) / (spectral_height - 1);
                    float   indexf = fj * (scales.count() - 1);
                    int     index0 = floor(indexf);
                    int     index1 = indexf >= (scales.count() - 1) ? index0 : (index0 + 1);
                    float   blend1 = indexf - index0;
                    float   scale  = scales[index0] * (1.0 - blend1) + scales[index1] * blend1;
                    float   fscale = math::clamp(r / scale, 0.0, 1.0);
                    float c_indexf = fscale * (colors.count() - 1);
                    int   c_index0 = floor(c_indexf);
                    int   c_index1 = c_indexf >= (colors.count() - 1) ? c_index0 : (c_index0 + 1);
                    float c_blend1 = c_indexf - c_index0;
                    rgba     color = colors[c_index0].mix(colors[c_index1], c_blend1);
                    rgba8   &pixel = res[size { j, x_image }];
                    pixel          = rgba8 {
                        u8(std::round(color.x * 255.0)),
                        u8(std::round(color.y * 255.0)),
                        u8(std::round(color.z * 255.0)),
                        u8(std::round(color.w * 255.0))
                    };
                }

                memcpy(fft_window, &fft_window[fft_size - overlap], sizeof(short) * overlap);
                fft_cur = overlap;
                x_image++;
            }

            free(sample_data);
            sample_data = null;
            sample_len  = 0;
        }

        delete[] fft_window;
        delete[] f_in;
        delete[] f_out;

        free(cfg);
        assert(x_image == res.width());
        return res;
    }

    i64 current_frame() {
        return current_frame_id;
    }

    ion::image get_current_image() {
        return current_image;
    }

    ion::image seek_frame(i64 frame_id) {
        if (frame_id == current_frame_id)
            return current_image;
        
        bool is_sync_sample = false;
        bool sequential = current_frame_id < 0 ? false : frame_id == (current_frame_id + 1);
        doubly sample_nalus = {};

        if (!sequential)
            h264bsdResetStorage(&dec);
        
        for (int cur_frame = frame_id; cur_frame >= 0; cur_frame--) {
            MP4Timestamp ts          = MP4ConvertToTrackTimestamp(mp4, video_track, cur_frame, hz);
            MP4SampleId  sample_id   = MP4GetSampleIdFromEditTime(mp4, video_track, ts, null, null);
            u8*          sample_data = null;
            u32          sample_len  = 0;
            bool         read        = MP4ReadSample(mp4, video_track, sample_id, &sample_data,
                                                     &sample_len, null, null, null, &is_sync_sample);
            assert(read);

            array       sample = memory::wrap(typeof(u8), sample_data, sz_t(sample_len));
            bool        A      = sample_data[0] != 0 && sample_data[1] != 0 &&
                                 sample_data[2] != 0 && sample_data[3] != 1;
            assert(!A);

            /// mp4 dec should be framing agnostic but it seems to require Annex B
            array nalus = Nalu::extract(sample, A ? Nalu::Annex::A : Nalu::Annex::B);
            bool is_key_frame = Nalu::find(nalus, Nalu::Type::idr);
            for (Nalu &n: nalus.elements<Nalu>()) {
                n->frame_id  = sample_id;
                n->key_frame = is_key_frame;
            }
            sample_nalus->insert(0, nalus);
            
            if (sequential || is_key_frame) break;
        }

        assert(sample_nalus->len() && (sequential || sample_nalus[0][0]->key_frame));

        bool pic_ready = false;
        for (array &nalus: sample_nalus.elements<array>()) {
            for (Nalu &nalu: nalus.elements<Nalu>()) {
                u8* payload    = nalu->payload.origin<u8>();
                u32 len        = nalu->payload.count();
                u32 bytes_read = 0;
                u32 total      = 0;
                while (total < len) {
                    u32 dec_result = h264bsdDecode(&dec, &payload[total], len - total, (u32)nalu->frame_id, &bytes_read);
                    total += bytes_read;
                    switch (dec_result) {
                        case H264BSD_RDY:       // decoding finished, nothing special
                            break;
                        case H264BSD_PIC_RDY:   // decoding of a picture finished
                            pic_ready = true;
                            break;
                        case H264BSD_HDRS_RDY:  // picture dimensions etc can be read (we read it from the first frame)
                            break;
                        case H264BSD_ERROR:
                        case H264BSD_PARAM_SET_ERROR:
                            assert(false);
                            break;
                    }
                }
            }
        }

        assert(pic_ready);
        assert(h264bsdPicWidth (&dec) * 16 == (u32)std::ceil(width  / 16.0) * 16);
        assert(h264bsdPicHeight(&dec) * 16 == (u32)std::ceil(height / 16.0) * 16);

        u32   is_idr = 0;
        u32   num_mb = 0;
        u32   pic_id = 0;
        u32  *pic    = h264bsdNextOutputPictureRGBA(&dec, &pic_id, &is_idr, &num_mb);
        assert(pic); /// todo: not building the picture correctly from key frame

        ion::image image = memory::wrap(typeof(rgba8), pic, width * height); /// wrap the memory, and lightly construct image
        image.mem->shape = new ion::size { height, width }; /// todo: set this on the stack
        current_frame_id = frame_id; /// only read backwards if we are going non-sequential, or its the first attempt
        
        /// if we seek a non sequential frame we must set our start time to be an offset 
        /// protect current_frame_id & start_time in a mutex
        if (play_pause && !sequential) {
            start_time = millis() - i64(current_frame_id * (1000.0 / hz));
        }
        return image;
    }
};

mx_implement(Video, mx, iVideo);

Video::Video(int width, int height, int hz, int audio_rate, path output) : Video() {
    data->width      = width;
    data->height     = height;
    data->hz         = hz;
    data->audio_rate = audio_rate;
    data->start(output);
}

Video::Video(path file) : Video() {
    data->load(file);
}

i64 Video::duration() {
    return data->video_duration;
}

i64 Video::timescale() {
    return data->video_timescale;
}

int Video::frame_rate() {
return data->hz;
}

i64 Video::audio_timescale() {
    return data->audio_timescale;
}

i64 Video::current_frame() {
    return data->current_frame();
}

ion::image Video::get_current_image() {
    return data->get_current_image();
}

bool Video::get_play_state() {
    return data->play_pause;
}

ion::image Video::seek_frame(i64 frame_id) {
    data->mtx_seek.lock();
    ion::image image = data->seek_frame(frame_id);
    data->current_image = image; /// we cant write to this and return this because thats out of sync
    data->mtx_seek.unlock();
    return image;
}

int Video::write_frame(Frame &frame) {
    return data->write_frame(frame);
}

image Video::audio_spectrum() {
    return data->spec;
}

i64 Video::frame_count() {
    return data->video_frame_count;
}

void Video::play_state(bool play_pause) {
    data->play_state(play_pause);
}

void Video::stop() {
    data->play_state(false);
    data->unload();
}

bool Video::is_recording() {
    return !data->playback;
}

bool Video::is_playback() {
    return data->playback;
}

}