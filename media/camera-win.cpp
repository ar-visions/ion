
#include <media/camera.hpp>

//#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>
#include <wchar.h>

namespace ion {

Media format_from_guid(GUID &subtype) {
    if (subtype == MFVideoFormat_MJPG)  return Media::MJPEG;
    if (subtype == MFVideoFormat_YUY2)  return Media::YUY2;
    if (subtype == MFVideoFormat_NV12)  return Media::NV12;
    if (subtype == MFVideoFormat_H264)  return Media::H264;
    if (subtype == MFAudioFormat_PCM)   return Media::PCM;
    if (subtype == MFAudioFormat_Float) return Media::PCMf32;
    return Media::undefined;
}

struct GuidHash {
    size_t operator()(const GUID& guid) const {
        const uint64_t* p = reinterpret_cast<const uint64_t*>(&guid);
        std::hash<uint64_t> hash;
        return hash(p[0]) ^ hash(p[1]);
    }
};

struct GuidEqual {
    bool operator()(const GUID& lhs, const GUID& rhs) const {
        return (memcmp(&lhs, &rhs, sizeof(GUID)) == 0);
    }
};

MStream camera(Array<StreamType> stream_types, Array<Media> priority, str video_alias, str audio_alias, int rwidth, int rheight) {
    return MStream(stream_types, priority, [stream_types, priority, video_alias, audio_alias, rwidth, rheight](MStream s) -> void {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        assert(!FAILED(hr));

        static bool has_init = false;
        if (!has_init) {
            MFStartup(MF_VERSION);
            has_init = true;
        }

        struct Routine {
            bool                video;
            IMFAttributes*      pConfig;
            IMFMediaSource*     pSource;
            IMFActivate*        pDevice;
            IMFSourceReader*    pReader;
            IMFMediaType*       pType;
            DWORD               dwStreamIndex;
            int                 selected_stream;
            int                 format_index = -1;
            int                 packet_id;
            Media               selected_format;
            int                 selected_media;
            bool                name_match;
            PCMInfo             pcm;
        };

        struct MediaCategory {
            str                 alias;
            IMFActivate**       ppDevices;
            u32                 deviceCount;
            IMFAttributes*      pConfig;
            GUID               &guid;
            DWORD               dwStreamIndex;
        };

        doubly routines;

        /// enum video capture devices, select one we have highest priority for
        MediaCategory search[2] = {
            { video_alias, null, 0, null, (GUID&)MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM },
            { audio_alias, null, 0, null, (GUID&)MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM }
        };
        /// Video & Audio device enumeration
        for (int cat = 0; cat < 2; cat++) {
            if (cat == 0 && !s->use_video)
                continue;
            if (cat == 1 && !s->use_audio)
                continue;
            MediaCategory &mcat = search[cat];
            MFCreateAttributes(&mcat.pConfig, 1);
            mcat.pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, mcat.guid);
            MFEnumDeviceSources(mcat.pConfig, &mcat.ppDevices, &mcat.deviceCount);
            ///
            for (int i = 0; i < mcat.deviceCount; i++) {
                u32 ulen = 0;
                mcat.ppDevices[i]->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &ulen);
                utf16 device_name { size_t(ulen + 1) };
                mcat.ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                    (WCHAR*)device_name.data, ulen + 1, &ulen);
                cstr utf8_name  = device_name.to_utf8();
                bool name_match = !!strstr(utf8_name, mcat.alias.data);
                free(utf8_name);

                Routine *r = new Routine {
                    .pConfig = mcat.pConfig,
                    .pDevice = mcat.ppDevices[i],
                    .dwStreamIndex = mcat.dwStreamIndex
                };
                r->pDevice->ActivateObject(IID_PPV_ARGS(&r->pSource));

                /// find the index of media source we want to select for this category
                DWORD stream_index  = 0;
                GUID  subtype       = GUID_NULL;
                IMFMediaType* pType = null;
                MFCreateSourceReaderFromMediaSource(r->pSource, null, &r->pReader);

                while (SUCCEEDED(r->pReader->GetNativeMediaType(stream_index, 0, &pType))) {
                    int selected_media;
                    if (cat == 0) {
                        selected_media = -1;
                        IMFMediaType* mediaType = nullptr;
                        GUID    subtype { 0 };
                        UINT32  frameRate    = 0u;
                        UINT32  denominator  = 0u;
                        DWORD   index        = 0u;
                        UINT32  width        = 0u;
                        UINT32  height       = 0u;
                        HRESULT hr = S_OK;
                        while (hr == S_OK) {
                            hr = r->pReader->GetNativeMediaType(stream_index, index, &mediaType);
                            if (hr != S_OK)
                                break;
                            hr = mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
                            hr = MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);
                            hr = MFGetAttributeRatio(mediaType, MF_MT_FRAME_RATE, &frameRate, &denominator);
                            if (hr == S_OK) {
                                if (width == rwidth && height == rheight && frameRate == 30) {
                                    selected_media = index;
                                    mediaType->Release();
                                    break;
                                }
                            }
                            mediaType->Release();
                            ++index;
                        }
                        if (selected_media == -1) {
                            pType->Release();
                            pType = null;
                            stream_index++;
                            continue;
                        }
                    } else {
                        selected_media = 0;
                    }

                    pType->GetGUID(MF_MT_SUBTYPE, &subtype);
                    Media vf = format_from_guid(subtype);
                    if (vf != Media::undefined) {
                        int index = priority.index_of(vf);
                        if (index >= 0 && (
                                (index < r->format_index) || 
                                (r->format_index == -1)   ||
                                (name_match && !r->name_match))) {
                            
                            r->video           = (cat == 0);
                            r->selected_format = vf;
                            r->format_index    = index;
                            r->selected_stream = stream_index;
                            r->selected_media  = selected_media;
                            r->name_match      = name_match;
                        }
                    }
                    pType->Release();
                    pType = null;
                    stream_index++;
                }

                assert(r->selected_format.value);

                /// load selected_stream (index of MediaFoundation device stream)
                /// routine must know the specs of the stream so that it may perform conversion
                /// todo: MStream should do so; (simplify this implementation for 1 stream type per request)
                r->pReader->GetNativeMediaType(r->selected_stream, r->selected_media, &r->pType);
                if (cat == 1) {


                    // Create a new media type for 16-bit PCM audio
                    // when we load the default device, its distorted f32; its rate seems fine but its quite distorted; its also not BigEndian or anything
                    IMFMediaType* pNewType = NULL;
                    MFCreateMediaType(&pNewType);
                    pNewType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                    pNewType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                    pNewType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                    pNewType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000); // or set your desired sample rate
                    pNewType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1); // or set your desired channel count

