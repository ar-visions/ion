
#include <media/camera-win.hpp>

//#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>
#include <wchar.h>

namespace ion {

Media video_format_from_guid(GUID &subtype) {
    if (subtype == MFVideoFormat_MJPG) return Media::MJPEG;
    if (subtype == MFVideoFormat_YUY2) return Media::YUY2;
    if (subtype == MFVideoFormat_NV12) return Media::NV12;
    if (subtype == MFVideoFormat_H264) return Media::H264;
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

Streams camera(array<StreamType> stream_types, array<Media> priority, str alias, int rwidth, int rheight) {
    return Streams(stream_types, priority, [stream_types, priority, alias, rwidth, rheight](Streams s) -> void {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        assert(!FAILED(hr));

        static bool has_init = false;
        if (!has_init) {
            MFStartup(MF_VERSION);
            has_init = true;
        }

        IMFMediaSource* pSource         = null;
        IMFAttributes*  pConfig         = null;
        IMFActivate**   ppDevices       = null;
        UINT32          deviceCount     = 0;
        Media           selected_format = Media::undefined;
        int             format_index    = -1;
        int             selected_stream = -1;

        /// enum video capture devices, select one we have highest priority for
        MFCreateAttributes(&pConfig, 1);
        pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        MFEnumDeviceSources(pConfig, &ppDevices, &deviceCount);
        IMFMediaType* pType = null;

        /// using while so we can break early (format not found), or at end of statement
        for (int i = 0; i < deviceCount; i++) {
            u32 ulen = 0;
            
            ppDevices[i]->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &ulen);
            utf16 device_name { size_t(ulen + 1) };
            ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                (WCHAR*)device_name.data, ulen + 1, &ulen);
            cstr utf8_name  = device_name.to_utf8();
            bool name_match = true;
            if (alias && !strstr(utf8_name, alias.data))
                name_match = false;
            
            free(utf8_name);
            if (!name_match)
                continue;
            ppDevices[i]->ActivateObject(IID_PPV_ARGS(&pSource));

            /// find the index of media source we want to select
            DWORD stream_index   = 0;
            GUID  subtype        = GUID_NULL;
            IMFSourceReader* pReader = null;
            MFCreateSourceReaderFromMediaSource(pSource, null, &pReader);
            while (SUCCEEDED(pReader->GetNativeMediaType(stream_index, 0, &pType))) {
                pType->GetGUID(MF_MT_SUBTYPE, &subtype);
                Media vf = video_format_from_guid(subtype);
                if (vf != Media::undefined) {
                    int index = priority.index_of(vf);
                    if (index >= 0 && (index < format_index || format_index == -1)) {
                        selected_format = vf;
                        format_index    = index;
                        selected_stream = stream_index;
                    }
                }
                pType->Release();
                pType = null;
                stream_index++;
            }
            
            if (!selected_format)
                continue;
            
            /// load selected_stream (index of MediaFoundation device stream)
            if (SUCCEEDED(pReader->GetNativeMediaType(selected_stream, 0, &pType))) {
                // set resolution; asset success
                HRESULT hr = MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, rwidth, rheight);
                assert(SUCCEEDED(hr));
                pReader->SetCurrentMediaType(selected_stream, null, pType);
            }

            if (pType) {
                struct Routine {
                    Media media;
                    DWORD dwStreamIndex;
                };
                doubly<Routine> routines;
                if (s->use_video)
                    routines += { selected_format, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM };
                if (s->use_audio)
                    routines += { Media::PCM,      (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM };

                /// get frame size; verify its the same as requested after we set it
                u32     width = 0, height = 0;
                assert(SUCCEEDED(MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height)));
                assert(rwidth == width && rheight == height);
                wprintf(L"width: %u, height: %u\n", width, height);

                /// get frame rate (hz) [hz = numerator / denominator; should probably store this in Streams]
                u32    numerator    = 0;
                u32    denominator  = 0;
                MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &numerator, &denominator);
                u32    hz           = numerator / denominator;
                s.set_info(width, height, hz);
                s.ready();

                /// capture loop
                while (!s->rt->stop) {
                    DWORD flags;
                    for (Routine &r: routines) {
                        IMFSample* pSample = null;
                        pReader->ReadSample(r.dwStreamIndex, 0, null, &flags, null, &pSample);
                        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                            break;
                        
                        if (pSample) {
                            IMFMediaBuffer* pBuffer         = null;
                            BYTE*           pData           = null;
                            DWORD           maxLength       = 0, 
                                            currentLength   = 0;
                            pSample->ConvertToContiguousBuffer(&pBuffer);
                            pBuffer->Lock(&pData, &maxLength, &currentLength);
                            s.push(MediaBuffer(r.media, pData, currentLength));
                            pBuffer->Unlock();
                            pBuffer->Release();
                            pSample->Release();
                        }
                    }
                }
                pType->Release();
            }
            pReader->Release();
            pSource->Release();
            ppDevices[i]->Release();
            CoTaskMemFree(ppDevices);
            break;
        }

        if (!selected_format)
            console.log("camera: no suitable device/format found");
        
        pConfig->Release();
        MFShutdown();
        if (!s->ready)
            s.cancel();
    });
}

}