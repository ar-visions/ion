
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



void test1() {
    IMFMediaSource* pSource = nullptr;
    IMFSourceReader* pReader = nullptr;
    IMFMediaType* pType = nullptr;
    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;

    // Initialize Media Foundation
    MFStartup(MF_VERSION);

    // Set up the attributes for the device enumerator: We're looking for video capture devices
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (SUCCEEDED(hr)) {
        hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    }
    // Create Media Source for the First Camera
    // This part of the code is simplified; normally, you would enumerate devices
    // and select the first camera.
    u32 count;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    if (SUCCEEDED(hr) && count > 0) {
        // Create the media source using the first device in the list.
        hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
    }

    // Create Source Reader
    hr = MFCreateSourceReaderFromMediaSource(pSource, nullptr, &pReader);

    // Set the desired media type (640x360, 30FPS)
    for (DWORD i = 0; ; ++i) {
        hr = pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pType);
        if (FAILED(hr)) { break; }

        UINT32 width = 0, height = 0;
        MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);

        UINT32 frameRateNum = 0, frameRateDenom = 0;
        MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &frameRateNum, &frameRateDenom);

        if (width == 640 && height == 360 && frameRateNum / frameRateDenom == 30) {
            hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType);
            break;
        }
        pType->Release();
    }

    // Check if suitable format found
    if (!pType) {
        // Handle error: Suitable format not found
    }

    // Read frames
    while (true) {

        i64 t1 = millis();

        DWORD streamIndex, flags;
        LONGLONG timestamp;
        IMFSample* pSample = nullptr;

        hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &pSample);
        
        if (FAILED(hr) || flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        if (pSample) {
            // Process the sample
            // ...

            pSample->Release();
        }

        i64 t2 = millis();
        printf("duration = %d\n", (int)(t2 - t1));
        fflush(stdout);
    }

    // Clean up
    pType->Release();
    pReader->Release();
    pSource->Release();
    MFShutdown();
}

MStream camera(array<StreamType> stream_types, array<Media> priority, str video_alias, str audio_alias, int rwidth, int rheight) {
    return MStream(stream_types, priority, [stream_types, priority, video_alias, audio_alias, rwidth, rheight](MStream s) -> void {

        test1();

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
            int                 selected_media;
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
                                printf("frame size: %d %d\n", width, height);
                                printf("frame rate: %d %d\n", frameRate, denominator);
                                fflush(stdout);
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
                    } else
                        selected_media = 0;

                    pType->GetGUID(MF_MT_SUBTYPE, &subtype);
                    Media vf = format_from_guid(subtype);
                    if (vf != Media::undefined) {
                        int index = priority.index_of(vf);
                        if (index >= 0 && (index < r->format_index || r->format_index == -1)) {
                            r->selected_format = vf;
                            r->format_index    = index;
                            r->selected_stream = stream_index;
                            r->selected_media  = selected_media;
                        }
                    }
                    pType->Release();
                    pType = null;
                    stream_index++;
                }

                assert(r->selected_format);
                /// load selected_stream (index of MediaFoundation device stream)
                if (SUCCEEDED(r->pReader->GetNativeMediaType(r->selected_stream, r->selected_media, &r->pType))) {
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

        s.set_info(rwidth, rheight, 30, 1);
        s.ready();

        /// capture loop
        Routine *r = routines[0];
        r->pReader->SetCurrentMediaType(r->dwStreamIndex, NULL, r->pType);

        while (!s->rt->stop) {
            DWORD flags = 0;
            i64 t = millis();
            for (Routine *r: routines) {
                IMFSample* pSample = null;
                r->pReader->ReadSample(r->dwStreamIndex, 0, null, &flags, null, &pSample);
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                    break;
                if (pSample) {
                    IMFMediaBuffer* pBuffer         = null;
                    BYTE*           pData           = null;
                    DWORD           maxLength       = 0, 
                                    currentLength   = 0;
                    //pSample->ConvertToContiguousBuffer(&pBuffer);
                    //pBuffer->Lock(&pData, &maxLength, &currentLength);
                    //s.push(
                    //    MediaBuffer(r->selected_format, pData, currentLength)
                    //);
                    //pBuffer->Unlock();
                    //pBuffer->Release();
                    pSample->Release();
                }
            }
            i64 t2 = millis();
            i64 t3 = t2 - t;
            printf("duration = %d\n", (int)t3);
            fflush(stdout);
        }
        
        MFShutdown();
        if (!s->ready)
            s.cancel();
    });
}

}