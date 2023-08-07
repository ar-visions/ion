#include <camera/camera.hpp>
#import <camera/apple.h>
#include <memory>
#include <vk/vk.hpp>
#include <vkh/vkh_device.h>
#include <vkh/vkh_image.h>

using namespace ion;



id<MTLDevice> device;
id<MTLCommandQueue> cmd_queue;

struct opaque_capture {
    metal_capture *capture;
    opaque_capture() {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        id<MTLCommandQueue> cmd_queue = [device newCommandQueue];
    }
};

/// this method updates the camera surface from an incoming metal texture
/// lots of issues surrounding an accelerated copy/blt of this texture imported from MoltenVK through extension of metal_objects
/// it seemed to import with no planes (which perhaps we can debug as to why lol, but )
void global_callback(void *metal_texture, void *metal_layer, void *context) {
    Camera *camera = (Camera*)context;
    camera->e->vk_device->mtx.lock();


    static id<MTLTexture> metalTexture;

    if (metalTexture) {
        const NSUInteger width         = 512;
        const NSUInteger height        = 512;
        const NSUInteger bytesPerPixel = 4; // Assuming 4 bytes per pixel (RGBA)
        const NSUInteger bytesPerRow   = width * bytesPerPixel;
        const NSUInteger imageSize     = width * height * bytesPerPixel;

        // Define your plain color here (RGBA format, 8 bits per channel)
        unsigned char colorData[] = { 255, 0, 255, 255 }; // Pink, fully opaque

        id<MTLBuffer> colorBuffer = [device newBufferWithBytes:colorData length:imageSize options:MTLResourceStorageModeShared];

        MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
        textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
        textureDescriptor.width = width;
        textureDescriptor.height = height;

        metalTexture = [device newTextureWithDescriptor:textureDescriptor];

        MTLRegion region = {
            { 0, 0, 0 },                     // Origin
            { width, height, 1 }             // Size
        };

        // Copy color data from the buffer to the texture
        [metalTexture replaceRegion:region mipmapLevel:0 withBytes:[colorBuffer contents] bytesPerRow:bytesPerRow];
    }
    

    //if (camera->image)
    //    vkh_image_drop(camera->image);

    /// this needs further debugging via the blit operation
    /// it reports no planes (the planes would be seemingly created here?)
    void *metal_texture2 = (__bridge void*)metalTexture;
    camera->image = vkh_image_create(
        camera->e->vkh, VK_FORMAT_B8G8R8A8_UNORM, 1920, 1080, VK_IMAGE_TILING_OPTIMAL, VKH_MEMORY_USAGE_GPU_ONLY,
        VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT, metal_texture);

    camera->e->vk_device->mtx.unlock();
    return;

    id<MTLTexture> texture = (__bridge id<MTLTexture>)metal_texture;
    NSUInteger stride      = texture.width * 4;
    NSUInteger bufferSize  = texture.height * stride;
    uint8_t   *pixels      = (uint8_t *)malloc(bufferSize);

    MTLRegion region = {
        {0, 0, 0}, // origin (x, y, z)
        {texture.width, texture.height, 1} // size (width, height, depth=1[?])
    };
    [texture getBytes:pixels
          bytesPerRow:stride
           fromRegion:region
          mipmapLevel:0];

    vkvg_surface_drop(camera->surface);
    //camera->surface = vkvg_surface_create_from_bitmap(
    //    camera->e->vkh->device, pixels, texture.width, texture.height);

    free(pixels);

    camera->e->vk_device->mtx.unlock();
}

Camera::Camera() : e((VkEngine)null), camera_index(0), image((VkhImage)null), capture((opaque_capture*)null) { }

Camera::Camera(VkEngine e, int camera_index) : e(e), camera_index(camera_index), image((VkhImage)null), capture((opaque_capture*)null) { }

/// Camera will resolve the vulkan texture in this module (not doing this in ux/app lol)
void Camera::start_capture() {
    capture          = new opaque_capture();
    capture->capture = [[metal_capture alloc] initWithCallback:global_callback context:(void*)this camera_index:camera_index];
    [capture->capture startCapture];
}

void Camera::stop_capture() {
    [capture->capture stopCapture];
    delete capture;
    capture = null;
}