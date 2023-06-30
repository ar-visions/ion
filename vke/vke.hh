#pragma once

#ifdef __APPLE__
    #include <MoltenVK/vk_mvk_moltenvk.h>
#else
    #include <vulkan/vulkan.h>
#endif

#include <mx/mx.hpp>
#include <math/math.hpp>

struct GLFWmonitor;

namespace ion {

#define VKE_LOG_ERR		    0x00000001
#define VKE_LOG_DEBUG		0x00000002
#define VKE_LOG_INFO_PTS	0x00000004
#define VKE_LOG_INFO_PATH	0x00000008
#define VKE_LOG_INFO_CMD	0x00000010
#define VKE_LOG_INFO_VBO	0x00000020
#define VKE_LOG_INFO_IBO	0x00000040
#define VKE_LOG_INFO_VAO	(VKE_LOG_INFO_VBO|VKE_LOG_INFO_IBO)
#define VKE_LOG_THREAD		0x00000080
#define VKE_LOG_DBG_ARRAYS	0x00001000
#define VKE_LOG_STROKE		0x00010000
#define VKE_LOG_FULL		0xffffffff

#define VKE_LOG_INFO		0x00008000//(VKE_LOG_INFO_PTS|VKE_LOG_INFO_PATH|VKE_LOG_INFO_CMD|VKE_LOG_INFO_VAO)
#ifdef DEBUG
	extern uint32_t vke_log_level;
	#ifdef VKVG_WIRED_DEBUG
		enum vkg_wired_debug_mode {
			vkg_wired_debug_mode_normal	= 0x01,
			vkg_wired_debug_mode_points	= 0x02,
			vkg_wired_debug_mode_lines		= 0x04,
			vkg_wired_debug_mode_both		= vkg_wired_debug_mode_points|vkg_wired_debug_mode_lines,
			vkg_wired_debug_mode_all		= 0xFFFFFFFF
		};
		extern vkg_wired_debug_mode vkg_wired_debug;
	#endif
#endif

enum VkeStatus {
	VKE_STATUS_SUCCESS = 0,			    /// no error occurred.
	VKE_STATUS_NO_MEMORY,				/// out of memory
	VKE_STATUS_INVALID_RESTORE,		    /// call to #vkg_restore without matching call to #vkg_save
	VKE_STATUS_NO_CURRENT_POINT,		/// path command expecting a current point to be defined failed
	VKE_STATUS_INVALID_MATRIX,			/// invalid matrix (not invertible)
	VKE_STATUS_INVALID_STATUS,			///
	VKE_STATUS_INVALID_INDEX,			///
	VKE_STATUS_NULL_POINTER,			/// NULL pointer
	VKE_STATUS_WRITE_ERROR,			    ///
	VKE_STATUS_PATTERN_TYPE_MISMATCH,	///
	VKE_STATUS_PATTERN_INVALID_GRADIENT,/// occurs when stops count is zero
	VKE_STATUS_INVALID_FORMAT,			///
	VKE_STATUS_FILE_NOT_FOUND,			///
	VKE_STATUS_INVALID_DASH,			/// invalid value for a dash setting
	VKE_STATUS_INVALID_RECT,			/// rectangle with height or width equal to 0.
	VKE_STATUS_TIMEOUT,				    /// waiting for a vulkan operation to finish resulted in a fence timeout (5 seconds)
	VKE_STATUS_DEVICE_ERROR,			/// vkvg device initialization error 
	VKE_STATUS_INVALID_IMAGE,			///
	VKE_STATUS_INVALID_SURFACE,		    ///
	VKE_STATUS_INVALID_FONT,			/// Unresolved font name
	VKE_STATUS_ENUM_MAX = 0x7FFFFFFF
};
///
enum VkeMemoryUsage {
    VKE_MEMORY_USAGE_UNKNOWN = 0,
    VKE_MEMORY_USAGE_GPU_ONLY = 1,
    VKE_MEMORY_USAGE_CPU_ONLY = 2,
    VKE_MEMORY_USAGE_CPU_TO_GPU = 3,
    VKE_MEMORY_USAGE_GPU_TO_CPU = 4,
    VKE_MEMORY_USAGE_CPU_COPY = 5,
    VKE_MEMORY_USAGE_GPU_LAZILY_ALLOCATED = 6,
    VKE_MEMORY_USAGE_MAX_ENUM = 0x7FFFFFFF
};
struct VkeWindow;
///
typedef void (*VkeKeyEvent   )(void*, int, int, int, int);
typedef void (*VkeCharEvent  )(void*, int);
typedef void (*VkeButtonEvent)(void*, int, int, int);
typedef void (*VkeCursorEvent)(void*, double, double, bool);
typedef void (*VkeResizeEvent)(void*, int, int);
///
extern uint32_t vke_log_level;
extern FILE    *vke_out;
///
#define vke_log(level, ...) {			\
	if ((vke_log_level) & (level))		\
		fprintf(vke_out, __VA_ARGS__);	\
}

