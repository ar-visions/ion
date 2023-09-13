#import "apple_capture.h"

@implementation AppleCapture {
    AVCaptureSession *_cs;
    NSWindow         *_window;
    void             *_user;
    EncodedFn         _encoded_fn;
    bool              _stopped;
}

- (void)stop {
    _stopped = true;
    [_cs stopRunning];
}

- (void)start:(NSWindow*)window userPtr:(void*)user encodedFn:(EncodedFn)encoded_fn {
    _window         = window;
    _user           = user;
    _encoded_fn     = encoded_fn;

    /// get the window rect
    CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    CGRect     windowRect = CGRectNull;

    CGWindowID windowID = [_window windowNumber];

    for (NSDictionary *info in (__bridge NSArray*)windowList) {
        NSNumber *windowNumber = [info objectForKey:(id)kCGWindowNumber];
        if ([windowNumber unsignedIntValue] == windowID) {
            CGRect bounds;
            CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)[info objectForKey:(id)kCGWindowBounds], &bounds);
            windowRect = bounds;
            break;
        }
    }
    CFRelease(windowList);

    if (!CGRectIsNull(windowRect)) {
        printf("could not find window\n");
        assert(false);
    }
    
    _cs = [[AVCaptureSession alloc] init];
    AVCaptureScreenInput *screenInput = [[AVCaptureScreenInput alloc] initWithDisplayID:CGMainDisplayID()];
    screenInput.cropRect = windowRect;
    if ([_cs canAddInput:screenInput]) {
        [_cs addInput:screenInput];
    }

    // ... Set up your output (e.g., AVCaptureMovieFileOutput or AVCaptureVideoDataOutput)
    // ... Set up your input, e.g., AVCaptureDeviceInput
    AVCaptureVideoDataOutput *videoOutput = [[AVCaptureVideoDataOutput alloc] init];
    [videoOutput setSampleBufferDelegate:self queue:dispatch_get_main_queue()];  // Set the delegate
    if ([_cs canAddOutput:videoOutput]) {
        [_cs addOutput:videoOutput];
    }

    [_cs startRunning];
}

#pragma mark - AVCaptureVideoDataOutputSampleBufferDelegate

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection {
    if (_stopped)
        return;
    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    size_t length = CMBlockBufferGetDataLength(blockBuffer);
    char* data;
    CMBlockBufferGetDataPointer(blockBuffer, 0, NULL, &length, &data);
    _encoded_fn(_user, (unsigned char*)data, (int)length);
}

@end

extern "C" {
void *capture_start(void *user, NSWindow *window, EncodedFn encoded_fn) {
    AppleCapture *capture = [[AppleCapture alloc] init];
    [capture start:window userPtr:user encodedFn:encoded_fn];
    return (__bridge void *)capture;
}

void  capture_stop (void *handle) {
    AppleCapture *capture = (__bridge AppleCapture *)handle;
    [capture stop];
    [capture release];
}
}