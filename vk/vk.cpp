#include <mx/mx.hpp>
#include <media/image.hpp>

#include <vk/vk.hpp>

//#define TINYOBJLOADER_IMPLEMENTATION
//#include <vk/tiny_obj_loader.h>

using namespace ion;

static const bool               enable_validation = is_debug();
static VkInstance               instance = 0;
static VkDebugUtilsMessengerEXT debugMessenger;

const std::vector<symbol> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo                  = {};
    createInfo.sType            = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType      = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback  = debugCallback;
}

std::vector<symbol> Vulkan::M::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<symbol> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    #ifndef WIN32
    extensions.push_back("VK_KHR_portability_enumeration");
    #endif
    #ifdef __APPLE__
    extensions.push_back("VK_KHR_get_physical_device_properties2");
    //extensions.push_back("VK_EXT_metal_surface");
    #endif

    if (enable_validation)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

mx_implement(Vulkan, mx);

bool Vulkan::M::check_validation() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

bool is_wayland() {
    const char* session_type = std::getenv("XDG_SESSION_TYPE");
    return session_type && strcmp(session_type, "wayland") == 0;
}

void Vulkan::M::init() {
    static bool init = false;
    if (init) return;

    init = true;

    if (enable_validation && !check_validation()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    v_major = 1;
    v_minor = 3;

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "ion:vk";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "ion:vk";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion         = VK_MAKE_VERSION(v_major, v_minor, 0);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &app_info;
    createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    auto extensions = getRequiredExtensions();

    extensions = std::vector<symbol>();
    extensions.push_back("VK_KHR_surface");
    extensions.push_back("VK_KHR_portability_enumeration"); /// there is a _subset extension too but not sure if thats device
    
    if (is_apple()) {
        extensions.push_back("VK_EXT_metal_surface");
    } else if (is_win()) {
        extensions.push_back("VK_KHR_win32_surface");
    } else {
        if (is_wayland())
            extensions.push_back("VK_KHR_wayland_surface");
        else
            extensions.push_back("VK_KHR_xcb_surface"); // xlib
    }//VK_KHR_xcb_surface
    
    extensions.push_back("VK_KHR_get_physical_device_properties2");
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data(); // VK_KHR_surface ?

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};

    if (enable_validation) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

    if (enable_validation) {
        VkDebugUtilsMessengerCreateInfoEXT dbg_info;
        populateDebugMessengerCreateInfo(dbg_info);
        if (CreateDebugUtilsMessengerEXT(instance, &dbg_info, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
}

VkInstance Vulkan::M::inst() {
    return instance;
}

Vulkan::M::operator bool() {
    return instance != VK_NULL_HANDLE;
}

Vulkan::M::~M() {
    if (enable_validation) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(instance, nullptr);
    glfwTerminate();
}

Window::operator VkPhysicalDevice() {
    return data->phys;
}

VkSampleCountFlagBits Window::M::getUsableSampling(VkSampleCountFlagBits max) {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(phys, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if ((max >= VK_SAMPLE_COUNT_64_BIT) && (counts & VK_SAMPLE_COUNT_64_BIT)) { return VK_SAMPLE_COUNT_64_BIT; }
    if ((max >= VK_SAMPLE_COUNT_32_BIT) && (counts & VK_SAMPLE_COUNT_32_BIT)) { return VK_SAMPLE_COUNT_32_BIT; }
    if ((max >= VK_SAMPLE_COUNT_16_BIT) && (counts & VK_SAMPLE_COUNT_16_BIT)) { return VK_SAMPLE_COUNT_16_BIT; }
    if ((max >= VK_SAMPLE_COUNT_8_BIT)  && (counts & VK_SAMPLE_COUNT_8_BIT))  { return VK_SAMPLE_COUNT_8_BIT;  }
    if ((max >= VK_SAMPLE_COUNT_4_BIT)  && (counts & VK_SAMPLE_COUNT_4_BIT))  { return VK_SAMPLE_COUNT_4_BIT;  }
    if ((max >= VK_SAMPLE_COUNT_2_BIT)  && (counts & VK_SAMPLE_COUNT_2_BIT))  { return VK_SAMPLE_COUNT_2_BIT;  }

    return VK_SAMPLE_COUNT_1_BIT;
}

GLFWwindow *Window::initWindow(vec2i &sz) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    return glfwCreateWindow(sz.x, sz.y, "Vulkan", nullptr, nullptr);        
}

SwapChainSupportDetails Window::querySwapChainSupport(VkPhysicalDevice phys, VkSurfaceKHR surface, SwapChainSupportDetails &details) {
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &details.capabilities) != VK_SUCCESS)
        throw std::runtime_error("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failure");

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

SwapChainSupportDetails Window::querySwapChainSupport(Window &gpu) {
    return querySwapChainSupport(gpu->phys, gpu->surface, gpu->details);
}

bool Window::checkDeviceExtensionSupport(VkPhysicalDevice phys, std::vector<symbol> &exts) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(exts.begin(), exts.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    //printf("the following device extensions are not supported (and need to be):");
    //for (std::string ext: requiredExtensions) {
    //    printf("device extension: %s\n", ext.c_str());
    //}
    return requiredExtensions.empty();
}

QueueFamilyIndices Window::findQueueFamilies(VkPhysicalDevice phy, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phy, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(phy, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(phy, i, surface, &presentSupport);

        if (presentSupport)
            indices.presentFamily = i;

        if (indices.isComplete())
            break;

        i++;
    }

    return indices;
}

QueueFamilyIndices Window::findQueueFamilies(Window &gpu) {
    return findQueueFamilies(gpu->phys, gpu->surface);
}

bool Window::isDeviceSuitable(VkPhysicalDevice phys, VkSurfaceKHR surface, QueueFamilyIndices &indices,
        SwapChainSupportDetails &swapChainSupport, std::vector<symbol> &exts) {
    bool extensionsSupported = checkDeviceExtensionSupport(phys, exts);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        querySwapChainSupport(phys, surface, swapChainSupport);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(phys, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate  && supportedFeatures.samplerAnisotropy;
}

void Window::M::framebuffer_resized(GLFWwindow* window, int width, int height) {
    /// i think acquire next image works with surface
    //Window::M *g = (Window::M*)(glfwGetWindowUserPointer(window));
    //g->sz = vec2i { width, height };
    //g->resize(g->sz, g->user_data);
}

/// select gpu, initialize vulkan, create window and register user
Window Window::select(vec2i sz, ResizeFn resize, void *user_data) {
    Window g = Window();

    /// this is from vk engine
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);
	glfwWindowHint(GLFW_FLOATING,   GLFW_FALSE);
	glfwWindowHint(GLFW_DECORATED,  GLFW_TRUE);

	int mcount;
    static int dpi_index = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&mcount);
	if (mcount > dpi_index) {
        float fx, fy;
		glfwGetMonitorContentScale(monitors[dpi_index], &fx, &fy);
        g->dpi_scale.x = fx;
		g->dpi_scale.y = fy;
	} else {
		g->dpi_scale.x = 1.0;
		g->dpi_scale.y = 1.0;
	}

    g->window = initWindow(sz);
    g->sz = sz;
    
    Vulkan vk { 1, 1 }; /// singleton; if constructed prior with a version, that remains set
    vk->init();

    g->instance = vk->inst();

    if (glfwCreateWindowSurface(g->instance, g->window, nullptr, &g->surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
        throw std::runtime_error("failed to find GPUs with Vulkan support!");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    g->device_extensions = {};
    g->device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    
    if (is_apple()) {
        g->device_extensions.push_back("VK_KHR_portability_subset");
        g->device_extensions.push_back("VK_EXT_metal_objects");
    }

    g->device_extensions.push_back("VK_KHR_dedicated_allocation"); /// for VMA with Vulkan >= 1.1

    for (const auto& phys: devices) {
        g->indices = findQueueFamilies(phys, g->surface);
        if (isDeviceSuitable(phys, g->surface, g->indices, g->details, g->device_extensions)) {
            g->phys        = phys;
            g->msaaSamples = g->getUsableSampling(VK_SAMPLE_COUNT_8_BIT);
            break;
        }
    }
    
    if (g->phys == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable Window!");
    }

    vkGetPhysicalDeviceFeatures(g->phys, &g->support);

    /// we should fully isolate glfw in vk
    glfwSetWindowUserPointer(g->window, user_data);
    //glfwSetFramebufferSizeCallback(g->window, M::framebuffer_resized); -- user data not layered here and will be handled in ux
    glfwShowWindow(g->window);
    return g;
}

uint32_t Window::M::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
    return 0;
}

Window::M::~M() {
    vkDestroySurfaceKHR(instance, surface, nullptr);
    glfwDestroyWindow(window);
}

Device::operator VkDevice() { return data->device; }

void Device::M::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(gpu->window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(gpu->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);
    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createColorResources();
    createDepthResources();
    createFramebuffers();
}

void Device::M::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1 + Asset::count> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    ///
    for (int i = 0; i < Asset::count; i++) {
        poolSizes[i + 1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[i + 1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void Device::M::createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            colorImageView,
            depthImageView,
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void Device::M::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void Device::M::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = gpu->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkCommandBuffer Device::M::command_begin() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Device::M::command_submit(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Device::M::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = command_begin();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    command_submit(commandBuffer);
}

void Device::M::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Device::M::createCommandPool() {
    QueueFamilyIndices &queueFamilyIndices = gpu->indices;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics command pool!");
    }
}

void Device::M::createColorResources() {
    VkFormat colorFormat = swapChainImageFormat;

    createImage(swapChainExtent.width, swapChainExtent.height, 1, gpu->msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        colorImage, colorImageMemory);

    colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

    createImage(swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, colorFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        resolveImage, resolveImageMemory);
    transitionImageLayout(resolveImage, colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

    createImage(swapChainExtent.width, swapChainExtent.height, 1,
        VK_SAMPLE_COUNT_1_BIT, colorFormat, VK_IMAGE_TILING_LINEAR,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        transferImage, transferImageMemory);
    transitionImageLayout(transferImage, colorFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
}

ion::image Device::M::screenshot() {
    uint32_t width  = gpu->sz.x;
    uint32_t height = gpu->sz.y;
    image    res { size { height, width }};
    
    VkCommandBuffer commandBuffer = command_begin();
    VkImageResolve  resolveRegion = {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1
        },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1
        },
        .extent         = { width, height, 1 }
    };

    vkCmdResolveImage(
        commandBuffer,
        colorImage,   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        resolveImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &resolveRegion);

    command_submit(commandBuffer);

    transitionImageLayout(resolveImage, VK_FORMAT_B8G8R8A8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1);

    commandBuffer = command_begin();

    VkImageCopy reg = {};
    reg.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    reg.srcSubresource.layerCount = 1;
    reg.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    reg.dstSubresource.layerCount = 1;
    reg.extent.width  = width;
    reg.extent.height = height;
    reg.extent.depth  = 1;

    vkCmdCopyImage(
        commandBuffer,
        resolveImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        transferImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &reg);

    command_submit(commandBuffer);

    void* data;
    
    vkMapMemory(device, transferImageMemory, 0, VK_WHOLE_SIZE, 0, &data);

    // Check if you need a custom row pitch (stride) during the copy
    VkSubresourceLayout layout;
    VkImageSubresource subResource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .arrayLayer = 0
    };
    vkGetImageSubresourceLayout(device, transferImage, &subResource, &layout);

    if (layout.rowPitch == width * sizeof(rgba8)) {
        memcpy(res.data, data, sizeof(rgba8) * height * width);
    } else {
        // If row pitch does not match, copy row by row
        uint8_t* dst = reinterpret_cast<uint8_t*>(res.data);
        uint8_t* src = reinterpret_cast<uint8_t*>(data);
        for (uint32_t y = 0; y < height; y++) {
            memcpy(dst + y * width * sizeof(rgba8), src + y * layout.rowPitch, width * sizeof(rgba8));
        }
    }
    
    vkUnmapMemory(device, transferImageMemory);
    //transitionImageLayout(transferImage, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
    transitionImageLayout(resolveImage,  VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
    return res;
}

void Device::M::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    createImage(swapChainExtent.width, swapChainExtent.height, 1, gpu->msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

VkFormat Device::M::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(gpu, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

VkFormat Device::M::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool Device::M::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void Device::M::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(gpu, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = command_begin();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    command_submit(commandBuffer);
}


VkImageView Device::M::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }

    return imageView;
}

void Device::M::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
        VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& imageMemory, void *platform_import) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = gpu->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

void Device::M::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    VkCommandBuffer commandBuffer = command_begin();

    VkImageMemoryBarrier barrier{};
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

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // Waits for transfer writes to finish
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; // Transfer reads can be started

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // The stage of the writes to wait on
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // The stage where reads will be performed
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT; // Waits for transfer writes to finish
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // Transfer reads can be started
        sourceStage           = VK_PIPELINE_STAGE_TRANSFER_BIT; // The stage of the writes to wait on
        destinationStage      = VK_PIPELINE_STAGE_TRANSFER_BIT; // The stage where reads will be performed
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

    command_submit(commandBuffer);
}

void Device::M::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = command_begin();

    VkBufferImageCopy region {
        0, 0, 0, 
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        {0, 0, 0},
        { width, height, 1 }
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    command_submit(commandBuffer);
}

VkSurfaceFormatKHR Device::M::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format     == VK_FORMAT_B8G8R8A8_UNORM && // was VK_FORMAT_B8G8R8A8_SRGB (incompatible with png/jpeg loader, internal rgba format)
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR Device::M::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Device::M::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(gpu->window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void Device::M::createLogicalDevice() {
    QueueFamilyIndices &indices = gpu->indices;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    float queuePriority = 1.0f;
    ///
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    
    /// set features used to mask of supported
    VkPhysicalDeviceFeatures featured_used { };
    featured_used.sampleRateShading = gpu->support.sampleRateShading;
	featured_used.logicOp		    = gpu->support.logicOp;
    featured_used.fillModeNonSolid  = gpu->support.fillModeNonSolid;
    featured_used.samplerAnisotropy = gpu->support.samplerAnisotropy;

    VkDeviceCreateInfo createInfo { };
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(gpu->device_extensions.size());
    createInfo.ppEnabledExtensionNames = gpu->device_extensions.data();
    createInfo.pEnabledFeatures        = &featured_used;


    if (enable_validation) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(gpu->phys, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void Device::M::createSwapChain() {
    SwapChainSupportDetails &swapChainSupport = gpu->details;//GPU::querySwapChainSupport(gpu);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = gpu->surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    

    QueueFamilyIndices &indices = gpu->indices;
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

void Device::M::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

void Device::M::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = gpu->msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = gpu->msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

/// this should be classified as a Renderer; Device is too common though
Device Device::create(Window &gpu) {
    Device dev;
    dev->gpu = gpu;
    dev->textureFormat = VK_FORMAT_B8G8R8A8_UNORM;
    dev->createLogicalDevice();
    dev->createSwapChain();
    dev->createImageViews();
    dev->createRenderPass();
    dev->createCommandPool();
    dev->createColorResources();
    dev->createDepthResources();
    dev->createFramebuffers();
    dev->createDescriptorPool();
    dev->createCommandBuffers();
    dev->createSyncObjects();
    return dev;
}

void Device::M::cleanupSwapChain() {
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    vkDestroyImageView(device, colorImageView, nullptr);
    vkDestroyImage(device, colorImage, nullptr);
    vkFreeMemory(device, colorImageMemory, nullptr);

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
}

Device::M::~M() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    
    cleanupSwapChain();
}

Texture::M::~M() {
    vkDestroySampler    (device, sampler, nullptr);
    vkDestroyImageView  (device, view,    nullptr);
    vkDestroyImage      (device, image,   nullptr);
    vkFreeMemory        (device, memory,  nullptr);
}

void Texture::M::create_image_view() {
    view = device->createImageView(
        image, device->textureFormat, VK_IMAGE_ASPECT_COLOR_BIT, device->mipLevels); // VK_FORMAT_R8G8B8A8_SRGB
}

void Texture::M::create_sampler() {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device->gpu, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(device->mipLevels);
    samplerInfo.mipLodBias = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void Device::M::loop(lambda<void(array<Pipes>&)> select) {
    array<Pipes> a_pipes;
    while (!glfwWindowShouldClose(gpu->window)) {
        /// user events fire off (events are processed by the user)
        glfwPollEvents();

        /// the user selects pipelines
        select(a_pipes);

        /// we draw pipelines
        drawFrame(a_pipes);

        /// we then reset for next frame
        a_pipes.destruct();
    }
    vkDeviceWaitIdle(device);
}

Texture &Window::M::texture(Device &dev, vec2i sz, bool sampling, VkImageUsageFlagBits usage) {
    static Texture tx = Texture();
    tx->device = dev;
    tx->format = dev->textureFormat;
    tx->usage  = usage;
    tx->msaaSamples = sampling ? dev->gpu->msaaSamples : VK_SAMPLE_COUNT_1_BIT;
    tx->create_image(sz); /// better to use one texture format for textures, and one texture format for swap.
    tx->create_image_view();
    tx->create_sampler();
    return tx;
}

void Texture::M::update_image(ion::image &img) {
    ion::rgba8 *pixels = img.data;
    vec2i sz = { int(img.width()), int(img.height()) };
    size_t image_size = sz.x * sz.y * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    device->createBuffer(image_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* vdata;
    vkMapMemory(device, stagingBufferMemory, 0, image_size, 0, &vdata);
        memcpy(vdata, pixels, static_cast<size_t>(image_size));
    vkUnmapMemory(device, stagingBufferMemory);

    device->transitionImageLayout(image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        device->mipLevels);
    device->copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(sz.x), static_cast<uint32_t>(sz.y));
    //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    /// we dont always want to do this; parameterize
    device->generateMipmaps(image, format, sz.x, sz.y, device->mipLevels);

    width      = sz.x;
    height     = sz.y;
    updated    = true;
}

void Texture::M::create_image(ion::image img, Asset type) {
    /// otherwise create a blank image; based on asset type, it will be black or white
    if (!img) {
        img = ion::size { 2, 2 };
        ion::rgba8 *pixels = img.data;
        if (type == Asset::env) /// a light map contains white by default
            memset(pixels, 255, sizeof(ion::rgba8) * (img.width() * img.height()));
    }
    ion::rgba8 *pixels = img.data;
    vec2i       sz     = { int(img.width()), int(img.height()) };

    /// set mip levels based on natural logarithm
    device->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(sz.x, sz.y)))) + 1;
    device->createImage(sz.x, sz.y, device->mipLevels,
        VK_SAMPLE_COUNT_1_BIT, format,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        image, memory); /// these are output vars
    
    update_image(img);
    asset_type = type;
}

/// needs a format specifier
void Texture::M::create_image(vec2i size) {
    int texWidth = size.x, texHeight = size.y, texChannels = 4;
    VkDeviceSize imageSize = size.x * size.y * 4;
    device->mipLevels = 1;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    device->createBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    device->createImage(texWidth, texHeight, device->mipLevels,
        msaaSamples, format, VK_IMAGE_TILING_OPTIMAL,
        usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

    device->transitionImageLayout(image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        device->mipLevels);
    device->copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    //device->generateMipmaps(image, format, texWidth, texHeight, device->mipLevels);
    width  = texWidth;
    height = texHeight;
    sz     = size;
}

ion::image Texture::asset_image(symbol name, Asset type) {
    array<ion::path> paths = {
        fmt {"textures/{0}.{1}.png", { name, type.symbol() }},
        fmt {"textures/{0}.png",     { type.symbol() }}
    };
    for (ion::path &p: paths) {
        if (p.exists())
            return ion::image(p);
    }
    return null;
}

Texture Texture::from_image(Device &dev, image img, Asset type) {
    Texture tx;
    //assert(path.exists());
    tx->device = dev;
    tx->format = dev->swapChainImageFormat;
    tx->usage  = VkImageUsageFlagBits(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    tx->create_image(img, type);
    tx->create_image_view();
    tx->create_sampler();
    return tx;
}

/// make this not a static method; change the texture already in memory
Texture Texture::load(Device &dev, symbol name, Asset type) {
    return from_image(dev, asset_image(name, type), type);
}

void Texture::update(image img) {
    data->update_image(img);
}

void Pipeline::M::createUniformBuffers() {
    VkDeviceSize bufferSize = gfx->utype->base_sz; /// was UniformBufferObject

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        device->createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers[i], uniformBuffersMemory[i]);
        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0,
            &uniformBuffersMapped[i]);
    }
}

VkVertexInputBindingDescription Pipeline::M::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = gfx->vtype->base_sz;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Pipeline::M::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    size_t        index = 0;
    doubly<prop> &props = *(doubly<prop>*)gfx->vtype->meta;
    attributeDescriptions.resize(props->len());

    auto get_format = [](prop &p) {
        if (p.member_type == typeof(glm::vec2)) return VK_FORMAT_R32G32_SFLOAT;
        if (p.member_type == typeof(glm::vec3)) return VK_FORMAT_R32G32B32_SFLOAT;
        if (p.member_type == typeof(glm::vec4)) return VK_FORMAT_R32G32B32A32_SFLOAT;
        if (p.member_type == typeof(float))     return VK_FORMAT_R32_SFLOAT;
        ///
        assert(false);
        return VK_FORMAT_UNDEFINED;
    };

    for (prop &p: props) {
        attributeDescriptions[index].binding  = 0;
        attributeDescriptions[index].location = index;
        attributeDescriptions[index].format   = get_format(p);
        attributeDescriptions[index].offset   = p.offset;
        index++;
    }

    return attributeDescriptions;
}

void Pipeline::M::start() {
    /// if we are not debugging, load pipeline and never 'reload' again
    /// (we expect resources to be in .spv form)
    if (!is_debug()) {
        reload(*this);
        init = true;
    } else {
        // while debugging we can adjust resources at runtime
        // compile .spv on load if its not there
        static lambda<void(bool, array<path_op> &)> rld;
        
        /// if there are no files it doesnt load, or reload.. thats kind of not a bug
        rld = [data=this](bool first, array<path_op> &ops) {
            console.log("compiling shaders");
            for (path_op &op: ops) {
                str ext = op->path.ext4();
                /// compile all .vert and .frag files
                /// if this is startup, all are given and we want to do that anyway
                if (ext == ".vert" || ext == ".frag") {
                    str s_path = op->path;
                    exec glslc = fmt { "glslc {0} -o {0}.spv", { s_path }};
                    int code = int(async(glslc).sync()[0]); /// these return an array of data processed
                    console.test(code == 0, "shader failed to compile: {0}", { s_path });
                }
            }
            /// if we already loaded before, cleanup resources
            data->device->mtx.lock();
            if (data->init)
                data->cleanup();

            /// perform reload
            data->reload((Pipeline::M&)*data);
            data->init = true;
            data->device->mtx.unlock();
        };

        /// spawn watcher
        watcher = watch::spawn(
            {"./shaders", "./textures", "./models"},
            {".frag", ".vert", ".png", ".obj"},
            {},
            rld);
    }
}

void Pipeline::M::assemble_graphics(Pipeline::M *pipeline, gltf::Model &m, Graphics &gfx) {
    using namespace gltf;
    
    str part = gfx->name;
    type_t vtype = pipeline->gfx->vtype;
    for (Scene &s: m->scenes) {
        /// iterate through node indices
        for (size_t inode: s->nodes) {
            Node &node = m->nodes [inode];
            /// load specific node name from node group
            if (node->name != part) continue;
            Mesh &mesh = m->meshes[node->mesh];
            for (Primitive &prim: mesh->primitives) {

                /// for each attrib we fill out the vstride
                struct vstride {
                    ion::prop      *prop;
                    type_t          compound_type;
                    Accessor::M    *accessor;
                    Buffer::M      *buffer;
                    BufferView::M  *buffer_view;
                    num             offset;
                };

                ///
                array<vstride> strides { prim->attributes->count() };
                size_t pcount = 0;
                size_t vlen = 0;
                for (field<mx> f: prim->attributes) {
                    str       prop_bind      = f.key.grab();
                    symbol    prop_sym       = symbol(prop_bind);
                    num       accessor_index = num(f.value);
                    Accessor &accessor       = m->accessors[accessor_index];

                    /// the src stride is the size of struct_type[n_components]
                    assert(vtype->meta_map);
                    prop*  p = (*vtype->meta_map)[prop_sym];
                    assert(p);

                    vstride &stride    = strides[pcount++];
                    stride.prop        = p;
                    stride.compound_type = p->member_type; /// native glm-type or float
                    stride.accessor    = accessor.data;
                    stride.buffer_view = m->bufferViews[accessor->bufferView].data;
                    stride.buffer      = m->buffers[stride.buffer_view->buffer].data;
                    stride.offset      = stride.prop->offset; /// origin at offset and stride by V type
                    
                    if (vlen)
                        assert(vlen == accessor->count);
                    else
                        vlen = accessor->count;
                    
                    if (stride.compound_type == typeof(float)) {
                        assert(accessor->componentType == gltf::ComponentType::FLOAT);
                        assert(accessor->type == gltf::CompoundType::SCALAR);
                    }
                    if (stride.compound_type == typeof(glm::vec2)) {
                        assert(accessor->componentType == gltf::ComponentType::FLOAT);
                        assert(accessor->type == gltf::CompoundType::VEC2);
                    }
                    if (stride.compound_type == typeof(glm::vec3)) {
                        assert(accessor->componentType == gltf::ComponentType::FLOAT);
                        assert(accessor->type == gltf::CompoundType::VEC3);
                    }
                    if (stride.compound_type == typeof(glm::vec4)) {
                        assert(accessor->componentType == gltf::ComponentType::FLOAT);
                        assert(accessor->type == gltf::CompoundType::VEC4);
                    }
                }
                strides.set_size(pcount);

                /// allocate entire vertex buffer for this 
                u8 *vbuf = (u8*)calloc(vlen, vtype->base_sz);
                

                /// copy data into vbuf
                for (vstride &stride: strides) {
                    /// offset into src buffer
                    u8 *dst        = vbuf;
                    num src_offset = stride.buffer_view->byteOffset;
                    /// size of member / accessor compound-type
                    num src_stride = stride.compound_type->base_sz;
                    for (num i = 0; i < vlen; i++) {
                        /// dst: vertex member position
                        u8 *member = &dst[stride.offset];
                        /// src: gltf buffer at offset: 
                        /// buffer-view + [0...accessor-count] * src-stride (same as our type size)
                        u8 *src    = &stride.buffer->uri[src_offset + src_stride * i];
                        memcpy(member, src, src_stride);

                        /// next vertex
                        dst += vtype->base_sz;
                    }
                }
                /// create vertex buffer by wrapping what we've copied from allocation (we have a primitive array)
                mx verts { memory::wrap(vtype, vbuf, vlen) }; /// load indices (always store 32bit uint)
                pipeline->createVertexBuffer(verts);

                /// indices data = indexing mesh-primitive->indices
                Accessor &a_indices = m->accessors[prim->indices];
                BufferView &view = m->bufferViews[ a_indices->bufferView ];
                Buffer      &buf = m->buffers    [ view->buffer ];
                memory *mem_indices;

                pipeline->indicesSize = a_indices->count;
                if (a_indices->componentType == ComponentType::UNSIGNED_SHORT) {
                    u16 *u16_window = (u16*)(buf->uri.data + view->byteOffset);
                    u32 *u32_alloc  = (u32*)calloc(sizeof(u32), a_indices->count);
                    for (int i = 0; i < a_indices->count; i++)
                        u32_alloc[i] = u32(u16_window[i]);
                    mem_indices = memory::wrap(typeof(u32), u32_alloc, a_indices->count);
                } else {
                    assert(a_indices->componentType == ComponentType::UNSIGNED_INT);
                    u32 *u32_window = (u32*)(buf->uri.data + view->byteOffset);
                    mem_indices = memory::window(typeof(u32), u32_window, a_indices->count);
                }
                pipeline->createIndexBuffer(mx(mem_indices));
                break;
            }
            break;
        }
    }
}

void Pipeline::M::createVertexBuffer(mx vertices) {
    VkDeviceSize bufferSize = vertices.type()->base_sz * vertices.count();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);
    void* vdata;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &vdata);
        memcpy(vdata, vertices.mem->origin, (size_t) bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);
    device->createBuffer(bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
    device->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Pipeline::M::cleanup() {
    vkDestroyPipeline(device, graphicsPipeline, null);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer (device, uniformBuffers[i],       null);
        vkFreeMemory    (device, uniformBuffersMemory[i], null);
    }

    /// delete texture here (not required i think, when we recreate it shouldnt matter that the pipeline is no longer)
    for (int i = 0; i < Asset::count; i++)
        textures[i] = Texture();

    vkFreeDescriptorSets(device, device->descriptorPool,
        uint32_t(descriptorSets.size()), descriptorSets.data());

    vkDestroyDescriptorSetLayout (device, descriptorSetLayout,  null);
    vkDestroyBuffer              (device, indexBuffer,          null);
    vkFreeMemory                 (device, indexBufferMemory,    null);
    vkDestroyBuffer              (device, vertexBuffer,         null);
    vkFreeMemory                 (device, vertexBufferMemory,   null);
    vkDestroyPipelineLayout      (device, pipelineLayout,       null);
}