enums(VkeStencilFaceFlags, front,
    "front, back, front_and_back",
     front, back, front_and_back);

/// mx-compatible app
/// mx doesnt require inheritance to it but getting an mx type can mean getting a referencable type with meta interface. dont need to inherit anything
enums(VkeSelection, Undefined,
    "Undefined, IntegratedGPU, DiscreteGPU, VirtualGPU, CPU",
     Undefined, IntegratedGPU, DiscreteGPU, VirtualGPU, CPU);

struct VkeDevice;

/// interfacing types for simple vulkan types (industrial-strength-hair-dryer-dragged-through-desert)
/// aligned enumerables be alright but they will also be annoying to keep in sync.  just passing the enum through the mx portal for now
struct VkeSurfaceKHR:mx {
    declare(VkeSurfaceKHR, mx, struct vke_surface, VkSurfaceKHR);
    VkeSurfaceKHR(VkSurfaceKHR &surface);
};

struct VkeInstance               :mx { declare(VkeInstance,                mx, VkInstance, VkInstance); };
struct VkeFilter                 :mx { declare(VkeFilter,                  mx, VkFilter, VkFilter) };
struct VkeSamplerMipmapMode      :mx { declare(VkeSamplerMipmapMode,       mx, VkSamplerMipmapMode, VkSamplerMipmapMode) };
struct VkeSamplerAddressMode     :mx { declare(VkeSamplerAddressMode,      mx, VkSamplerAddressMode, VkSamplerAddressMode) };

struct VkeGPU:mx { declare(VkeGPU, mx, VkPhysicalDevice, VkPhysicalDevice);
    array<symbol> usable_extensions(array<symbol> input);
    //operator VkPhysicalDevice&();
    //operator VkPhysicalDevice*();
};

