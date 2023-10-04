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
