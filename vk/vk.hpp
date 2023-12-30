#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

/// this belongs here i think
//#include <vk/tiny_obj_loader.h>

#include <mx/mx.hpp>
#include <media/image.hpp>

#include <watch/watch.hpp>

#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>

#define VMA_VULKAN_VERSION 1001000 // Vulkan 1.1 (skia does not use 1.2) <- look at this

#include <vk/vk_mem_alloc.h>
#include <vk/gltf.hpp>

namespace ion {

static const int MAX_FRAMES_IN_FLIGHT = 2;
static const int MAX_PBR_LIGHTS       = 3;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct Vulkan:mx {
    struct M {
        int v_major = 1;
        int v_minor = 0;
        uint32_t version = VK_MAKE_VERSION(1, 0, 0);
        VkApplicationInfo app_info;
        void init();
        ~M();
        VkInstance inst();
        bool check_validation();
        std::vector<symbol> getRequiredExtensions();
        operator bool();
        type_register(M);
    };
    mx_declare(Vulkan, mx, M);

    /// need not construct unless we want a specific version other than 1.0
    Vulkan(int v_major, int v_minor):Vulkan() {
        data->v_major = v_major;
        data->v_minor = v_minor;
        data->version = VK_MAKE_VERSION(v_major, v_minor, 0);
    }
};

template <> struct is_singleton<Vulkan> : true_type { };

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

struct Pipeline;

/// we need more key handlers
using ResizeFn = void(*)(vec2i&, void*);

struct Texture;
struct Device;

struct Window:mx {

    struct M {
        VkInstance              instance;
        VkPhysicalDevice        phys        = VK_NULL_HANDLE;
        VkSampleCountFlagBits   msaaSamples = VK_SAMPLE_COUNT_1_BIT; /// max allowed by this GPU, not the actual sample counts used all the time; it can also force sampling down
        GLFWwindow*             window      = NULL;
        VkSurfaceKHR            surface     = 0;
        QueueFamilyIndices      indices;
        SwapChainSupportDetails details;
        ResizeFn                resize; /// deprecate
        vec2i                   sz;
        VkPhysicalDeviceFeatures support; // move to generic
        vec2d                   dpi_scale;
        std::vector<symbol>     device_extensions; // move to generic

        VkSampleCountFlagBits getUsableSampling(VkSampleCountFlagBits max);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

        static void framebuffer_resized(GLFWwindow *, int, int);

        ~M();
        operator bool() { return phys != VK_NULL_HANDLE; }
        type_register(M);

        Texture &texture(Device &dev, vec2i sz, bool sampling,
            VkImageUsageFlagBits usage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
    };

    operator VkPhysicalDevice();

    static GLFWwindow *initWindow(vec2i &sz);
    
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice phys, VkSurfaceKHR surface, SwapChainSupportDetails &details);

    static SwapChainSupportDetails querySwapChainSupport(Window &gpu);

    /// this checkDeviceExt needs to init the exts elsewhere, at config time
    static bool isDeviceSuitable(VkPhysicalDevice phys, VkSurfaceKHR surface,
        QueueFamilyIndices &indices, SwapChainSupportDetails &swapChainSupport,
        std::vector<symbol> &ext);
    
    static bool checkDeviceExtensionSupport(VkPhysicalDevice phys, std::vector<symbol> &ext);

    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice phy, VkSurfaceKHR surface);

    static QueueFamilyIndices findQueueFamilies(Window &gpu);

    
    static Window select(vec2i sz, ResizeFn resize, void *user_data);
    
    mx_object(Window, mx, M);
};

static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                           VkFormat format, VkImageLayout oldLayout,
                           VkImageLayout newLayout, uint32_t mipLevels) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

struct Pipes;

struct Device:mx {
    struct M {
        Window                      gpu;
        VkDevice                    device;
        VkQueue                     graphicsQueue;
        VkQueue                     presentQueue;
        VkSwapchainKHR              swapChain;
        std::vector<VkImage>        swapChainImages;
        VkFormat                    swapChainImageFormat;
        VkFormat                    textureFormat;
        VkExtent2D                  swapChainExtent;
        std::vector<VkImageView>    swapChainImageViews;
        std::vector<VkFramebuffer>  swapChainFramebuffers;
        VkRenderPass                renderPass;
        VkDescriptorPool            descriptorPool;
        VkCommandPool               commandPool;
        
        VkImage                     resolveImage; /// the resolve of colorImage, used for screenshots
        VkDeviceMemory              resolveImageMemory;
        VkImage                     transferImage;
        VkDeviceMemory              transferImageMemory;

