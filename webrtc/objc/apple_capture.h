#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <AVFoundation/AVFoundation.h>
#else
using NSWindow     = void;
using AppleCapture = void;
#endif

typedef void(*EncodedFn)(void*, unsigned char*, int);

#ifdef __OBJC__
@interface AppleCapture : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>

- (void) start:(NSWindow*)window userPtr:(void*)user encodedFn:(EncodedFn)encoded_fn;
- (void) stop;

@end
#else

extern "C" {
void *capture_start(void *user, NSWindow *window, EncodedFn encoded_fn);
void  capture_stop (void *apple_capture);
}

#endif