/*
struct VkeDeviceCreateInfo              :mx { declare(VkeDeviceCreateInfo,               mx, VkDeviceCreateInfo, VkDeviceCreateInfo) };
struct VkeObjectType                    :mx { declare(VkeObjectType,                     mx, VkObjectType, VkObjectType) };
struct VkePhysicalDeviceFeatures        :mx { declare(VkePhysicalDeviceFeatures,         mx, VkPhysicalDeviceFeatures, VkPhysicalDeviceFeatures) };
struct VkeSampleCountFlagBits           :mx { declare(VkeSampleCountFlagBits,            mx, VkSampleCountFlagBits, VkSampleCountFlagBits) };
struct VkeImageType                     :mx { declare(VkeImageType,                      mx, VkImageType, VkImageType); };
struct VkeFormat                        :mx { declare(VkeFormat,                         mx, VkFormat, VkFormat); };
struct VkeImageUsageFlags               :mx { declare(VkeImageUsageFlags,                mx, VkImageUsageFlags, VkImageUsageFlags); };
struct VkeImageTiling                   :mx { declare(VkeImageTiling,                    mx, VkImageTiling, VkImageTiling); };
struct VkePipelineStageFlags            :mx { declare(VkePipelineStageFlags,             mx, VkPipelineStageFlags, VkPipelineStageFlags) };
struct VkeResult                        :mx { declare(VkeResult,                         mx, VkResult, VkResult) };
struct VkeImageViewType                 :mx { declare(VkeImageViewType,                  mx, VkImageViewType, VkImageViewType) };
struct VkeImageAspectFlags              :mx { declare(VkeImageAspectFlags,               mx, VkImageAspectFlags, VkImageAspectFlags) };
struct VkeImageLayout                   :mx { declare(VkeImageLayout,                    mx, VkImageLayout, VkImageLayout) };
struct VkeImageSubresourceRange         :mx { declare(VkeImageSubresourceRange,          mx, VkImageSubresourceRange, VkImageSubresourceRange) };
struct VkeDescriptorImageInfo           :mx { declare(VkeDescriptorImageInfo,            mx, VkDescriptorImageInfo, VkDescriptorImageInfo) };
struct VkeBufferUsageFlags              :mx { declare(VkeBufferUsageFlags,               mx, VkBufferUsageFlags, VkBufferUsageFlags) };
struct VkeDeviceSize                    :mx { declare(VkeDeviceSize,                     mx, VkDeviceSize, VkDeviceSize) };
struct VkeDescriptorBufferInfo          :mx { declare(VkeDescriptorBufferInfo,           mx, VkDescriptorBufferInfo, VkDescriptorBufferInfo) };
struct VkeCommandPoolCreateFlags        :mx { declare(VkeCommandPoolCreateFlags,         mx, VkCommandPoolCreateFlags, VkCommandPoolCreateFlags) };
struct VkeCommandBufferLevel            :mx { declare(VkeCommandBufferLevel,             mx, VkCommandBufferLevel, VkCommandBufferLevel) };
struct VkeCommandBufferUsageFlags       :mx { declare(VkeCommandBufferUsageFlags,        mx, VkCommandBufferUsageFlags, VkCommandBufferUsageFlags) };
struct VkePresentModeKHR                :mx { declare(VkePresentModeKHR,                 mx, VkPresentModeKHR, VkPresentModeKHR) };
struct VkeDeviceQueueCreateInfo         :mx { declare(VkeDeviceQueueCreateInfo,          mx, VkDeviceQueueCreateInfo, VkDeviceQueueCreateInfo) };
struct VkePhysicalDeviceProperties      :mx { declare(VkePhysicalDeviceProperties,       mx, VkPhysicalDeviceProperties, VkPhysicalDeviceProperties) }; 
struct VkeQueueFamilyProperties         :mx { declare(VkeQueueFamilyProperties,          mx, VkQueueFamilyProperties, VkQueueFamilyProperties) }; 
struct VkePhysicalDeviceMemoryProperties:mx { declare(VkePhysicalDeviceMemoryProperties, mx, VkPhysicalDeviceMemoryProperties, VkPhysicalDeviceMemoryProperties) };
struct VkeAttachmentLoadOp              :mx { declare(VkeAttachmentLoadOp,               mx, VkAttachmentLoadOp, VkAttachmentLoadOp) }; 
struct VkeAttachmentStoreOp             :mx { declare(VkeAttachmentStoreOp,              mx, VkAttachmentStoreOp, VkAttachmentStoreOp) };
struct VkePipelineCache                 :mx { declare(VkePipelineCache,                  mx, VkPipelineCache, VkPipelineCache); };
struct VkePipelineLayout                :mx { declare(VkePipelineLayout,                 mx, VkPipelineLayout, VkPipelineLayout); };
struct VkeDescriptorSetLayout           :mx { declare(VkeDescriptorSetLayout,            mx, VkDescriptorSetLayout, VkDescriptorSetLayout); };


struct VkeClearRect:mx {
    declare(VkeClearRect, mx, VkClearRect, VkClearRect);
    VkeClearRect(rectd rect, size_t layer, size_t count);
};

struct VkeDescriptorSet:mx {
    declare(VkeDescriptorSet, mx, VkDescriptorSet, VkDescriptorSet);
};

struct VkeDescriptorPool:mx {
    declare(VkeDescriptorPool, mx, VkDescriptorPool, VkDescriptorPool);
};

struct VkeRenderPassBeginInfo:mx {
    declare(VkeRenderPassBeginInfo, mx, VkRenderPassBeginInfo, VkRenderPassBeginInfo);
};

struct VkePipeline:mx {
    declare(VkePipeline, mx, struct vke_pipeline, VkPipeline);
}; 
*/

