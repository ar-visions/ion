#pragma once
#include <mx/mx.hpp>
#include <math/math.hpp>

namespace ion {
enum VkeStatus {
	VKE_STATUS_SUCCESS = 0,			    /// no error occurred.
	VKE_STATUS_NO_MEMORY,				/// out of memory
	VKE_STATUS_INVALID_RESTORE,		    /// call to #vkvg_restore without matching call to #vkvg_save
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

/// mx-compatible app
/// mx doesnt require inheritance to it but getting an mx type can mean getting a referencable type with meta interface. dont need to inherit anything
enums(VkeSelection, Undefined,
    "Undefined, IntegratedGPU, DiscreteGPU, VirtualGPU, CPU",
     Undefined, IntegratedGPU, DiscreteGPU, VirtualGPU, CPU);

struct VkeDevice;

/// interfacing types for simple vulkan types (industrial-strength-hair-dryer-dragged-through-desert)
/// aligned enumerables be alright but they will also be annoying to keep in sync.  just passing the enum through the mx portal for now
struct VkeSurfaceKHR             :mx { ptr_declare(VkeSurfaceKHR,              mx, VkSurfaceKHR);             };
struct VkeInstance               :mx { ptr_declare(VkeInstance,                mx, VkInstance);               };
struct VkeFilter                 :mx { ptr_declare(VkeFilter,                  mx, VkFilter)                  };
struct VkeSamplerMipmapMode      :mx { ptr_declare(VkeSamplerMipmapMode,       mx, VkSamplerMipmapMode)       };
struct VkeSamplerAddressMode     :mx { ptr_declare(VkeSamplerAddressMode,      mx, VkSamplerAddressMode)      };

bool _get_dev_extension_is_supported (VkExtensionProperties* pExtensionProperties, uint32_t extensionCount, const char* name) {
	for (uint32_t i=0; i<extensionCount; i++) {
		if (strcmp(name, pExtensionProperties[i].extensionName)==0)
			return true;
	}
	return false;
}

struct VkeGPU:mx { ptr_declare(VkeGPU, mx, VkPhysicalDevice);
    array<symbol> usable_extensions(array<symbol> input);
};

struct VkeDeviceCreateInfo              :mx { ptr_declare(VkeDeviceCreateInfo,               mx, VkDeviceCreateInfo)               };
struct VkeObjectType                    :mx { ptr_declare(VkeObjectType,                     mx, VkObjectType)                     };
struct VkePhysicalDeviceFeatures        :mx { ptr_declare(VkePhysicalDeviceFeatures,         mx, VkPhysicalDeviceFeatures)         };
struct VkeSampleCountFlagBits           :mx { ptr_declare(VkeSampleCountFlagBits,            mx, VkSampleCountFlagBits)            };
struct VkeImageType                     :mx { ptr_declare(VkeImageType,                      mx, VkImageType);                     };
struct VkeFormat                        :mx { ptr_declare(VkeFormat,                         mx, VkFormat);                        };
struct VkeImageUsageFlags               :mx { ptr_declare(VkeImageUsageFlags,                mx, VkImageUsageFlags);               };
struct VkeImageTiling                   :mx { ptr_declare(VkeImageTiling,                    mx, VkImageTiling);                   };
struct VkePipelineStageFlags            :mx { ptr_declare(VkePipelineStageFlags,             mx, VkPipelineStageFlags)             };
struct VkeResult                        :mx { ptr_declare(VkeResult,                         mx, VkResult)                         };
struct VkeImageViewType                 :mx { ptr_declare(VkeImageViewType,                  mx, VkImageViewType)                  };
struct VkeImageAspectFlags              :mx { ptr_declare(VkeImageAspectFlags,               mx, VkImageAspectFlags)               };
struct VkeImageLayout                   :mx { ptr_declare(VkeImageLayout,                    mx, VkImageLayout)                    };
struct VkeImageSubresourceRange         :mx { ptr_declare(VkeImageSubresourceRange,          mx, VkImageSubresourceRange)          };
struct VkeDescriptorImageInfo           :mx { ptr_declare(VkeDescriptorImageInfo,            mx, VkDescriptorImageInfo)            };
struct VkeBufferUsageFlags              :mx { ptr_declare(VkeBufferUsageFlags,               mx, VkBufferUsageFlags)               };
struct VkeDeviceSize                    :mx { ptr_declare(VkeDeviceSize,                     mx, VkDeviceSize)                     };
struct VkeDescriptorBufferInfo          :mx { ptr_declare(VkeDescriptorBufferInfo,           mx, VkDescriptorBufferInfo)           };
struct VkeCommandPoolCreateFlags        :mx { ptr_declare(VkeCommandPoolCreateFlags,         mx, VkCommandPoolCreateFlags)         };
struct VkeCommandBufferLevel            :mx { ptr_declare(VkeCommandBufferLevel,             mx, VkCommandBufferLevel)             };
struct VkeCommandBufferUsageFlags       :mx { ptr_declare(VkeCommandBufferUsageFlags,        mx, VkCommandBufferUsageFlags)        };
struct VkePresentModeKHR                :mx { ptr_declare(VkePresentModeKHR,                 mx, VkPresentModeKHR)                 };
struct VkeDeviceQueueCreateInfo         :mx { ptr_declare(VkeDeviceQueueCreateInfo,          mx, VkDeviceQueueCreateInfo)          };
struct VkePhysicalDeviceProperties      :mx { ptr_declare(VkePhysicalDeviceProperties,       mx, VkPhysicalDeviceProperties)       }; 
struct VkeQueueFamilyProperties         :mx { ptr_declare(VkeQueueFamilyProperties,          mx, VkQueueFamilyProperties)          }; 
struct VkePhysicalDeviceMemoryProperties:mx { ptr_declare(VkePhysicalDeviceMemoryProperties, mx, VkPhysicalDeviceMemoryProperties) }; 

struct VkeSemaphore:mx {
    ptr_declare(VkeSemaphore, mx, VkSemaphore);
    VkeSemaphore(VkeDevice &dev, uint64_t initialValue);
    bool wait(VkeDevice &dev, VkeSemaphore timeline, const uint64_t wait);
};

struct VkeCommandPool:mx {
    ptr_declare(VkeCommandPool, mx, VkCommandPool);
    VkeCommandPool(VkeDevice &vke, uint32_t qFamIndex, VkeCommandPoolCreateFlags flags);
};

struct VkeCommandBuffer:mx {
    ptr_declare(VkeCommandBuffer, mx, VkCommandBuffer);

