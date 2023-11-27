#ifdef __APPLE__

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

struct camera_t;

@interface AppleCapture : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>

@property (nonatomic, strong) AVCaptureSession *captureSession;
@property (nonatomic, strong) AVCaptureVideoDataOutput *videoOutput;
@property (nonatomic, assign) camera_t* camera;

extern "C" void AllowKeyRepeats(void);

//- (AVCaptureDeviceFormat *) selectMJPEGFormatForDevice:(AVCaptureDevice *)device;
- (AVCaptureDeviceFormat *)filterDevice:(AVCaptureDevice *)device format:(FourCharCode)format;
- (void)start:(camera_t*)camera;
- (void)stop;
- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;

@end

#endif