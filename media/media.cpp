#include <mx/mx.hpp>
#include <image/image.hpp>
#include <media/media.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _CRT_SECURE_NO_WARNINGS
#include <zlib.h>

#define  MINIMP3_ONLY_MP3
#define  MINIMP3_ONLY_SIMD
#define  MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#define DR_WAV_IMPLEMENTATION
#include <media/dr_wav.h>

//#include <gsl/gsl_fft_complex.h>
//#include <gsl/gsl_errno.h>
//#include <gsl/gsl_fft_real.h>

#include <ogg/ogg.h>
#include <opus/opus.h>
#include <opusenc/opusenc.h>
#include <opusfile/opusfile.h>

extern "C" {
    #include <shine/layer3.h>
};

namespace ion {

mx inflate(mx input) {
    size_t sz = input.byte_len();
    array<char> out(sz * 4);
    
    // setup zlib inflate stream
    z_stream stream { };
    stream.next_in  = Z_NULL;
    stream.avail_in = 0;
    stream.zalloc   = Z_NULL;
    stream.zfree    = Z_NULL;
    stream.opaque   = Z_NULL;

    if (::inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        console.fault("inflate failure: init");
        return {};
    }

    u8 *src = input.data<u8>();
    stream.avail_in = u32(sz);
    stream.next_in  = src;
    const size_t CHUNK_SIZE = 1024;
    char buf[CHUNK_SIZE];
    do {
        stream.avail_out = CHUNK_SIZE;
        stream.next_out  = (Bytef*)buf;
        int res = ::inflate(&stream, Z_NO_FLUSH);  // inflate the data
        ///
        if (res == Z_NEED_DICT  ||
            res == Z_DATA_ERROR ||
            res == Z_MEM_ERROR) {
            ::inflateEnd(&stream);
            console.fault("inflate failure: {0}", int(res));
            return {};
        }
        ///
        size_t inflated_sz = CHUNK_SIZE - stream.avail_out;
        out.push(buf, inflated_sz);
        ///
    } while (stream.avail_out == 0);
    ///
    ::inflateEnd(&stream);
    return out;
}

image audio::fft_image(ion::size sz) {
    return image();
}

/// utility for setting artist and title on mp3 files only
static bool mp3_set_id3(path p, str artist, str title) {
    cstr  s_path = p.cs();
    FILE* mp3f   = fopen(s_path, "r+b");
    if (mp3f == NULL) {
        console.fault("mp3_set_idv3v2: failed to open {0}", { str(s_path) });
        return false;
    }

    // calculate the total size of the artist and song fields
    size_t artist_sz = artist.len() + 1;
    size_t  title_sz = title.len()  + 1;

    // create the artist and song metadata frames
    u8 *artistFrame = new u8[10 + artist_sz];  // Frame ID (4 bytes) + Frame Size (4 bytes) + Flags (2 bytes) + Artist field value
    u8 *titleFrame  = new u8[10 +  title_sz]; // Frame ID (4 bytes) + Frame Size (4 bytes) + Flags (2 bytes) + Song field value

    memcpy(artistFrame, "TPE1", 4);  // Frame ID: TPE1 (artist)
    memcpy(titleFrame,  "TIT2", 4);    // Frame ID: TIT2 (song)

    // set the frame sizes (excluding the frame ID and frame size fields)
    artistFrame[4] = (artist_sz >> 24) & 0xFF;
    artistFrame[5] = (artist_sz >> 16) & 0xFF;
    artistFrame[6] = (artist_sz >> 8)  & 0xFF;
    artistFrame[7] =  artist_sz & 0xFF;

    titleFrame[4] = (title_sz >> 24) & 0xFF;
    titleFrame[5] = (title_sz >> 16) & 0xFF;
    titleFrame[6] = (title_sz >> 8)  & 0xFF;
    titleFrame[7] =  title_sz & 0xFF;

    // write the artist and song fields to the frames
    memcpy(artistFrame + 10, artist.cs(), artist_sz);
    memcpy(titleFrame  + 10,  title.cs(),  title_sz);

    // move the file position indicator to the beginning of the file
    fseek(mp3f, 0, SEEK_SET);

    // check if an ID3v2 tag exists
    unsigned char id3Header[10];
    fread(id3Header, 1, 10, mp3f);

    if (memcmp(id3Header, "ID3", 3) == 0) {
        // ID3v2 tag exists, so overwrite the existing tag with the new frames
        fwrite(id3Header, 1, 10, mp3f);  // Write the existing ID3v2 header
        size_t tagSize = ((id3Header[6] & 0x7F) << 21) | ((id3Header[7] & 0x7F) << 14) | ((id3Header[8] & 0x7F) << 7) | (id3Header[9] & 0x7F);
        fseek(mp3f, tagSize, SEEK_CUR);  // Skip the existing tag

        // write the new artist and song frames
        fwrite(artistFrame, 1, sizeof(artistFrame), mp3f);
        fwrite(titleFrame,  1, sizeof(titleFrame),  mp3f);
    } else {
        // no ID3v2 tag exists, so create a new one
        const char id3Header[] = "ID3\x03\x00\x00\x00\x00\x00";
        fwrite(id3Header, 1, sizeof(id3Header), mp3f);

        // Write the new artist and song frames
        fwrite(artistFrame, 1, sizeof(artistFrame), mp3f);
        fwrite(titleFrame,  1, sizeof(titleFrame),  mp3f);
    }
    
    fclose(mp3f);
    delete[] artistFrame;
    delete[] titleFrame;
    return true;
}

/// use this to hide away the data and isolate its dependency
mx_implement(audio, mx);










void audio::convert_mono() {
    if (data->channels == 1)
        return;
    i16   *n_samples = new short[data->total_samples]; /// now its 1:1 with total_samples because new channels = 1
    for (int       i = 0; i < data->total_samples; i++) {
        float    sum = 0;
        for (int   c = 0; c < data->channels; c++)
                sum += data->samples[i * data->channels + c];
        n_samples[i] = sum / data->channels;
    }
    delete[] data->samples;  /// delete previous data
    data->samples  = n_samples; /// set new data
    data->channels = 1;        /// set to 1 channel
}

/// save / export to destination format (.mp3 .opus .wav support)
bool audio::save(path dest, i64 bitrate) {
    str  ext    = dest.ext();
    cstr s_path = dest.cs();
    
    /// conversion to opus
    if (ext == ".opus") {
        const i64        latency    = 20; /// compute 20ms frame size
        const i64        frame_size = data->sample_rate / (1000 / latency);
        OggOpusEnc      *enc;
        OggOpusComments *comments;
        int              error;
        
        comments = ope_comments_create();
        ope_comments_add(comments, "ARTIST", data->artist.cs());
        ope_comments_add(comments, "TITLE",  data->title.cs());
        enc = ope_encoder_create_file(s_path, comments, data->sample_rate, data->channels, 0, &error);
        ope_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate));
        
