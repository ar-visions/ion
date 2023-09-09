#ifdef __APPLE__
#import <AVFoundation/AVFoundation.h>
#import <Metal/Metal.h>

typedef void(*CaptureCallback)(void*, void*, void*);

@interface metal_capture : NSObject

- (instancetype)initWithCallback:(CaptureCallback)callback context:(void*)ctx camera_index:(int)camera_index;
- (void)startCapture;
- (void)stopCapture;
@end
#endif