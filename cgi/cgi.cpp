#include <cgi/cgi.hpp>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace ion {

const int MAX_FRAMES_IN_FLIGHT = 2;
uint32_t version = VK_MAKE_VERSION(1, 3, 0);

/// just based on shader you could use ray trace or composite

static bool         init = false;
static VkInstance   instance;
///
/// instance version should just be a build arg; why A) have multiple instances of multiple versions and B) have the user be burdened with which version they choose.  they should choose to just use cgi

struct _gpu {
	VkPhysicalDevice					gpu;
	VkPhysicalDeviceMemoryProperties	mem_props;
	vec2i 								dpi;
	VkPhysicalDeviceProperties			gpu_props;
	VkQueueFamilyProperties*			queues;
	uint32_t							queueCount;
	int									cQueue; // compute index
	int									gQueue; // graphic index
	int									tQueue; // transfer index
	int									pQueue; // presentation index
	array<VkExtensionProperties>		ext_props;
};

struct _frame {
    array<VkImage>          images;
    VkImageView             image_view;
    VkBuffer                uniform_buffer;
    VkDeviceMemory          uniform_buffer_memory;
    void*                   uniform_buffer_mapped;
    VkDescriptorPool        descriptor_pool;
    VkFramebuffer           frame_buffers;
    VkDescriptorSet         descriptor_sets;
    VkCommandBuffer         command_buffer;
    VkSemaphore             image_available_semaphores;
    VkSemaphore             render_finished_semaphores;
    VkFence                 in_flight_fences;
};

/// physicalDevice -> gpu
struct _device {
    VkDevice                            dev;
    _gpu                               *gpu;
    VkDebugUtilsMessengerEXT            debug_messenger;
    VkSampleCountFlagBits               samples = VK_SAMPLE_COUNT_1_BIT;
    VkRenderPass                        render_pass;
    VkQueue                             graphics_queue;
    VkQueue                             present_queue;

    VkSwapchainKHR                      swap_chain;
    array<_frame>                       frames;
    VkFormat                            frame_format;
    VkExtent2D                          frame_extent;
    _frame*                             frame;

    VkSurfaceKHR                        surface;

    VkCommandPool                       command_pool;
    VkSampler                           texture_sampler;
    uint32_t                            mip_levels;

    VkImage                             color_image;
    VkDeviceMemory                      color_image_memory;
    VkImageView                         color_image_view;

    VkImage                             depth_image;
    VkDeviceMemory                      depth_image_memory;
    VkImageView                         depth_image_view;

    VkImage                             texture_image;
    VkDeviceMemory                      texture_image_memory;
    VkImageView                         texture_image_view;

    VkPipelineLayout                    pipeline_layout;

    VkBuffer                            vertex_buffer;
    VkDeviceMemory                      vertex_buffer_memory;

    VkBuffer                            index_buffer;
    VkDeviceMemory                      index_buffer_memory;

    array<VkBuffer>                     uniform_buffers;
    array<VkDeviceMemory>               uniform_buffers_memory;
    array<void*>                        uniform_buffers_mapped;

    VkDescriptorPool                    descriptorPool;

    std::vector<VkDescriptorSet>        descriptorSets;
    std::vector<VkCommandBuffer>        commandBuffers;
    std::vector<VkSemaphore>            imageAvailableSemaphores;
    std::vector<VkSemaphore>            renderFinishedSemaphores;
    std::vector<VkFence>                inFlightFences;
    uint32_t currentFrame = 0;

    bool framebufferResized = false;



void createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

    uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("failed to find suitable memory type!");
    }
    
    ~_device() {
        vkDestroyDevice(dev, null);
    }
};

struct _window {
    _device*        idev;
    VkSurfaceKHR    surface;
    recti           rect;
};

struct _vertex {
    type_t          vertex_type;
    mx              vbo;    /// array<T> is based on mx, so you may reference data in the mx
    array<u32>      ibo;    /// indices are always in 32bit u-integer
    VkBuffer        buffer;
    _device        *idev;
    VkDeviceMemory  device_mem;

    VkVertexInputBindingDescription get_binding_description() {
        return VkVertexInputBindingDescription {
            .binding   = 0,
            .stride    = vbo.type().base_sz,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
    }

    /// converted triangle.cpp to work with mx
    static array<VkVertexInputAttributeDescription> get_attribute_descriptions(u32 binding) {
        array<VkVertexInputAttributeDescription> attrs { };
        doubly<prop> &meta = vertex_type->meta;
        u32 index = 0;
        auto get_format = [](prop &p) {
            if (p->member_type == typeof(vec2f)) return VK_FORMAT_R32G32_SFLOAT;
            if (p->member_type == typeof(vec3f)) return VK_FORMAT_R32G32B32_SFLOAT;
            if (p->member_type == typeof(vec4f)) return VK_FORMAT_R32G32B32A32_SFLOAT;
            if (p->member_type == typeof(vec4f)) return VK_FORMAT_R32_SFLOAT;
        };
        for (prop &p: meta) {
            VkVertexInputAttributeDescription v = {
                .binding  = binding,
                .location = index++,
                .format   = get_format(prop),
                .offset   = p.offset
            };
            attrs += v;
        }
        return attrs;
    }

    /// move to device
    void create_buffer(
            VkDeviceSize size,   VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
            VkBuffer&    buffer, VkDeviceMemory&    device_mem) {
        VkBufferCreateInfo bufferInfo { };
        bufferInfo.sType        = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size         = size;
        bufferInfo.usage        = usage;
        bufferInfo.sharingMode  = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create buffer!");

        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(device, buffer, &mem_req);
        VkMemoryAllocateInfo allocInfo { };
        allocInfo.sType             = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize    = mem_req.size;
        allocInfo.memoryTypeIndex   = findMemoryType(mem_req.memoryTypeBits, properties);
        if (vkAllocateMemory(device, &allocInfo, nullptr, &device_mem) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }
        vkBindBufferMemory(idev->dev, buffer, device_mem, 0);
    }

    ~_vintern() {
        vkDestroyBuffer(idev->dev, buffer, null);
    }
};

struct _shader {
    _device              *idev;
    array<VkBuffer>       uniformBuffers;
    array<VkDeviceMemory> uniformBuffersMemory;
    array<void*>          uniformBuffersMapped;
    VkDescriptorSetLayout descriptor_set_layout;

    void create_uniform_buffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers      .resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

            vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
        }
    }

    void create_descriptor_set_layout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding {};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(idev->dev, &layoutInfo, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    _shader(path cs) {

    }
};

struct _pipeline {
    _shader *shader;
};

struct _window {

};

ptr_implement(vdata, mx);



vdata::vdata(mx vbo, array<u32> ibo):mx(&data) {
    data->vbo = vbo;
    data->ibo = ibo;
    /// create index buffer and vertex buffer

    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

}