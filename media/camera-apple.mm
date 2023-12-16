#import "camera-apple.h"

#include <media/media.hpp>
#include <media/streams.hpp>
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>

using CaptureCallback = ion::lambda<void(void*, ion::sz_t)>;

#define kNumberBuffers 3

struct audio_t {
    CaptureCallback     callback;
    ion::str            audio_alias;
    int                 selected = -1;
    ion::Media          selected_format;
    int                 sample_rate;
    bool                halt;
    bool                shutdown;
    AudioQueueBufferRef buffers[kNumberBuffers];
    AudioQueueRef       queue;
    UInt32              bufferByteSize;
    SInt64              currentPacket;

    bool select(ion::str &audio_alias) {
        this->audio_alias = audio_alias;

        OSStatus        status;
        AudioObjectID  *deviceIDs = NULL;
        UInt32          dataSize  = 0;
        UInt32          propertySize;

        // Get the size of the data to be returned
        AudioObjectPropertyAddress propertyAddress = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize);
        if (status != kAudioHardwareNoError) {
            fprintf(stderr, "Error getting data size for audio devices\n");
            return -1;
        }

        // Calculate the number of devices and allocate memory
        UInt32 deviceCount = dataSize / sizeof(AudioObjectID);
        deviceIDs = (AudioObjectID *)malloc(dataSize);

        // Get the audio devices
        status = AudioObjectGetPropertyData(
            kAudioObjectSystemObject, &propertyAddress, 
            0, NULL, &dataSize, deviceIDs);
        
        if (status != kAudioHardwareNoError) {
            fprintf(stderr, "Error getting audio devices\n");
            free(deviceIDs);
            return -1;
        }

        // Iterate over each device and print its name
        for (UInt32 i = 0; selected == -1 && i < deviceCount; ++i) {
            CFStringRef deviceName = NULL;
            propertySize = sizeof(deviceName);

            /// filter by mics (needs a number of channels in input)
            bool is_mic = false;
            AudioObjectPropertyAddress propertyAddressMic = {
                kAudioDevicePropertyStreamConfiguration,
                kAudioDevicePropertyScopeInput,
                kAudioObjectPropertyElementMaster
            };
            status = AudioObjectGetPropertyDataSize(deviceIDs[i], &propertyAddressMic, 0, NULL, &dataSize);
            if (status == kAudioHardwareNoError && dataSize > 0) {
                AudioBufferList *bufferList = (AudioBufferList *)malloc(dataSize);
                status = AudioObjectGetPropertyData(deviceIDs[i], &propertyAddressMic, 0, NULL, &dataSize, bufferList);
                if (status == kAudioHardwareNoError) {
                    int channelCount = 0;
                    for (UInt32 j = 0; j < bufferList->mNumberBuffers; j++)
                        channelCount += bufferList->mBuffers[j].mNumberChannels;
                    if (channelCount > 0)
                        is_mic = true;
                }
                free(bufferList);
            }
            if (!is_mic)
                continue;
            propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
            status = AudioObjectGetPropertyData(deviceIDs[i], &propertyAddress, 0, NULL, &propertySize, &deviceName);
            if (status == kAudioHardwareNoError && deviceName) {
                char name[256];
                if (CFStringGetCString(deviceName, name, sizeof(name), kCFStringEncodingUTF8)) {
                    printf("microphone %u: %s\n", (unsigned int)i, name);
                    if (selected == -1 || !audio_alias || strstr(name, audio_alias.data))
                        selected = (int)deviceIDs[i];
                }
                CFRelease(deviceName);
            }
        }