/// these utility functions let you relate to what structure it stores it in, and what purpose it serves.  its literally 1-4 things per thing instead of spitting out 20 boilerplate lines on each
struct VkeAttachmentDescription:mx {
    declare(VkeAttachmentDescription, mx, VkAttachmentDescription, VkAttachmentDescription);
    ///
    static VkeAttachmentDescription         color(VkeFormat format, VkeSampleCountFlagBits samples, VkeAttachmentLoadOp load, VkeAttachmentStoreOp store);
    static VkeAttachmentDescription depth_stencil(VkeFormat format, VkeSampleCountFlagBits samples, VkeAttachmentLoadOp load, VkeAttachmentStoreOp store);
    static VkeAttachmentDescription color_resolve(VkeFormat format);
}; 

struct VkeRenderPass:mx {
    declare(VkeRenderPass, mx, VkRenderPass, VkRenderPass);
    VkeRenderPass(VkeDevice dev, array<VkeAttachmentDescription> desc);
};

struct VkeSemaphore:mx {
    declare(VkeSemaphore, mx, struct vke_semaphore, VkSemaphore);
    VkeSemaphore(VkeDevice &dev, uint64_t initialValue);
    bool wait(VkeDevice &dev, VkeSemaphore timeline, const uint64_t wait);
};

struct VkeCommandPool:mx {
    declare(VkeCommandPool, mx, struct vke_command_pool, VkCommandPool);
    VkeCommandPool(VkeDevice &vke, uint32_t qFamIndex, VkeCommandPoolCreateFlags flags);
};

struct VkeViewport:mx {
    declare(VkeViewport, mx, VkViewport, VkViewport);
    VkeViewport(recti area, float min_depth, float max_depth);
};

enums(VkePipelineBindPoint, graphics,
    "graphics, compute, ray_tracing",
     graphics, compute, ray_tracing);

enums(VkeShaderStageFlags, VERTEX,
    "VERTEX, TESSELLATION_CONTROL, TESSELLATION_EVALUATION, GEOMETRY, FRAGMENT, COMPUTE, ALL_GRAPHICS, ALL, RAYGEN, ANY_HIT, CLOSEST_HIT, MISS_BIT, INTERSECTION, CALLABLE, TASK, MESH",
    VERTEX                     = 0x00000001,
    TESSELLATION_CONTROL       = 0x00000002,
    TESSELLATION_EVALUATION    = 0x00000004,
    GEOMETRY                   = 0x00000008,
    FRAGMENT                   = 0x00000010,
    COMPUTE                    = 0x00000020,
    ALL_GRAPHICS               = 0x0000001F,
    ALL                        = 0x7FFFFFFF,
    RAYGEN                     = 0x00000100,
    ANY_HIT                    = 0x00000200,
    CLOSEST_HIT                = 0x00000400,
    MISS_BIT                   = 0x00000800,
    INTERSECTION               = 0x00001000,
    CALLABLE                   = 0x00002000,
    TASK                       = 0x00000040,
    MESH                       = 0x00000080
);