        VkImage                     colorImage;
        VkDeviceMemory              colorImageMemory;
        VkImageView                 colorImageView;
        VkImage                     depthImage;
        VkDeviceMemory              depthImageMemory;
        VkImageView                 depthImageView;
        VkSemaphore                 semaphore;
        bool                        framebufferResized;
        uint32_t                    mipLevels;
        std::mutex                  mtx;

        std::vector<VkCommandBuffer> commandBuffers;

        std::vector<VkSemaphore>    imageAvailableSemaphores;
        std::vector<VkSemaphore>    renderFinishedSemaphores;
        std::vector<VkFence>        inFlightFences;
        uint32_t                    currentFrame = 0;

        lambda<void(VkImage, VkCommandBuffer)> preCommands;
        lambda<void(VkImage, VkCommandBuffer)> postCommands;

        // resolves colorImage (to resolveImage), transfers resolveImage
        ion::image screenshot();

        void drawFrame(array<Pipes> &a_pipes);
        void recreateSwapChain();
        void createDescriptorPool();
        void createFramebuffers();
        void createSyncObjects();
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        VkCommandBuffer command_begin();
        void command_submit(VkCommandBuffer commandBuffer);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void createCommandBuffers();
        void createCommandPool();
        void createColorResources();
        void createDepthResources();
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        VkFormat findDepthFormat();
        bool hasStencilComponent(VkFormat format);
        void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
            VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, void *platform_import = null);
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        void createLogicalDevice();
        void createSwapChain();
        void createImageViews();
        void createRenderPass();
        void cleanupSwapChain();
        ~M();
        void loop(lambda<void(array<Pipes>&)>);
        operator bool() { return device != VK_NULL_HANDLE; }
        type_register(M);
    };

    static Device create(Window &gpu);

    operator VkDevice();
    mx_object(Device, mx, M);
};

enums(Asset, color, 
     color, normal, material, reflect, env); /// populate from objects normal map first, and then adjust by equirect if its provided

enums(Rendition, none,
     none, shader, wireframe);

enums(ShadeModule, undefined,
     undefined, vertex, fragment, compute);

/// never support 16bit indices for obvious reasons.  you do 1 cmclark section too many and there it goes
struct ngon {
    size_t size;
    u32   *indices;
};

using vpair = std::pair<int, int>;

struct vpair_hash {
    std::size_t operator()(const vpair& p) const {
        return std::hash<int>()(p.first) ^ std::hash<int>()(p.second);
    }
};

using mesh  = array<ngon>;

//using u32   = uint32_t;

using face  = ngon;

struct Texture:mx {
    struct M {
        Device          device;
        VkImageUsageFlagBits usage;
        VkFormat        format;
        VkImage         image;
        VkDeviceMemory  memory;
        VkImageView     view;
        VkSampler       sampler;
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        int             width;
        int             height;
        ion::path       path;
        Asset           asset_type;
        vec2i           sz;
        bool            updated;
        
        void create_image_view();
        void create_sampler();
        void create_image(ion::image img, Asset type); // VK_FORMAT_R8G8B8A8_SRGB
        void create_image(vec2i sz);
        void update_image(ion::image &img);
        
        ~M();
        operator bool() { return image != VK_NULL_HANDLE; }
        type_register(M);
    };
    mx_object(Texture, mx, M);

    static Texture    load(Device &dev, symbol name, Asset type);
    static ion::image asset_image(symbol name, Asset type);
    static Texture    from_image(Device &dev, image img, Asset type);

    void update(image img);
};

using GraphicsGen = lambda<void(mx&,mx&,array<image>&)>;

struct GraphicsData {
    str    name;
    symbol shader;
    type_t utype;
    type_t vtype;
    GraphicsGen gen;
    ///
    type_register(GraphicsData);
};

struct Graphics:mx {
    Graphics(symbol name, type_t utype, type_t vtype, symbol shader = "pbr", GraphicsGen gen = null) : Graphics() {
        data->name   = name;
        data->shader = shader;
        data->utype  = utype;
        data->vtype  = vtype;
        data->gen    = gen;
    }
    mx_object(Graphics, mx, GraphicsData);
};

using MG = map<Graphics>;

struct Pipeline:mx {
    struct M {
        mx                          user; // user data
        str                         name; // Pipelines represent 'parts' of models and must have a name
        Device                      device;
        memory*                     gmem; // graphics memory (grabbed)
        Graphics                    gfx;
        std::vector<VkBuffer>       uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        std::vector<void*>          uniformBuffersMapped;
        VkDescriptorSetLayout       descriptorSetLayout;
        VkPipelineLayout            pipelineLayout;
        VkPipeline                  graphicsPipeline;
        VkBuffer                    vertexBuffer;
        VkDeviceMemory              vertexBufferMemory;
        VkBuffer                    indexBuffer;
        VkDeviceMemory              indexBufferMemory;
        size_t                      indicesSize;
        lambda<void(Pipeline::M&)>  reload;
        lambda<void(memory*)>       uniform_update;
        bool                        init;
        watch                       watcher;