Pipeline::M::~M() {
    cleanup();
    gmem->drop();
}

void Pipeline::M::createIndexBuffer(mx mx_indices) {
    assert(mx_indices.type() == typeof(u32));
    array<uint32_t> indices = mx_indices.grab();
    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.len();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* vdata;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &vdata);
        memcpy(vdata, indices.data, (size_t) bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);
    device->createBuffer(bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer, indexBufferMemory);
    device->copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

/// the array initializer is quite broken when performing a sized allocation; also code buffer is not sized
VkShaderModule Pipeline::M::createShaderModule(const array<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.len();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data);

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

void Pipeline::M::createGraphicsPipeline() {
    char vert[64];
    char frag[64];
    snprintf(vert, sizeof(vert), "shaders/%s.vert.spv", gfx->shader);
    snprintf(frag, sizeof(frag), "shaders/%s.frag.spv", gfx->shader);

    auto vertShaderCode = array<char>::read_file(vert);
    auto fragShaderCode = array<char>::read_file(frag);

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";


    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(attr_desc.size());
    vertexInputInfo.pVertexBindingDescriptions      = &binding_desc;
    vertexInputInfo.pVertexAttributeDescriptions    = attr_desc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = device->gpu->msaaSamples;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable        = VK_TRUE;
    depthStencil.depthWriteEnable       = VK_TRUE;
    depthStencil.depthCompareOp         = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable  = VK_FALSE;
    depthStencil.stencilTestEnable      = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType                 = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable         = VK_FALSE;
    colorBlending.logicOp               = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount       = 1;
    colorBlending.pAttachments          = &colorBlendAttachment;
    colorBlending.blendConstants[0]     = 0.0f;
    colorBlending.blendConstants[1]     = 0.0f;
    colorBlending.blendConstants[2]     = 0.0f;
    colorBlending.blendConstants[3]     = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType                  = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount      = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates         = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType            = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount   = 1;
    pipelineLayoutInfo.pSetLayouts      = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo { };
    pipelineInfo.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount             = 2;
    pipelineInfo.pStages                = shaderStages;
    pipelineInfo.pVertexInputState      = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState    = &inputAssembly;
    pipelineInfo.pViewportState         = &viewportState;
    pipelineInfo.pRasterizationState    = &rasterizer;
    pipelineInfo.pMultisampleState      = &multisampling;
    pipelineInfo.pDepthStencilState     = &depthStencil;
    pipelineInfo.pColorBlendState       = &colorBlending;
    pipelineInfo.pDynamicState          = &dynamicState;
    pipelineInfo.layout                 = pipelineLayout;
    pipelineInfo.renderPass             = device->renderPass;
    pipelineInfo.subpass                = 0;
    pipelineInfo.basePipelineHandle     = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}