enums(VkeBufferUsage, TRANSFER_SRC,
    "TRANSFER_SRC, TRANSFER_DST, UNIFORM_TEXEL_BUFFER, STORAGE_TEXEL_BUFFER, UNIFORM_BUFFER, STORAGE_BUFFER, INDEX_BUFFER, VERTEX_BUFFER, INDIRECT_BUFFER",
    TRANSFER_SRC               = 0x00000001,
    TRANSFER_DST               = 0x00000002,
    UNIFORM_TEXEL_BUFFER       = 0x00000004,
    STORAGE_TEXEL_BUFFER       = 0x00000008,
    UNIFORM_BUFFER             = 0x00000010,
    STORAGE_BUFFER             = 0x00000020,
    INDEX_BUFFER               = 0x00000040,
    VERTEX_BUFFER              = 0x00000080,
    INDIRECT_BUFFER            = 0x00000100
);

enums(VkeCommandBufferUsage, one_time_submit,
    "one-time-submit, render-pass-continue, simultaneous",
     one_time_submit           = 1,
     render_pass_continue      = 2,
     simultaneous              = 4
);

enums(VkeSubpassContents, INLINE,
    "INLINE, SECONDARY_COMMAND_BUFFERS",
     INLINE                    = 0,
     SECONDARY_COMMAND_BUFFERS = 2
);

struct VkeBuffer:mx {
    declare (VkeBuffer, mx, struct vke_buffer, VkBuffer);
    ///
    VkeBuffer   (VkeDevice pDev, VkeBufferUsage usage, VkeMemoryUsage memprops, VkeDeviceSize size, bool mapped);
    ///
    void resize	(VkeDeviceSize newSize, bool mapped);
    void reset	();
    ///
    VkeDescriptorBufferInfo get_descriptor      ();
    VkeResult               map  			    ();
    void                    unmap		        ();
    void*                   get_mapped_pointer  ();
    void                    flush 			    ();
};

struct VkeCommandBuffer:mx {
    declare(VkeCommandBuffer, mx, struct vke_command_buffer, VkCommandBuffer);

    VkeCommandBuffer    (VkeDevice &vke, VkeCommandPool cmdPool, VkeCommandBufferLevel level, uint32_t count = 1);

    void begin          (VkeCommandBufferUsage flags);
    void finish         ();
    void label_start    (const char* name, const float color[4]);
    void label_insert   (const char* name, const float color[4]);
    void label_end      ();

    void bind_pipeline(
        VkePipelineBindPoint    bind,
        VkePipeline             pipeline);
    
    void bind_descriptor_sets(
        VkePipelineBindPoint      pipelineBindPoint,
        VkePipelineLayout         layout,
        compact<VkeDescriptorSet> pDescriptorSets);

    void bind_index_buffer(
        VkeBuffer               buffer,
        u64                     offset,
        type_t                  index_type);

    void bind_vertex_buffers(
        u32                     first,
        u32                     count,
        compact<VkeBuffer>      buffers,
        array<u64>              offsets);

    void draw_indexed(
        uint32_t                indexCount,
        uint32_t                instanceCount,
        uint32_t                firstIndex,
        int32_t                 vertexOffset,
        uint32_t                firstInstance);

    void draw(
        uint32_t                vertexCount,
        uint32_t                instanceCount,
        uint32_t                firstVertex,
        uint32_t                firstInstance);

    void set_stencil_compare_mask(
        VkeStencilFaceFlags            faceMask,
        uint32_t                compareMask);

    void set_stencil_reference(
        VkeStencilFaceFlags            faceMask,
        uint32_t                reference);

    void set_stencil_write_mask(
        VkeStencilFaceFlags            faceMask,
        uint32_t                writeMask);

    void begin_render_pass(
        VkeRenderPassBeginInfo  pRenderPassBegin,
        VkeSubpassContents      contents);

    void end_render_pass();

    void set_viewport(
        compact<VkeViewport> pViewports);