        if (!enc) {
            console.fault("error encoding to file {0}: {1}", { str(s_path), error });
            ope_comments_destroy(comments);
            return false;
        }
        
        for (i64 i = 0; i < data->total_samples; i += frame_size) {
            i64 remain = data->total_samples - i;
            ope_encoder_write(enc, &data->samples[i * data->channels], int(math::min(remain, frame_size))); // number of frames to write
        }
        ope_encoder_drain(enc); /// drain? flush!
        ope_encoder_destroy(enc);
        ope_comments_destroy(comments);
        
    } else if (ext == ".mp3") {
        FILE          *mp3_file = fopen(s_path, "wb");
        shine_config_t config;
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
        const size_t samples_per_frame = 1152;
        for (i64 i = 0; i < data->total_samples; i += samples_per_frame) {
            int n_bytes = 0;
            u8 *encoded = shine_encode_buffer_interleaved(
                s, &data->samples[i * data->channels], &n_bytes);
            fwrite(encoded, sizeof(u8), n_bytes, mp3_file);
        }
        /// cleanup
        shine_close(s);
        fclose(mp3_file);
        
        /// set id3 info separate
        mp3_set_id3(s_path, data->artist, data->title);
        ///
    } else if (ext == ".wav") {
        /// hiii everybody!
        /// hiii dr wav!
        drwav_data_format format { };
        format.container      = drwav_container_riff;
        format.format         = DR_WAVE_FORMAT_PCM;
        format.channels       = u32(data->channels);
        format.sampleRate     = u32(data->sample_rate);
        format.bitsPerSample  = 16;
        
        drwav wav;
        if (!drwav_init_file_write(&wav, s_path, &format, null))
            console.fault("failed to open output wav file for writing.");
        /// write the PCM frames to the WAV file
        /// drwav_write_pcm_frames(pWav, frameCount, pSamples)
        drwav_uint64 w = drwav_write_pcm_frames(&wav, u64(data->total_samples), data->samples);
        if (w != data->total_samples)
            console.fault("failed to write all pcm frames to the wav file.");
        drwav_uninit(&wav);
    }
    return true;
}