        std::vector<VkDescriptorSet>                   descriptorSets;
        VkVertexInputBindingDescription                binding_desc;
        std::vector<VkVertexInputAttributeDescription> attr_desc;

        Texture textures[Asset::count];

        void start();
        void cleanup(); /// M calls cleanup, but cleanup is called prior to a reload
        void createUniformBuffers();

        ~M();

        VkShaderModule createShaderModule(const array<char>& code);

        void createDescriptorSetLayout();
        void createDescriptorSets();
        void updateDescriptorSets();
        void createGraphicsPipeline();

        
        /// initialize vertex array, and index arrays
        static void assemble_graphics(Pipeline::M *pipeline, gltf::Model &m, Graphics &gfx);

        void createVertexBuffer(mx vertices);
        void createIndexBuffer(mx indices);

        VkVertexInputBindingDescription getBindingDescription();
        std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        operator bool() { return graphicsPipeline != VK_NULL_HANDLE; }
        type_register(M);
    };

    /// pipelines track themselves, and notify the user (the app that a reload is happening [likely needed for status/logging alone?])
    /// important to note that Device is not reloaded
    /// if you have 40 pipelines and 1 file changes associated to one of them, we are not rebuilding all 40, just 1.
    Pipeline(Device &device, Graphics graphics):Pipeline() { /// instead of a texture it needs flags for the resources to load
        data->gmem           = graphics.hold(); /// just so we can hold onto the data; we must drop this in our dtr
        data->gfx            = graphics.data;
        data->device         = device;
        data->name           = graphics->name;
    }

    mx_object(Pipeline, mx, M);
};

/// for Daniel to call back.. they got tired of calling
struct Pipes:mx {
    struct M {
        Device                      device;
        gltf::Model                 m;
        symbol                      model;
        array<Texture>              textures { Asset::count };
        array<Pipeline>             pipelines;
        lambda<void(Pipeline::M&)>  reload;
        lambda<void(memory*)>       uniform_update;
        register(M);
    };

    mx_basic(Pipes);

    /// alternately Pipeline could be a vector of Parts data; i just need this simplicity now
    Pipes(Device &device, symbol model, array<Graphics> parts):Pipes() { /// major change: parts uses Graphics
        using namespace gltf;
        data->device = device;
        data->model = model;
        if (model) {
            path gltf_path = fmt {"models/{0}.gltf", { model }};
            data->m = Model::load(gltf_path);
        }

        /// all pipelines must use same Device
        for (Graphics &gfx: parts)
            data->pipelines += Pipeline(device, gfx);

        auto reload_textures = [data=data, model=model]() {
            static bool loaded = false;
            if (!loaded) { /// add watch ability back to this with a cache
                for (size_t i = 0; i < Asset::count; i++) /// todo: check against usage map to see if the texture applies and should be loaded
                    data->textures[i] = Texture::load(data->device, model, Asset(i));
                loaded = true;
            }
        };
                
        /// it would be best to make this one runtime
        data->uniform_update = [data=data](memory *pipeline_mem) {
            Pipeline::M *p   = (Pipeline::M *)pipeline_mem->origin;
            static void *ubo = p->gfx->utype->functions->alloc_new(null, null); /// not a leak.  its a singleton
            p->gfx->utype->functions->process(ubo, pipeline_mem);
            memcpy(p->uniformBuffersMapped[p->device->currentFrame], ubo, p->gfx->utype->base_sz);
        };

        data->reload = [data=data, reload_textures=reload_textures](Pipeline::M &p) { /// this will need a proper cleanup proc in itsself
            p.binding_desc = p.getBindingDescription();
            p.attr_desc    = p.getAttributeDescriptions();
            p.createUniformBuffers();
            p.createDescriptorSetLayout();
            p.createGraphicsPipeline();

            reload_textures();

            /// load texture assets for all enumerables (minus undefined)
            for (size_t i = 0; i < Asset::count; i++) /// todo: check against usage map to see if the texture applies and should be loaded
                p.textures[i] = data->textures[i];
            
            Pipeline::M::assemble_graphics(&p, data->m, p.gfx);
            p.createDescriptorSets();
        };

        for (Pipeline &p: data->pipelines) {
            p->uniform_update = data->uniform_update;
            p->reload = data->reload;
            p->start();
        }
        reload_textures();
    }

    Pipeline &operator[](str s) {
        for (Pipeline &p: data->pipelines) {
            if (s == p->name)
                return p;
        }
        assert(false);
        static Pipeline def;
        return def;
    }
};





}