    void set_scissor(
        array<recti> pScissors);

    void push_constants(
        VkePipelineLayout       layout,
        VkeShaderStageFlags     stageFlags,
        uint32_t                offset,
        uint32_t                size,
        const void*             pValues);

    VkeCommandBuffer operator[](size_t i);

    operator VkCommandBuffer();
};

/// interfacing types for simple vulkan types
struct VkeMonitor:mx {
    declare(VkeMonitor, mx, GLFWmonitor*, GLFWmonitor*);
    static array<VkeMonitor> enumerate();
};

///
struct VkeImageView:mx {
    declare(VkeImageView, mx, VkImageView, VkImageView);
};

///
struct VkeEvent:mx {
    declare(VkeEvent, mx, VkEvent, VkEvent);
    VkeEvent(VkeDevice vke);
};

///
struct VkeFence:mx {
    declare(VkeFence, mx, struct vke_fence, VkFence);
    VkeFence(VkeDevice dev, bool signalled);
};

///
struct VkeQueue:mx {
    declare(VkeQueue, mx, struct vke_queue, VkQueue);
    VkeQueue(VkeDevice dev, uint32_t familyIndex, uint32_t qIndex);

    /// just using one of these.  make statics out of void args
    void submit(
		VkeFence 					 fence,
		array<VkeCommandBuffer>  	 cmd_buffers,
		array<VkePipelineStageFlags> wait_stages, /// --> must be size of wait_semaphores len() <--
		array<VkeSemaphore> 		 wait_semaphores, array<VkeSemaphore> signal_semaphores,
		array<u64> 					 wait_values, 	  array<u64> 		  signal_values);
};

/// 
struct VkePhyInfo:mx {
    declare(VkePhyInfo, mx, struct vke_phyinfo, VkPhysicalDevice);
    VkePhyInfo(VkeGPU gpu);

    //operator VkPhysicalDevice &();

    VkePhysicalDeviceProperties       get_properties       ();
    VkePhysicalDeviceMemoryProperties get_memory_properties();
    void                              get_queue_fam_indices(int* pQueue, int* gQueue, int* tQueue, int* cQueue);
    VkeQueueFamilyProperties          get_queues_props     (uint32_t* qCount);

    bool create_queues               (int qFam, uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo);
    bool create_presentable_queues   (uint32_t queueCount, const float* queue_priorities, VkeDeviceQueueCreateInfo qInfo);
    bool create_graphic_queues	     (uint32_t queueCount, const float* queue_priorities, VkeDeviceQueueCreateInfo qInfo);
    bool create_transfer_queues		 (uint32_t queueCount, const float* queue_priorities, VkeDeviceQueueCreateInfo qInfo);
    bool create_compute_queues		 (uint32_t queueCount, const float* queue_priorities, VkeDeviceQueueCreateInfo qInfo);
    bool try_get_extension_properties(const char* name, const VkExtensionProperties* properties);
    void  set_dpi                    (vec2i);
    vec2i get_dpi                    ();

    VkExtensionProperties *lookup_extension (symbol input);
    array<symbol>          usable_extensions(array<symbol> input);

    VkePhyInfo(VkeGPU gpu, VkeSurfaceKHR surface);
};

///
struct VkeApp:mx {
    declare(VkeApp, mx, struct vke_app, struct vke_app);

    array<VkePhyInfo> hardware(VkeSurfaceKHR surface);

    VkeApp(
        uint32_t v_major, uint32_t v_minor, const char* app_name,
        array<symbol> layers = {}, array<symbol> ext_instance = {});

    void enable_debug();
    static VkeApp main (array<symbol> layers = {}, array<symbol> ext_instance = {});
    static void   push (VkeApp app);
};