audio::audio(path res, bool force_mono) : mx(&data) {
    str  ext = res.ext();
    cstr s_path = res.cs();
    int  error;
    ///
    if (ext == ".opus") {
        /// open file
        OggOpusFile *opus_file = op_open_file(s_path, &error);
        if (error != 0) console.fault("failed to open file: {0}, error {1}", { error });

       OpusTags* tags = (OpusTags*)op_tags(opus_file, 0);
        if (tags != NULL) {
            data->artist = opus_tags_query(tags, "ARTIST", 0);
            data->title  = opus_tags_query(tags, "TITLE",  0);
        }
        
        /// allocate samples
        const int buffer_size = 1024;
        opus_int16 buffer[buffer_size];
        data->channels = op_channel_count(opus_file, -1);
        data->total_samples = op_pcm_total(opus_file, -1);
        data->samples = new i16[data->total_samples * data->channels];
        const OpusHead *head = op_head(opus_file, -1);
        if (!head) console.fault("failed to read opus head");
        data->sample_rate = i32(head->input_sample_rate);
        
        /// read samples
        size_t t = 0;
        int samples_read = 0;
        do {
            samples_read = op_read(opus_file, &data->samples[t], buffer_size, NULL);
            if (samples_read < 0)
                console.fault("error reading file: {0}", { samples_read });
            
            t += samples_read * data->channels;
        } while (samples_read != 0);
        op_free(opus_file);
        
        /// this will always result in exactly total_samples read -- or a crash
        if (t != data->total_samples) console.fault("sample count mismatch: {0}", { str(res) });
        
    } else if (ext == ".mp3") {
        /// open file
        mp3dec_t         dec;
        mp3dec_ex_t      api;
        mp3dec_init(&dec);
        assert(!mp3dec_ex_open(&api, s_path, MP3D_SEEK_TO_SAMPLE));
        data->total_samples = api.samples / data->channels;
        data->channels      = api.info.channels;
        data->sample_rate   = api.info.hz;
        
        /// read samples
        data->samples       = new short[data->total_samples * data->channels];
        assert(mp3dec_ex_read(&api, data->samples, api.samples) == data->total_samples * data->channels);
        mp3dec_ex_close(&api);
        ///
    } else if (ext == ".wav") {
        drwav w;
        if (!drwav_init_file(&w, s_path, null)) {
            console.fault("killer wav: {0}", { str(s_path) });
        }
        ///
        data->channels      = w.channels;
        data->sample_rate   = w.sampleRate;
        data->total_samples = w.totalPCMFrameCount;
        data->samples       = new i16[data->total_samples * data->channels];
        int t = 0;

        /// put faith in dr wav
        for (;;) {
            u64 read = drwav_read_pcm_frames_s16(&w, 2048, &data->samples[t]);
            if (read == 0) break;
            t += read * data->channels;
        }
        /// mirror handed to you.  check mirror
        console.test(t == (data->total_samples * data->channels), "data integrity error");
    } else {
        console.fault("extension not supported: {0}", { ext });
    }
    
    /// convert to mono if we feel like it.
    if (data && data->channels > 1 && force_mono)
        convert_mono();
}

/// obtain short waves from skinny aliens
array<short> audio::pcm_data() {
    return mx::wrap<short>(data->samples, data->total_samples * data->channels);
}

int           audio::channels() { return  data->channels;                    }
audio::operator          bool() { return  data->total_samples >  0;          }
bool         audio::operator!() { return  data->total_samples == 0;          }
size_t            audio::size() { return  data->total_samples * data->channels; }
size_t       audio::mono_size() { return  data->total_samples;               }

}
