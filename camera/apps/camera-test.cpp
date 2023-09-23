
#include <stdio.h>

#if defined(__APPLE__)



#elif defined(__linux__)

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main() {
    usleep(10000000);
    
    int test =0;
    test++;
    printf("camera-test!\n");
    int fd = open("/dev/video2", O_RDWR);
    if (fd == -1) {
        perror("Cannot open device");
        return 1;
    }

    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = 640;  // Adjust as per your requirement
    fmt.fmt.pix.height      = 360;  // Adjust as per your requirement
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Setting Pixel Format");
        return 1;
    }

    FILE* file = fopen("output.h264", "wb");
    if (!file) {
        perror("Cannot open output file");
        return 1;
    }

    char buffer[8192];
    int bytesRead;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytesRead, file);
    }

    fclose(file);
    close(fd);

    return 0;
}

#else

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

#endif