struct VkeWindow:mx {
    declare(VkeWindow, mx, struct vke_window, struct vke_window);
    ///
    VkeWindow(
		VkeDevice &dev, VkeMonitor mon,
        void*, int w, int h, const char *title,
		VkeResizeEvent resize_event,  VkeKeyEvent  key_event,
		VkeCursorEvent cursor_event,  VkeCharEvent char_event,
		VkeButtonEvent button_event);
    ///
    bool        should_close  ();
    bool        destroy       ();
    const char *clipboard     ();
    void        set_clipboard (const char*);
    void        set_title     (const char*);
    void        show          ();
    void        hide          ();
    void       *user_data     ();
    void        poll_events   ();
};

/// interfacing types for simple vulkan types
struct VkeFramebuffer:mx {
    declare(VkeFramebuffer, mx, struct vke_framebuffer, VkFramebuffer);
};

/// interfacing types for simple vulkan types
struct VkeSampler:mx {
    declare(VkeSampler, mx, struct vke_sampler, VkSampler);
    static VkeSampleCountFlagBits max_samples(VkeSampleCountFlagBits counts);
};

struct VkeImage:mx {
    declare(VkeImage, mx, struct vke_image, VkImage);
    
    /// consolidate into 2 or 3
    VkeImage(VkeDevice dev,  VkeImage vkImg, VkeFormat format,
             uint32_t width, uint32_t height);
    
    VkeImage(VkeDevice dev, VkeImageType imageType, VkeFormat format,
             uint32_t               width,     uint32_t           height,
             VkeMemoryUsage         memprops,  VkeImageUsageFlags usage,
             VkeSampleCountFlagBits samples,   VkeImageTiling     tiling,
		     uint32_t               mipLevels, uint32_t           arrayLayers);
    
    VkeImage(VkeDevice pDev, VkFormat format , VkSampleCountFlagBits num_samples,
             uint32_t width, uint32_t height,  VkeMemoryUsage memprops, VkImageUsageFlags usage);
    
    /// this is 'basic'
    VkeImage(VkeDevice dev, VkeFormat format, uint32_t width, uint32_t height, VkeImageTiling tiling,
			 VkeMemoryUsage memprops, VkeImageUsageFlags usage);
    
    VkeImage(VkeDevice dev, VkeFormat format, VkeSampleCountFlagBits num_samples, uint32_t width, uint32_t height,
			 VkeMemoryUsage memprops, VkeImageUsageFlags usage);

    ///
    static VkeImage tex2d_array_create(VkeDevice pDev, VkeFormat format, uint32_t width, uint32_t height, uint32_t layers, VkeMemoryUsage memprops, VkeImageUsageFlags usage);

    void set_sampler      (VkeSampler       sampler);
    void create_descriptor(VkeImageViewType viewType,  VkeImageAspectFlags aspectFlags, VkeFilter magFilter, VkeFilter minFilter, VkeSamplerMipmapMode mipmapMode, VkeSamplerAddressMode addressMode);
    void create_view      (VkeImageViewType viewType,  VkeImageAspectFlags aspectFlags);
    void create_sampler   (VkeFilter        magFilter, VkeFilter minFilter,  VkeSamplerMipmapMode mipmapMode, VkeSamplerAddressMode addressMode);
    void set_layout       (VkeCommandBuffer cmdBuff,   VkeImageAspectFlags      aspectMask,       VkeImageLayout        old_image_layout, VkeImageLayout new_image_layout, VkePipelineStageFlags src_stages, VkePipelineStageFlags dest_stages);
    void set_layout_subres(VkeCommandBuffer cmdBuff,   VkeImageSubresourceRange subresourceRange, VkeImageLayout        old_image_layout, VkeImageLayout new_image_layout, VkePipelineStageFlags src_stages, VkePipelineStageFlags dest_stages);

    void     destroy_sampler();
    VkeImage drop           ();
    void     reference      ();
    void*    map            ();
    void     unmap          ();
    void     set_name       (const char* name);
    uint64_t get_stride	    ();

