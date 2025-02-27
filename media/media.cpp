#include <mx/mx.hpp>
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

#if 0
extern "C" {
    #include <shine/layer3.h>
};
#endif

namespace ion {


MediaBuffer::MediaBuffer(Media type, const mx &buf, int id):MediaBuffer() {
    /// assert supported media and their respective mx data formats
    if (buf.type() == typeof(float)) {
        assert(type == Media::PCMf32);
    } else if (buf.type() == typeof(short)) {
        assert(type == Media::PCM);
    } else if (buf.type() == typeof(u8)) {
        assert(type == Media::YUY2 || type == Media::NV12);
    } else
        assert(false);

    data->type = type;
    data->buf  = buf;
    data->id   = id;
}

MediaBuffer::MediaBuffer(PCMInfo &pcm, const mx &buf, int id) : MediaBuffer() {
    data->pcm  = pcm;
    data->type = pcm->format;
    data->buf  = buf;
    data->id   = id;
    assert(pcm->format == Media::PCM || pcm->format == Media::PCMf32);
}

/// hand-off constructor
MediaBuffer::MediaBuffer(PCMInfo &pcm) : MediaBuffer() {
    data->pcm  = pcm;
    data->type = pcm->format;
    data->buf  = pcm->audio_buffer; /// no copy
    pcm->audio_buffer = array(typeof(u8), 0, data->buf.reserve()); /// recycle potential
}

PCMu32 ::PCMu32(const array &buf) : MediaBuffer(Media::PCMu32, buf, 0) { }
PCMf32 ::PCMf32(const array &buf) : MediaBuffer(Media::PCMf32, buf, 0) { }
PCM    ::PCM   (const array &buf) : MediaBuffer(Media::PCM,    buf, 0) { }
YUY2   ::YUY2  (const array &buf) : MediaBuffer(Media::YUY2,   buf, 0) { }
NV12   ::NV12  (const array &buf) : MediaBuffer(Media::NV12,   buf, 0) { }
MJPEG  ::MJPEG (const array &buf) : MediaBuffer(Media::MJPEG,  buf, 0) { }
H264   ::H264  (const array &buf) : MediaBuffer(Media::H264,   buf, 0) { }

struct iaudio {
    ion::i32    sample_rate; /// hz
    ion::i16   *samples;
    ion::i8     channels;
    ion::i64    total_samples;
    ion::str    artist;
    ion::str    title;
    
    static i16 *alloc_pcm(int samples, int channels) {
        const int pad = 2048; /// mp3/aac never goes over this size
        sz_t sz = samples * channels + pad;
        i16 *pcm = new i16[sz];
        memset(pcm, 0, sz * sizeof(i16));
        return pcm;
    }

    ~iaudio()       { delete[] samples; }
    operator bool() { return samples; }
};

mx inflate(mx input) {
    size_t sz = input.byte_len();
    array out(typeof(char), sz * 4);
    
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

    u8 *src = input.get<u8>(0);
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
            console.fault("inflate failure: {0}", { res });
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
mx_implement(audio, mx, iaudio);

void audio::convert_mono() {
    if (data->channels == 1)
        return;
    i16     *samples = iaudio::alloc_pcm(data->total_samples, 1); /// now its 1:1 with total_samples because new channels = 1
    for (int       i = 0; i < data->total_samples; i++) {
        float    sum = 0;
        for (int   c = 0; c < data->channels; c++)
                sum += data->samples[i * data->channels + c];
        samples[i] = sum / data->channels;
    }
    memset(&samples[data->total_samples], 0, 1024 * sizeof(short));
    delete[] data->samples;  /// delete previous data
    data->samples  = samples; /// set new data
    data->channels = 1;        /// set to 1 channel
}

/// save / export to destination format (.mp3 .wav support -- todo: add aac here (impl in Stream))
bool audio::save(path dest, i64 bitrate) {
    str  ext    = dest.ext();
    cstr s_path = dest.cs();
    
    if (ext == ".mp3") {
        #if 0
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
        #endif
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
    } else if (ext == ".m4a" || ext == ".mp4") {
        return audio::save_m4a(dest, bitrate);
    }
    return true;
}

audio::audio(MediaBuffer buf) : audio() {
    /// buf has all info it needs its pcm delegate; that must exist
    assert(buf && buf->pcm);
    assert(buf->type == Media::PCM); /// must be short-based
}

audio audio::random_noise(i64 millis, int channels, int sample_rate) {
    int   total_samples = sample_rate * millis / 1000;
    audio res;
    res->total_samples  = total_samples;
    res->channels       = channels;
    res->sample_rate    = sample_rate;
    res->samples        = iaudio::alloc_pcm(total_samples, channels);

    i16  min = -32767;
    i16  max =  32767;
    for (sz_t i = 0; i < total_samples * channels; i += channels) {
        for (sz_t ii = 0; ii < channels; ii++)
            res->samples[i + ii] = rand::uniform(min, max);
    }
    return res;
}

audio::audio(path res, bool force_mono) : audio() {
    str  ext = res.ext();
    cstr s_path = res.cs();
    ///
    if (ext == ".mp3") {
        /// open file
        mp3dec_t         dec;
        mp3dec_ex_t      api;
        mp3dec_init(&dec);
        bool success = !mp3dec_ex_open(&api, s_path, MP3D_SEEK_TO_SAMPLE);
        assert(success);
        data->total_samples = api.samples / data->channels;
        data->channels      = api.info.channels;
        data->sample_rate   = api.info.hz;
        
        /// read samples
        data->samples       = iaudio::alloc_pcm(data->total_samples, data->channels);
        bool read = mp3dec_ex_read(&api, data->samples, api.samples) == data->total_samples * data->channels;
        assert(read);
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
        data->samples       = iaudio::alloc_pcm(data->total_samples, data->channels);
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
array audio::pcm_data() {
    return mx::wrap<short>(data->samples, data->total_samples * data->channels);
}

int           audio::channels() { return  data->channels;                    }
audio::operator          bool() { return  data->total_samples >  0;          }
bool         audio::operator!() { return  data->total_samples == 0;          }
sz_t              audio::size() { return  data->total_samples * data->channels; }
sz_t         audio::mono_size() { return  data->total_samples;               }
sz_t     audio::total_samples() { return  data->total_samples;               }
sz_t       audio::sample_rate() { return  data->sample_rate; }
i16 *          audio::samples() { return  data->samples; }
}
