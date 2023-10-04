#ifdef __APPLE__

typedef void (*CaptureCallback)(void*, void*, int);

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

@interface AppleCapture : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>

@property (nonatomic, strong) AVCaptureSession *captureSession;
@property (nonatomic, strong) AVCaptureVideoDataOutput *videoOutput;
@property (nonatomic, assign) void *user;
@property (nonatomic, assign) CaptureCallback callback;

extern "C" void AllowKeyRepeats(void);

//- (AVCaptureDeviceFormat *) selectMJPEGFormatForDevice:(AVCaptureDevice *)device;
- (AVCaptureDeviceFormat *)filterDevice:(AVCaptureDevice *)device format:(FourCharCode)format;
- (void)start:(int)index width:(int)width height:(int)height user:(void*)user callback:(CaptureCallback)callback;
- (void)stop;
- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;

@end

#endif