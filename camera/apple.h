#import <AVFoundation/AVFoundation.h>
#import <Metal/Metal.h>

typedef void(*CaptureCallback)(void*, void*, void*);

@interface metal_capture : NSObject

- (instancetype)initWithCallback:(CaptureCallback)callback context:(void*)ctx;
- (void)startCapture;
- (void)stopCapture;
@end