        free(deviceIDs);
        return selected >= 0;
    }

    void stop() {
        if (shutdown)
            return;
        halt = true;

        /// stop and cleanup
        printf("Stopping recording...\n");
        AudioQueueStop(queue, true);

        /// dispose of the audio queue and buffers
        for (int i = 0; i < kNumberBuffers; ++i)
            AudioQueueFreeBuffer(queue, buffers[i]);
        
        AudioQueueDispose(queue, true);
    }

    static void AudioInputCallback(
            void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer,
            const AudioTimeStamp *inStartTime, UInt32 inNumPackets,
            const AudioStreamPacketDescription *inPacketDesc) {
        audio_t *audio = (audio_t*)inUserData;

        if (inNumPackets > 0) {
            void  *src = inBuffer->mAudioData;
            UInt32 sz  = inBuffer->mAudioDataByteSize;
            audio->callback(src, sz);
            ///
            audio->currentPacket += inNumPackets;
        }

        /// Re-enqueue the buffer if recording is still in progress
        if (!audio->halt && !audio->shutdown) {
            AudioQueueEnqueueBuffer(audio->queue, inBuffer, 0, NULL);
        }
    }

    ion::async start() {
        return ion::async(1, [this](ion::runtime* rt, int i) -> ion::mx {

            AudioStreamBasicDescription dataFormat;
            dataFormat.mSampleRate          = sample_rate;
            dataFormat.mFormatID            = kAudioFormatLinearPCM;
            dataFormat.mFormatFlags         = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
            dataFormat.mFramesPerPacket     = 1;
            dataFormat.mChannelsPerFrame    = 1; // Mono; use 2 for stereo
            dataFormat.mBitsPerChannel      = 32;
            dataFormat.mBytesPerPacket      = dataFormat.mBytesPerFrame = 
                (dataFormat.mBitsPerChannel / 8) * dataFormat.mChannelsPerFrame;

            // Create the audio queue
            AudioQueueNewInput(&dataFormat, AudioInputCallback, (void*)this, NULL,
                kCFRunLoopCommonModes, 0, &queue);

            // Determine the buffer size
            UInt32 maxPacketSize;
            UInt32 size = sizeof(maxPacketSize);
            AudioQueueGetProperty(queue,
                kAudioQueueProperty_MaximumOutputPacketSize, &maxPacketSize, &size);

            // Calculate the buffer size
            bufferByteSize = 2048; // Example buffer size; adjust as necessary
            currentPacket  = 0;

            for (int i = 0; i < kNumberBuffers; ++i) {
                AudioQueueAllocateBuffer(queue, bufferByteSize, &buffers[i]);
                AudioQueueEnqueueBuffer(queue, buffers[i], 0, NULL);
            }

            // Start the audio queue
            halt = false;
            AudioQueueStart(queue, NULL);

            // Run loop to keep the application running while recording
            printf("Recording...\n");
            while (!halt) {
                CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.25, false);
            }
            return true;
        });
    }
};

struct camera_t {
    CaptureCallback callback;
    ion::str        video_alias;
    int             width        = 640;
    int             height       = 360;
    int             rate         = 30;
    bool            halt         = false;
    AppleCapture   *capture;
    ion::array<ion::Media> priority;
    ion::Media      selected_format;
};

extern "C" void AllowKeyRepeats(void)
{
    @autoreleasepool {
        [[NSUserDefaults standardUserDefaults] setBool: NO forKey: @"ApplePressAndHoldEnabled"];
    }
}

@implementation AppleCapture

- (AVCaptureDeviceFormat *)filterDevice:(AVCaptureDevice *)device format:(FourCharCode)f {
    for (AVCaptureDeviceFormat *format in [device formats]) {
        CMFormatDescriptionRef formatDescription = format.formatDescription;
        FourCharCode codecType = CMFormatDescriptionGetMediaSubType(formatDescription);
        if (codecType == f) {
            return format;
        }
    }
    return nil;
}

- (void)stop {
    [self.captureSession stopRunning];
}

CMVideoCodecType codec_type(ion::Media &m) {
    switch (m.value) {
        case ion::Media::MJPEG: return kCMVideoCodecType_JPEG;
        case ion::Media::YUY2:  
            printf("found format\n");
            return kCVPixelFormatType_422YpCbCr8;
        default: return 0;
    }
}