// fix this, get this working; its basically a duplicate of 
void Pipeline::M::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding layoutBinding[1 + Asset::count]; // uniform buffer + samplers[asset_count]
    memset(&layoutBinding, 0, sizeof(layoutBinding));

    layoutBinding[0].binding = 0;
    layoutBinding[0].descriptorCount = 1;
    layoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding[0].pImmutableSamplers = nullptr;
    layoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    /// once this works, go back to array i think.
    for (int i = 0; i < Asset::count; i++) {
        layoutBinding[i+1].binding = i+1;
        layoutBinding[i+1].descriptorCount = 1;
        layoutBinding[i+1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layoutBinding[i+1].pImmutableSamplers = nullptr;
        layoutBinding[i+1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(sizeof(layoutBinding) / sizeof(VkDescriptorSetLayoutBinding));
    layoutInfo.pBindings = layoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void Pipeline::M::updateDescriptorSets() {
    bool updated = false;
    for (int a = 0; a < Asset::count; a++) { /// todo: remove 'undefined' Texture enum; very confusing idea!
        if (textures[a]->updated) {
            updated = true;
            break;
        }
    }
    if (!updated)
        return;
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo {};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = gfx->utype->base_sz;//sizeof(UniformBufferObject);

        std::array<VkWriteDescriptorSet, 1 + Asset::count> descriptorWrites { };
        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = descriptorSets[i];
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo     = &bufferInfo;

        VkDescriptorImageInfo imageInfo[Asset::count];
        memset(imageInfo, 0, sizeof(imageInfo));
        for (size_t a = 0; a < Asset::count; a++) {
            imageInfo[a].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo[a].imageView   = textures[a]->view;
            imageInfo[a].sampler     = textures[a]->sampler; /// if images are not found, they are converted into constant 2x2 image
        }

        for (int ii = 0; ii < Asset::count; ii++) {
            descriptorWrites[ii+1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[ii+1].dstSet          = descriptorSets[i];
            descriptorWrites[ii+1].dstBinding      = ii+1;
            descriptorWrites[ii+1].dstArrayElement = 0;
            descriptorWrites[ii+1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[ii+1].descriptorCount = 1;
            descriptorWrites[ii+1].pImageInfo      = &imageInfo[ii];
        }

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    for (int a = 0; a < Asset::count; a++)
        textures[a]->updated = false;
}

/// get this working, test this
void Pipeline::M::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo {};
    allocInfo.sType                 = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool        = device->descriptorPool;
    allocInfo.descriptorSetCount    = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts           = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
    updateDescriptorSets();
}

void Pipeline::M::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = device->renderPass;
    renderPassInfo.framebuffer = device->swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = device->swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width    = (float) device->swapChainExtent.width;
        viewport.height   = (float) device->swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = device->swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = {vertexBuffer}; // vertexBuffer = 0x00!
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[device->currentFrame], 0, nullptr);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesSize), 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
}

void Device::M::drawFrame(array<Pipes>& a_pipes) {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    /// select command buffer for current frame, reset fences and command buffer
    VkImage        image = swapChainImages[currentFrame];
    VkCommandBuffer cmd = commandBuffers[currentFrame];
    VkFence       fence = inFlightFences[currentFrame];
    vkResetFences(device, 1, &fence);
    vkResetCommandBuffer(cmd, 0);

    /// need another command buffer for presentation logic, or a 'Pipeline'
    VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &bi);

    /// pre commands are run prior to pipelines
    if (preCommands) preCommands(image, cmd);

    /// array pipes -> array pipelines -> pipeline
    for (Pipes &pipes: a_pipes) {
        for (Pipeline &pipeline: pipes->pipelines) {
            memory *mem = pipeline.grab();
            pipeline->uniform_update(mem); /// this user function may update textures in sync with the frame
            mem->drop();
            pipeline->updateDescriptorSets(); /// make sure textures are updated
            pipeline->recordCommandBuffer(cmd, imageIndex);
        }
    }
    /// post commands are afterwards (gfx may want to blt vector image on the screen)
    if (postCommands) postCommands(image, cmd);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

/// test this later!

vec3f average_verts(face& f, array<vec3f>& verts) {
    vec3f sum(0.0f);
    for (size_t i = 0; i < f.size; ++i)
        sum += verts[f.indices[i]];
    return sum / r32(f.size);
}

mesh subdiv(mesh& input_mesh, array<vec3f>& verts) {
    mesh sdiv_mesh;
    std::unordered_map<vpair, vec3f, vpair_hash> face_points;
    std::unordered_map<vpair, vec3f, vpair_hash> edge_points;
    array<vec3f> new_verts = verts;
    const size_t vlen = verts.len();

    /// Compute face points
    for (face& f: input_mesh) {
        vec3f face_point = average_verts(f, verts);
        for (u32 i = 0; i < f.size; ++i) {
            int vertex_index1 = f.indices[i];
            int vertex_index2 = f.indices[(i + 1) % f.size];
            vpair edge(vertex_index1, vertex_index2);
            face_points[edge] = face_point;
        }
    }

    /// Compute edge points
    for (auto& fp: face_points) {
        const  vpair &edge = fp.first;
        vec3f       midpoint = (verts[edge.first] + verts[edge.second]) / 2.0f;
        vec3f face_point_avg = (face_points[edge] + face_points[vpair(edge.second, edge.first)]) / 2.0f;
        ///
        edge_points[edge]  = (midpoint + face_point_avg) / 2.0f;
    }

    /// Update original verts
    for (size_t i = 0; i < verts.len(); ++i) {
        vec3f sum_edge_points(0.0f);
        vec3f sum_face_points(0.0f);
        u32 n = 0;

        for (const auto& ep : edge_points) {
            vpair edge = ep.first;
            if (edge.first == i || edge.second == i) {
                sum_edge_points += ep.second;
                sum_face_points += face_points[edge];
                n++;
            }
        }

        vec3f avg_edge_points = sum_edge_points / r32(n);
        vec3f avg_face_points = sum_face_points / r32(n);
        vec3f original_vertex = verts[i];

        new_verts[i] = (avg_face_points + avg_edge_points * 2.0f + original_vertex * (n - 3.0f)) / r32(n);
    }

    /// create new faces
    for (const face& f : input_mesh) {
        
        /// for each vertex in face
        for (u32 i = 0; i < f.size; ++i) {
            face new_face;
            new_face.size = 4;
            u32 new_indices[4];

            new_indices[0] = f.indices[i];
            new_indices[1] = u32(vlen + std::distance(edge_points.begin(), edge_points.find(vpair(f.indices[i], f.indices[(i + 1) % f.size]))));
            new_indices[2] = u32(vlen + std::distance(edge_points.begin(), edge_points.find(vpair(f.indices[(i + 1) % f.size], f.indices[(i + 2) % f.size]))));
            new_indices[3] = u32(vlen + std::distance(edge_points.begin(), edge_points.find(vpair(f.indices[(i + 2) % f.size], f.indices[i]))));

            new_face.indices = new_indices;
            sdiv_mesh       += new_face;
        }
    }

    return sdiv_mesh;
}

mesh subdiv_cube() {
    array<vec3f> verts = {
        // Front face
        { -0.5f, -0.5f,  0.5f },
        {  0.5f, -0.5f,  0.5f },
        {  0.5f,  0.5f,  0.5f },
        { -0.5f,  0.5f,  0.5f },

        // Back face
        { -0.5f, -0.5f, -0.5f },
        { -0.5f,  0.5f, -0.5f },
        {  0.5f,  0.5f, -0.5f },
        {  0.5f, -0.5f, -0.5f },

        // Top face
        { -0.5f,  0.5f, -0.5f },
        { -0.5f,  0.5f,  0.5f },
        {  0.5f,  0.5f,  0.5f },
        {  0.5f,  0.5f, -0.5f },

        // Bottom face
        { -0.5f, -0.5f, -0.5f },
        {  0.5f, -0.5f, -0.5f },
        {  0.5f, -0.5f,  0.5f },
        { -0.5f, -0.5f,  0.5f },

        // Right face
        {  0.5f, -0.5f, -0.5f },
        {  0.5f,  0.5f, -0.5f },
        {  0.5f,  0.5f,  0.5f },
        {  0.5f, -0.5f,  0.5f },

        // Left face
        { -0.5f, -0.5f, -0.5f },
        { -0.5f, -0.5f,  0.5f },
        { -0.5f,  0.5f,  0.5f },
        { -0.5f,  0.5f, -0.5f }
    };

    mesh input_mesh = {
        /* Size: */ 24,
        /* Indices: */ {
            0,  1,  2,  0,  2,  3,  // Front face
            4,  5,  6,  4,  6,  7,  // Back face
            8,  9,  10, 8,  10, 11, // Top face
            12, 13, 14, 12, 14, 15, // Bottom face
            16, 17, 18, 16, 18, 19, // Right face
            20, 21, 22, 20, 22, 23  // Left face
        }
    };

    return subdiv(input_mesh, verts);
}
