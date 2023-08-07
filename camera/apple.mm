#import <camera/apple.h>
#import <media/media.hpp>

@interface metal_capture () <AVCaptureVideoDataOutputSampleBufferDelegate>
@property (nonatomic, strong) AVCaptureSession         *captureSession;
@property (nonatomic, strong) AVCaptureDeviceInput     *videoInput;
@property (nonatomic, strong) AVCaptureVideoDataOutput *videoOutput;
@property (nonatomic, strong) dispatch_queue_t          videoOutputQueue;
@property (nonatomic, strong) id<MTLTexture>            currentFrameTexture;
@property (nonatomic, strong) id<MTLDevice>             metalDevice;
@property (nonatomic, strong) id<MTLCommandQueue>       metalCommandQueue;
@property (nonatomic, assign) CVMetalTextureCacheRef    metalTextureCache;
@property (nonatomic, assign) CaptureCallback           captureCallback;
@property (nonatomic, assign) void*                     ctx;
@property (nonatomic, assign) int                       camera_index;
@end

@implementation metal_capture

- (instancetype)initWithCallback:(CaptureCallback)callback context:(void*)ctx camera_index:(int)camera_index {
    self = [super init];
    if (self) {
        _captureSession = [[AVCaptureSession alloc] init];
        _videoOutputQueue = dispatch_queue_create("videoOutputQueue", DISPATCH_QUEUE_SERIAL);
        _metalDevice = MTLCreateSystemDefaultDevice();
        _metalCommandQueue = [_metalDevice newCommandQueue];
        _captureCallback = callback;
        _ctx = ctx;
        _camera_index = camera_index;
        // Create the Metal texture cache
        CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, _metalDevice, nil, &_metalTextureCache);
    }
    return self;
}

- (void)dealloc {
    CVMetalTextureCacheFlush(_metalTextureCache, 0);
    CFRelease(_metalTextureCache);
}

AVCaptureDevice* select_camera(int camera_index) {
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSArray<AVCaptureDevice *> *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];    
    #pragma clang diagnostic pop
    return (devices.count > camera_index) ? devices[camera_index] : nil;
}

- (void)startCapture {
    [_captureSession beginConfiguration];

    // Set the capture preset (choose an appropriate resolution)
    if ([_captureSession canSetSessionPreset:AVCaptureSessionPreset1920x1080]) {
        [_captureSession setSessionPreset:AVCaptureSessionPreset1920x1080];
    }

    // Create an AVCaptureDevice for the webcam (front or back camera)
    AVCaptureDevice *videoDevice = select_camera(_camera_index);
    NSAssert(videoDevice, @"camera not found at index: %d", _camera_index);

    // Create an AVCaptureDeviceInput using the video device
    NSError *error = nil;
    _videoInput = [AVCaptureDeviceInput deviceInputWithDevice:videoDevice error:&error];
    
    if (error) {
        NSLog(@"Error creating AVCaptureDeviceInput: %@", error);
        return;
    }

    // Add the input to the session
    if ([_captureSession canAddInput:_videoInput]) {
        [_captureSession addInput:_videoInput];
    }

    // Create an AVCaptureVideoDataOutput to receive frames
    _videoOutput = [[AVCaptureVideoDataOutput alloc] init];

    // Set the output pixel format (choose appropriate format for your needs)
    NSDictionary *videoSettings = @{(NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
    _videoOutput.videoSettings = videoSettings;

    // Set the dispatch queue for frame capture
    [_videoOutput setSampleBufferDelegate:self queue:_videoOutputQueue];

    // Add the output to the session
    if ([_captureSession canAddOutput:_videoOutput]) {
        [_captureSession addOutput:_videoOutput];
    }

    [_captureSession commitConfiguration];
    [_captureSession startRunning];
}

- (void)stopCapture {
    [_captureSession stopRunning];
    [_captureSession removeInput:_videoInput];
    [_captureSession removeOutput:_videoOutput];
}

#pragma mark - AVCaptureVideoDataOutputSampleBufferDelegate

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
            fromConnection:(AVCaptureConnection *)connection {
    // Convert the CMSampleBuffer to a CVPixelBuffer
    CVPixelBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    // Create a Metal texture from the CVPixelBuffer using the Metal texture cache
    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    
    
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
        width:width height:height mipmapped:NO];
    
    CVMetalTextureRef metalTextureRef = nil;

    CVReturn status = CVMetalTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault, _metalTextureCache, imageBuffer, nil,
        textureDescriptor.pixelFormat, width, height, 0, &metalTextureRef);
    
    if (status != kCVReturnSuccess) {
        NSLog(@"Error creating Metal texture from CVPixelBuffer");
        return;
    }
    
    self.currentFrameTexture = CVMetalTextureGetTexture(metalTextureRef);

    if (self.captureCallback) {
        self.captureCallback((__bridge void*)self.currentFrameTexture, 0, self.ctx);
    }

    CFRelease(metalTextureRef);
}

@end