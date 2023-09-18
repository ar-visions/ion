
#include <iostream>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>

int main() {
// Initialize Media Foundation
    MFStartup(MF_VERSION);

    IMFMediaSource* pSource = nullptr;
    IMFAttributes* pConfig = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 deviceCount = 0;

    // Enumerate video capture devices
    MFCreateAttributes(&pConfig, 1);
    pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    MFEnumDeviceSources(pConfig, &ppDevices, &deviceCount);

    if (deviceCount == 0) {
        // No devices found
        return 1;
    }

    ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));

    IMFMediaType* pType = nullptr;
    IMFSourceReader* pReader = nullptr;
    MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader);

    DWORD streamIndex = 0;
    while (SUCCEEDED(pReader->GetNativeMediaType(streamIndex, 0, &pType))) {
        GUID subtype = GUID_NULL;
        pType->GetGUID(MF_MT_SUBTYPE, &subtype);

        WCHAR subtypeName[40]; // Allocate a buffer for the string
        StringFromGUID2(subtype, subtypeName, ARRAYSIZE(subtypeName));
        wprintf(L"Subtype: %s\n", subtypeName);

        if (subtype == MFVideoFormat_H264) {
            pReader->SetCurrentMediaType(streamIndex, NULL, pType);
            break;
        }
        pType->Release();
        pType = nullptr;
        streamIndex++;
    }

    if (!pType) {
        // H.264 format not found
        return 2;
    }

    // Capture loop
    while (true) {
        DWORD flags;
        IMFSample* pSample = nullptr;
        pReader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, nullptr, &flags, nullptr, &pSample);

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break; // end of stream
        }

        if (pSample) {
            // Process the H.264 sample
            // ... 

            pSample->Release();
        }
    }

    // Cleanup
    pType->Release();
    pReader->Release();
    pSource->Release();
    ppDevices[0]->Release();
    CoTaskMemFree(ppDevices);
    pConfig->Release();

    MFShutdown();
    return 0;
}