                    r->selected_format = Media::PCM;

                    // Set the new media type on the source reader
                    hr = r->pReader->SetCurrentMediaType(r->selected_stream, NULL, pNewType);
                    assert(SUCCEEDED(hr));

                    r->pType = pNewType;


                    GUID audio_type;
                    hr = r->pType->GetGUID(MF_MT_SUBTYPE, &audio_type);
                    assert(SUCCEEDED(hr));

                    Media format;
                    u32 samples_per_second;
                    u32 channels;
                    u32 bits_per_sample;

                    hr = r->pType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samples_per_second);
                    assert(SUCCEEDED(hr));
                    hr = r->pType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
                    assert(SUCCEEDED(hr));
                    hr = r->pType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits_per_sample);
                    assert(SUCCEEDED(hr));

                    if (bits_per_sample == 32) {
                        if (audio_type == MFAudioFormat_Float) {
                            format = Media::PCMf32; // sample units are 32-bit floating-point
                        } else if (audio_type == MFAudioFormat_PCM) {
                            format = Media::PCMu32; // sample units are 32-bit integer
                        } else {
                            assert(false);
                        }
                    } else if (bits_per_sample == 16) {
                        if (audio_type == MFAudioFormat_PCM) {
                            format = Media::PCM; // sample units are 16-bit floating-point
                        } else {
                            assert(false);
                        }
                    } else {
                        assert(false);
                    }
                    
                    /// MStreams converts this stream of pcm-whatever to 16bit short in the channel count
                    /// todo: remove pcm info from this module, 
                    /// as it will be used cross platform 
                    /// (no need to do anything different in each driver)
                    s.init_output_pcm(Media::PCM, 1, 48000 / 30);
                    s.init_input_pcm (format, channels, samples_per_second / 30);
                }
                routines.push(r);
            }
        }

        assert(routines->len() > 0);

        s.set_info(rwidth, rheight, 30, 1);
        s.ready();

        /// capture loop
        Routine *r_video = routines.get<Routine*>(0);
        s.set_video_format(r_video->selected_format);

        assert(r_video->selected_format);

        for (Routine *r: routines.elements<Routine*>())
            if (r_video->selected_format != Media::undefined)
                async(1, [r, s](runtime *rt, int i) -> mx {
                    MStream &ms = (MStream &)s;
                    r->pReader->SetCurrentMediaType(r->selected_stream, NULL, r->pType);
                    while (!ms->rt->stop) {
                        DWORD flags = 0;
                        IMFSample* pSample = null;
                        r->pReader->ReadSample(r->selected_stream, 0, null, &flags, null, &pSample);
                        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                            break;
                        ///
                        if (pSample) {
                            IMFMediaBuffer* mbuffer         = null;
                            BYTE*           data            = null;
                            DWORD           maxLength       = 0, 
                                            data_len        = 0;
                            pSample->ConvertToContiguousBuffer(&mbuffer);
                            mbuffer->Lock(&data, &maxLength, &data_len);
                            
                            PCMInfo &pcm = r->pcm;
                            if (!r->video) {
                                if (r->selected_format == Media::PCMf32) {
                                    assert(data_len % 4 == 0);
                                    Array<float> audio(sz_t(data_len) / 4);
                                    memcpy(audio.data, data, data_len / 4);
                                    audio.set_size(data_len / 4);
                                    ms.push_audio(audio);
                                } else {
                                    FILE *f = fopen("test2.pcm", "ab");
                                    fwrite(data, 1, data_len, f);
                                    fclose(f);
                                    
                                    assert(data_len % 2 == 0);
                                    Array<short> audio(sz_t(data_len) / 2);
                                    memcpy(audio.data, data, data_len / 2);
                                    audio.set_size(data_len / 2);
                                    ms.push_audio(audio);
                                }
                            } else {
                                /// video-based method is to simply send the nalu frames received
                                Array<u8> video = Array<u8>(sz_t(data_len));
                                memcpy(video.data, data, data_len);
                                video.set_size(data_len);
                                ms.push_video(video);
                            }

                            mbuffer->Unlock();
                            mbuffer->Release();
                            pSample->Release();
                        }
                    }
                    return true;
                });

        while (!s->rt->stop) {
            usleep(1000);
        }
        
        MFShutdown();
        if (!s->ready)
            s.cancel();
    });
}

}