    VkeDescriptorImageInfo get_descriptor(VkeImageLayout imageLayout);
};

/// most should be reducable here.  using a extern / intern pattern containing the Vk internals
/// we want a simplistic interface but not hindered; something that isolates vulkan

struct VkeDevice:mx {
    declare(VkeDevice, mx, struct vke_device, VkDevice);
    ///
    VkeDevice(VkeApp app, VkePhyInfo phyInfo, VkeDeviceCreateInfo pDevice_info);
    VkeDevice(VkeInstance inst, VkeGPU gpu, VkeDevice vk);
    ///
    void              init_debug_utils       ();
    VkeDevice         vk                     ();
    VkeGPU            gpu                    ();
    VkePhyInfo        phyinfo                ();
    VkeApp	          app                    ();
    void              set_object_name        (VkeObjectType objectType, uint64_t handle, const char *name);
    VkeSampler        create_sampler         (VkeFilter magFilter, VkeFilter minFilter, VkeSamplerMipmapMode mipmapMode, VkeSamplerAddressMode addressMode);
    void              destroy_sampler        (VkeSampler sampler);
    const void*       get_device_requirements(VkePhysicalDeviceFeatures pEnabledFeatures);
    int               present_index          ();
    int               transfer_index         ();
    int               graphics_index         ();
    int               compute_index          ();

    static bool layer_is_present(const char* layerName);
    static VkeDevice select_device(VkeSelection selection, array<symbol> required_exts);
};

struct VkePresenter:mx {
    declare(VkePresenter, mx, struct vke_presenter, struct vke_presenter);
    ///
    VkePresenter(VkeDevice dev,  uint32_t presentQueueFamIdx, VkeSurfaceKHR surface,
                 uint32_t width, uint32_t height, VkeFormat preferedFormat, VkePresentModeKHR presentMode);
    bool draw              ();
    bool acquireNextImage  (VkeFence fence, VkeSemaphore semaphore);
    void build_blit_cmd    (VkeImage blitSource, uint32_t width, uint32_t height);
    void create_swapchain  ();
    void get_size          (uint32_t* pWidth, uint32_t* pHeight);
    void swapchain_destroy ();

    void init(VkeFormat preferedFormat, VkePresentModeKHR presentMode);
};

/// timeline is VkeSemaphore with a time value
struct VkeShaderModule:mx {
    declare(VkeShaderModule, mx, VkShaderModule, VkShaderModule);
    static VkeShaderModule load(VkeDevice vke, const char* path);
};

bool           vke_memory_type_from_properties(VkPhysicalDeviceMemoryProperties* memory_properties, uint32_t typeBits, VkeMemoryUsage requirements_mask, uint32_t *typeIndex);

char *      read_spv(const char *filename, size_t *psize);
uint32_t*   readFile(uint32_t* length, const char* filename);
void        dumpLayerExts();
void        set_image_layout(VkeCommandBuffer cmdBuff, VkeImage image, VkeImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkePipelineStageFlags dest_stages);
void        set_image_layout_subres(VkeCommandBuffer cmdBuff, VkeImage image, VkeImageSubresourceRange subresourceRange, VkeImageLayout old_image_layout, VkeImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkePipelineStageFlags dest_stages);


#define FENCE_TIMEOUT 100000000

VkeDevice vke_select_device(VkPhysicalDeviceType);

/// vector of additional extensions given to select device would also be ok
/// but, there are three extension systems in vulkan instance extension, device extension and feature extensions
/// i think its preferred to have them all in one spot.
/// it would be odd to have multiple vulkan instances with different effective sets.
/// who would enjoy doing that im not sure, but better to keep it simple for now
void configure(bool _validation, bool _mesa_overlay, bool _render_doc);

/// all of vke_* and vkg_* objects should be
/// replace all cases of calloc and malloc with this
/// remove mutex references
/// use io_grab() inside of object_reference()
/// if they were called vke_grab() the user could make use of it


}