    VkeCommandBuffer    (VkeDevice &vke, VkeCommandPool cmdPool, VkeCommandBufferLevel level, uint32_t count = 1);
    void cmd_begin      (VkeCommandBufferUsageFlags flags);
    void cmd_end        ();
    void label_start    (const char* name, const float color[4]);
    void label_insert   (const char* name, const float color[4]);
    void label_end      ();
};

/// interfacing types for simple vulkan types
struct VkeMonitor:mx {
    ptr_declare(VkeMonitor, mx, GLFWmonitor*);
    static array<VkeMonitor> enumerate();
};

///
struct VkeImageView:mx {
    ptr_declare(VkeImageView, mx, VkImageView);
};

///
struct VkeEvent:mx {
    ptr_declare(VkeEvent, mx, VkEvent);
    VkeEvent(VkeDevice vke);
};

///
struct VkeFence:mx {
    ptr_declare(VkeFence, mx, VkFence);
    VkeFence(VkeDevice dev, bool signalled);
};

///
struct VkeQueue:mx {
    ptr_declare(VkeQueue, mx, struct vke_queue);
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
    ptr_declare(VkePhyInfo, mx, struct vke_phyinfo);
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
    ptr_declare(VkeApp, mx, struct vke_app);

    array<VkePhyInfo> hardware(VkeSurfaceKHR surface);

    VkeApp(
        uint32_t v_major, uint32_t v_minor, const char* app_name,
        array<symbol> layers = {}, array<symbol> ext_instance = {});

    void enable_debug();

    

    static VkeApp main (array<symbol> layers = {}, array<symbol> ext_instance = {});
    static void   push (VkeApp app);
};

struct VkeWindow:mx {
    ptr_declare(VkeWindow, mx, struct vke_window);
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
struct VkeSampler:mx {
    ptr_declare(VkeSampler, mx, struct vke_sampler);
    static VkeSampleCountFlagBits max_samples(VkeSampleCountFlagBits counts);
    operator VkSampler &();
};

struct VkeImage:mx {
    ptr_declare(VkeImage, mx, struct vke_image);
    
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

    operator VkImage &();

    VkeDescriptorImageInfo get_descriptor(VkeImageLayout imageLayout);
};

/// most should be reducable here.  using a extern / intern pattern containing the Vk internals
/// we want a simplistic interface but not hindered; something that isolates vulkan

struct VkeDevice:mx {
    ptr_declare(VkeDevice, mx, struct vke_device);
    ///
    VkeDevice(VkeApp app, VkePhyInfo phyInfo, VkeDeviceCreateInfo pDevice_info);
    VkeDevice(VkeInstance inst, VkeGPU gpu, VkeDevice vk);
    ///
    operator VkDevice &();
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

struct VkeBuffer:mx {
    ptr_declare (VkeBuffer, mx, struct vke_buffer);
    ///
    VkeBuffer   (VkeDevice pDev, VkeBufferUsageFlags usage, VkeMemoryUsage memprops, VkeDeviceSize size, bool mapped);
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

struct VkePresenter:mx {
    ptr_declare(VkePresenter, mx, struct vke_presenter);
    ///
    VkePresenter(VkeDevice dev,  uint32_t presentQueueFamIdx, VkeSurfaceKHR surface,
                 uint32_t width, uint32_t height, VkeFormat preferedFormat, VkePresentModeKHR presentMode);
    bool draw              ();
    bool acquireNextImage  (VkeFence fence, VkeSemaphore semaphore);
    void build_blit_cmd    (VkeImage blitSource, uint32_t width, uint32_t height);
    void create_swapchain  ();
    void get_size          (uint32_t* pWidth, uint32_t* pHeight);
    void swapchain_destroy ();
};

/// timeline is VkeSemaphore with a time value
struct VkeShaderModule:mx {
    ptr_declare(VkeShaderModule, mx, VkShaderModule);
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
void vke_configure(bool _validation, bool _mesa_overlay, bool _render_doc);

/// all of vke_* and vkg_* objects should be
/// replace all cases of calloc and malloc with this
/// remove mutex references
/// use io_grab() inside of object_reference()
/// if they were called vke_grab() the user could make use of it


}