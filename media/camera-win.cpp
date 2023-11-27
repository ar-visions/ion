
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

Streams camera(array<StreamType> stream_types, array<Media> priority, str video_alias, str audio_alias, int rwidth, int rheight) {
    return Streams(stream_types, priority, [stream_types, priority, video_alias, audio_alias, rwidth, rheight](Streams s) -> void {

        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        assert(!FAILED(hr));

        static bool has_init = false;
        if (!has_init) {
            MFStartup(MF_VERSION);
            has_init = true;
        }

        struct Routine {
            IMFAttributes*      pConfig;
            IMFMediaSource*     pSource;
            IMFActivate*        pDevice;
            IMFSourceReader*    pReader;
            IMFMediaType*       pType;
            DWORD               dwStreamIndex;
            int                 selected_stream;
            int                 format_index = -1;
            Media               selected_format;
        };

        doubly<Routine*> routines;

        struct MediaCategory {
            str             alias;
            IMFActivate**   ppDevices;
            u32             deviceCount;
            IMFAttributes*  pConfig;
            GUID           &guid;
            DWORD           dwStreamIndex;
        };

        /// enum video capture devices, select one we have highest priority for
        MediaCategory search[2] = {
            { video_alias, null, 0, null, (GUID&)MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM },
            { audio_alias, null, 0, null, (GUID&)MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM }
        };

        for (int cat = 0; cat < 2; cat++) {
            if (cat == 0 && !s->use_video)
                continue;
            if (cat == 1 && !s->use_audio)
                continue;
            MediaCategory &mcat = search[cat];
            MFCreateAttributes(&mcat.pConfig, 1);
            mcat.pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, mcat.guid);
            MFEnumDeviceSources(mcat.pConfig, &mcat.ppDevices, &mcat.deviceCount);
            
            /// using while so we can break early (format not found), or at end of statement
            for (int i = 0; i < mcat.deviceCount; i++) {
                u32 ulen = 0;
                mcat.ppDevices[i]->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &ulen);
                utf16 device_name { size_t(ulen + 1) };
                mcat.ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                    (WCHAR*)device_name.data, ulen + 1, &ulen);
                cstr utf8_name  = device_name.to_utf8();
                bool name_match = !!strstr(utf8_name, mcat.alias.data);

                free(utf8_name);
                if (!name_match)
                    continue;

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
                    pType->GetGUID(MF_MT_SUBTYPE, &subtype);
                    Media vf = format_from_guid(subtype);
                    if (vf != Media::undefined) {
                        int index = priority.index_of(vf);
                        if (index >= 0 && (index < r->format_index || r->format_index == -1)) {
                            r->selected_format = vf;
                            r->format_index    = index;
                            r->selected_stream = stream_index;
                        }
                    }
                    pType->Release();
                    pType = null;
                    stream_index++;
                }

                assert(r->selected_format);
                
                /// load selected_stream (index of MediaFoundation device stream)
                if (SUCCEEDED(r->pReader->GetNativeMediaType(r->selected_stream, 0, &r->pType))) {
                    if (cat == 0) { 
                        // set resolution; asset success
                        HRESULT hr = MFSetAttributeSize(r->pType, MF_MT_FRAME_SIZE, rwidth, rheight);
                        assert(SUCCEEDED(hr));
                    }
                    HRESULT hr = r->pReader->SetCurrentMediaType(r->selected_stream, null, r->pType);
                    assert(SUCCEEDED(hr));
                    if (cat == 1) {
                        IMFSample* pSample = null;
                        DWORD flags = 0;
                        HRESULT    hr = r->pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                            0, NULL, &flags, NULL, &pSample);
                        pSample = pSample;
                    }
                }
                routines += r;
            }
        }

        for (Routine *r: routines) {
            if (r->dwStreamIndex != (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM)
                continue;
            /// get frame rate (hz) [hz = numerator / denominator; should probably store this in Streams]
            u32    numerator    = 0;
            u32    denominator  = 0;
            MFGetAttributeRatio(r->pType, MF_MT_FRAME_RATE, &numerator, &denominator);
            u32    hz           = numerator / denominator;
            s.set_info(rwidth, rheight, hz);
        }

        s.ready();

        /// capture loop
        while (!s->rt->stop) {
            DWORD flags = 0;
            for (Routine *r: routines) {
                IMFSample* pSample = null;
                r->pReader->SetCurrentMediaType(r->dwStreamIndex, NULL, r->pType);
                r->pReader->ReadSample(r->dwStreamIndex, 0, null, &flags, null, &pSample);
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                    break;
                if (pSample) {
                    IMFMediaBuffer* pBuffer         = null;
                    BYTE*           pData           = null;
                    DWORD           maxLength       = 0, 
                                    currentLength   = 0;
                    pSample->ConvertToContiguousBuffer(&pBuffer);
                    pBuffer->Lock(&pData, &maxLength, &currentLength);
                    s.push(
                        MediaBuffer(r->selected_format, pData, currentLength)
                    );
                    pBuffer->Unlock();
                    pBuffer->Release();
                    pSample->Release();
                }
            }
        }
        
        MFShutdown();
        if (!s->ready)
            s.cancel();
    });
}

}