- (void)start:(camera_t*)camera {
    self.captureSession = [[AVCaptureSession alloc] init];
    self.captureSession.sessionPreset = AVCaptureSessionPresetHigh;
    self.camera = camera;

    NSArray<AVCaptureDevice *> *devices = [AVCaptureDevice devices];
    int current_index = 0;

    bool single = false;
    ion::str filter = self.camera->video_alias;
    if (filter)
        for (AVCaptureDevice *device in devices) {
            if ([device hasMediaType:AVMediaTypeVideo] && [device supportsAVCaptureSessionPreset:AVCaptureSessionPresetHigh]) {
                NSString   *device_name = [device localizedName];
                const char *device_cs   = [device_name UTF8String];
                bool        use_device  = strstr(device_cs, filter.data) != NULL;
                printf("filter.data = %s\n", filter.data);
                if (!use_device)
                    continue;
                printf("device cs = %s\n", device_cs);
                single = true;
            }
        }
    
    if (!single) {
        printf("setting filter to null\n");
        filter = ion::null; /// lets just choose one for god sakes. the mac is not a common target and i dont want to change the query device.  lets pick teh first usable one
    }
    /// one massive issue with this api is multiple cameras with the same name will shuffle from run to run. LOL.
    printf("devices count %d\n", (int)[devices count]);
    for (AVCaptureDevice *device in devices) {
        if ([device hasMediaType:AVMediaTypeVideo] && [device supportsAVCaptureSessionPreset:AVCaptureSessionPresetHigh]) {
            
            
            printf("1\n");
            CMVideoCodecType found = 0;
            for (ion::Media &m: camera->priority) {
                CMVideoCodecType t = codec_type(m);
                if (!found)
                    found = t;
            }
            if (!found)
                continue;
            
            printf("2\n");
            NSString   *device_name = [device localizedName];
            const char *device_cs   = [device_name UTF8String];
            bool        use_device  = !filter || strstr(device_cs, filter.data) != NULL;

            printf("why?\n");
            if (!use_device)
                continue;

            printf("using device %s\n", device_cs);

            NSError *error;
            AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
            if (error) {
                fprintf(stderr, "failed to obtain capture device\n");
                exit(1);
            }
            if ([self.captureSession canAddInput:input]) {
                [self.captureSession addInput:input];
            }

            self.videoOutput = [[AVCaptureVideoDataOutput alloc] init];
            [self.videoOutput setSampleBufferDelegate:self queue:dispatch_queue_create("videoQueue", DISPATCH_QUEUE_SERIAL)];

            if ([self.captureSession canAddOutput:self.videoOutput]) {
                [self.captureSession addOutput:self.videoOutput];
            }

            [self.captureSession startRunning];
            printf("started capture session\n");
            break;
        }
    }
}

#pragma mark - AVCaptureVideoDataOutputSampleBufferDelegate

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection {
    /// Extracting the CMBlockBuffer from the CMSampleBuffer
    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    exit(0);
    if (blockBuffer == NULL) {
        self.camera->halt = true;
        return; /// Handle the error or exit the function
    }
    
    /// Getting a pointer to the raw data
    size_t lengthAtOffset;
    size_t totalLength;
    char  *dataPointer;
    CMBlockBufferGetDataPointer(
        blockBuffer, 0, &lengthAtOffset, 
        &totalLength, &dataPointer);
    self.camera->callback((void*)dataPointer, totalLength);
}

@end


#include <media/camera.hpp>
#ifdef __APPLE__
#import <media/camera-apple.h>
#endif
#include <memory>

using namespace ion;

namespace ion {

MStream camera(array<StreamType> stream_types, array<Media> priority, str video_alias, str audio_alias,
               int width, int height) {

    return MStream(stream_types, priority,
            [stream_types, priority, video_alias, audio_alias, width, height]
    (MStream s) -> void {

        /// setup audio
        audio_t  audio { };
        assert(audio.select((str&)audio_alias));
        audio.selected_format = Media::PCMf32;
        audio.sample_rate     = 48000;
        audio.current_id      = 0;
        audio.callback        = [&](void* v_audio, sz_t v_audio_size) {
            MStream  &streams = (MStream &)s;
            array<float> packet(v_audio_size / sizeof(float));
            memcpy(packet.data, v_audio, v_audio_size);
            packet.set_size(v_audio_size / sizeof(float));
            streams.push(MediaBuffer(audio.selected_format, packet, audio.current_id++));

        };
        audio.start();

        /// setup camera
        camera_t camera { };
        camera.capture  = [AppleCapture new];
        camera.video_alias = video_alias;
        camera.width    = width;
        camera.height   = height;
        camera.priority = priority;
        camera.rate     = 30;
        i64 last = millis();
        camera.callback = [&](void* v_image, sz_t v_image_size) {
            MStream &streams = (MStream &)s;
            streams.push(MediaBuffer(camera.selected_format, (u8*)v_image, v_image_size));
            i64 t = millis();
            printf("duration = %d\n", (int)(t - last));
            exit(0);
            last = t;
        };
        [camera.capture start:&camera];

        printf("started camera...\n");
        while (!s->rt->stop) {
            if (camera.halt)
                break;
        }

        [camera.capture stop];
        audio.stop();
    });

}

}