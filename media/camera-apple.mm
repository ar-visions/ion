#import "apple.h"

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

- (void)start:(int)index width:(int)width height:(int)height user:(void*)user callback:(CaptureCallback)callback {
    self.captureSession = [[AVCaptureSession alloc] init];
    self.captureSession.sessionPreset = AVCaptureSessionPresetHigh;
    self.user = user;
    self.callback = callback;

    //NSArray<AVCaptureDeviceType> *deviceTypes = @[AVCaptureDeviceTypeExternalUnknown];
    //AVCaptureDeviceDiscoverySession *discoverySession = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:deviceTypes mediaType:AVMediaTypeVideo position:AVCaptureDevicePositionUnspecified];
    //NSArray<AVCaptureDevice *> *devices = discoverySession.devices;

    NSArray<AVCaptureDevice *> *devices = [AVCaptureDevice devices];

    int current_index = 0;

    printf("devices count %d\n", (int)[devices count]);
    for (AVCaptureDevice *device in devices) {
        if ([device hasMediaType:AVMediaTypeVideo] && [device supportsAVCaptureSessionPreset:AVCaptureSessionPresetHigh]) {
            
            /// check if device supports format specified
            if (![self filterDevice:device format:kCMVideoCodecType_JPEG])
                continue;
            
            bool use_device = current_index++ == index;
            if (!use_device)
                continue;

            NSError *error;
            AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
            if (error) {
                fprintf(stderr, "failed to obtain capture device\n");
                exit(1);
            }
            if ([self.captureSession canAddInput:input]) {
                [self.captureSession addInput:input];
            }

            exit(1);

            self.videoOutput = [[AVCaptureVideoDataOutput alloc] init];
            [self.videoOutput setSampleBufferDelegate:self queue:dispatch_queue_create("videoQueue", DISPATCH_QUEUE_SERIAL)];

            if ([self.captureSession canAddOutput:self.videoOutput]) {
                [self.captureSession addOutput:self.videoOutput];
            }

            [self.captureSession startRunning];
            break;
        }
    }
}

#pragma mark - AVCaptureVideoDataOutputSampleBufferDelegate

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection {
    // Extracting the CMBlockBuffer from the CMSampleBuffer
    printf("got capture\n");
    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    if (blockBuffer == NULL) {
        return; // Handle the error or exit the function
    }
    
    // Getting a pointer to the raw data
    size_t lengthAtOffset;
    size_t totalLength;
    char *dataPointer;
    CMBlockBufferGetDataPointer(blockBuffer, 0, &lengthAtOffset, &totalLength, &dataPointer);
    self.callback(self.user, (void*)dataPointer, totalLength);
}

@end


#include <camera/camera.hpp>
#ifdef __APPLE__
#import <camera/apple.h>
#endif
#include <memory>
//#include <vk/vk.hpp>
//#include <vkh/vkh_device.h>
//#include <vkh/vkh_image.h>

using namespace ion;

namespace ion {

Camera::Camera(void *user, CaptureCallback callback) : user(user), callback(callback) { }

#ifdef __APPLE__

/*
void global_callback(void *metal_texture, void *metal_layer, void *context) {
    Camera *camera = (Camera*)context;
    camera->e->vk_device->mtx.lock();
    camera->image = vkh_image_create(
        camera->e->vkh, VK_FORMAT_B8G8R8A8_UNORM, u32(camera->width), u32(camera->height),
        VK_IMAGE_TILING_OPTIMAL, VKH_MEMORY_USAGE_GPU_ONLY,
        VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT, metal_texture);
    camera->e->vk_device->mtx.unlock();
}

todo:
convert metal_texture to h264 nalu packet src
*/


void Camera::on_data(void *user, void *data, int len) {
    printf("got data: %p, %d\n", user, len);
}

/// Camera will resolve the vulkan texture in this module (not doing this in ux/app lol)
void Camera::start_capture() {
    //capture->capture = [[metal_capture alloc] initWithCallback:global_callback context:(void*)this camera_index:camera_index];
    AppleCapture *cap = [AppleCapture new];
    [cap start:0 width:1920 height:1080 user:(void*)this callback:&Camera::on_data];

    capture = (AppleCapture_*)CFBridgingRetain(cap);

}

void Camera::stop_capture() {
    [((__bridge AppleCapture*)capture) stop];
    capture = null;
}

#else

void Camera::start_capture() {
}

void Camera::stop_capture() {
}

#endif

}