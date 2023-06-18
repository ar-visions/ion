#ifndef VK_HELPERS_H
#define VK_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

/// handlers wrappers
#include <vke/vulkan.h>
#include <io/io.h>

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
///
typedef void (*VkeKeyEvent   )(VkeWindow, int, int, int, int);
typedef void (*VkeCharEvent  )(VkeWindow, int);
typedef void (*VkeButtonEvent)(VkeWindow, int, int, int);
typedef void (*VkeCursorEvent)(VkeWindow, double, double, bool);
typedef void (*VkeResizeEvent)(VkeWindow, int, int);
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
     Undefined, IntegratedGPU, DiscreteGPU, VirtualGPU, CPU
);

/// interfacing types for simple vulkan types
struct VkeSurface:mx {
    using intern = VkSurfaceKHR;
    ptr_declare(VkeSurface);
};

/// interfacing types for simple vulkan types
struct VkeMonitor:mx {
    using intern = GLFWmonitor;
    ptr_declare(VkeMonitor);
    static array<VkeMonitor> enumerate();
};

/// Dont want this to conflict too much with VkePhysInfo
struct VkeGPU:mx {
    using intern = VkPhysicalDevice;
    ptr_declare(VkeGPU);
};
///

struct VkeApp:mx {
    using intern = struct vke_app; /// take data param out of ptr/ctr, its just intern for ptr and data for ctr
    ptr_declare(VkeApp);

    VkeApp(uint32_t v_major, uint32_t v_minor, const char* app_name,
           uint32_t layersCount, const char **layers, uint32_t ext_count, const char* extentions[]);

    array<VkePhyInfo> phyinfos(VkeSurfaceKHR surface);

    void enable_debug_messenger(VkeApp app, VkDebugUtilsMessageTypeFlagsEXT typeFlags, VkDebugUtilsMessageSeverityFlagsEXT severityFlags, PFN_vkDebugUtilsMessengerCallbackEXT callback);
    
    static VkeApp main ();
    static void   push (VkeApp app);
};

struct VkePhyInfo:mx {
    VkePhyInfo(VkePhysicalDevice phy, VkeSurfaceKHR surface);
    VkePhyInfo                       vke_phyinfo_grab                 (VkePhyInfo phy);
    VkePhyInfo                       vke_phyinfo_drop                 (VkePhyInfo phy);
    VkPhysicalDeviceProperties       vke_phyinfo_get_properties       (VkePhyInfo phy);
    VkPhysicalDeviceMemoryProperties vke_phyinfo_get_memory_properties(VkePhyInfo phy);
    void                             vke_phyinfo_get_queue_fam_indices(VkePhyInfo phy, int* pQueue, int* gQueue, int* tQueue, int* cQueue);
    VkQueueFamilyProperties*         vke_phyinfo_get_queues_props(VkePhyInfo phy, uint32_t* qCount);

    bool create_queues(int qFam, uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo);
    bool create_presentable_queues(uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo);
    bool create_graphic_queues	     (uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo);
    bool create_transfer_queues		 (uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo);
    bool create_compute_queues		 (uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo);
    bool try_get_extension_properties(const char* name, const VkExtensionProperties* properties);
    void set_dpi                     (int  hdpi, int  vdpi);
    void get_dpi                     (int *hdpi, int *vdpi);
};

/// interfacing types for simple vulkan types
struct VkeSampler:mx {
    using intern = VkSampler;
    ptr_declare(VkeSampler);
    void destructor();
};


struct VkeDevice:mx {
    ptr_declare(VkeDevice);
    ///
    VkeDevice(VkeApp app, VkePhyInfo phyInfo, VkDeviceCreateInfo* pDevice_info);
    VkeDevice(VkInstance inst, VkPhysicalDevice phy, VkDevice vkdev);
    ///
    VkeDevice        drop             (VkeDevice dev);
    void             init_debug_utils (VkeDevice dev);
    VkDevice         vk               (VkeDevice dev);
    VkPhysicalDevice gpu              (VkeDevice dev);
    VkePhyInfo       phyinfo          (VkeDevice dev);
    VkeApp	         app              (VkeDevice dev);
    void             set_object_name  (VkeDevice dev, VkObjectType objectType, uint64_t handle, const char *name);
    VkeSampler       create_sampler   (VkeDevice dev, VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode);
    void             destroy_sampler  (VkeDevice dev, VkSampler sampler);
    const void*      get_device_requirements (VkPhysicalDeviceFeatures* pEnabledFeatures);
    int              device_present_index    (VkeDevice);
    int              device_transfer_index   (VkeDevice);
    int              device_graphics_index   (VkeDevice);
    int              vke_device_compute_index    (VkeDevice);
};

using VkeApp                    = vke_app*;

using VkeDevice                 = vke_device*;
using VkeImage                  = vke_image*;
using VkeBuffer                 = vke_buffer*;
using VkeQueue                  = vke_queue*;
using VkePresenter              = vke_presenter*;
using VkeWindow                 = vke_window*;
using VkeMonitor                = vke_monitor*;

///VkeDevice interface uses an internal type vke_device (data)
using VkeDevice = sp<vke_device>;



/**
 * @brief query required instance extensions for vkvg.
 *
 * @param pExtensions a valid pointer to the array of extension names to fill, the size may be queried
 * by calling this method with pExtension being a NULL pointer.
 * @param pExtCount a valid pointer to an integer that will be fill with the required extension count.
 */

struct VkePresenter:mx {
    void vke_get_required_instance_extensions (const char** pExtensions, uint32_t* pExtCount);

    VkePresenter vke_presenter_create            (VkeDevice dev, uint32_t presentQueueFamIdx, VkSurfaceKHR surface, uint32_t width, uint32_t height, VkFormat preferedFormat, VkPresentModeKHR presentMode);
    VkePresenter vke_presenter_drop              (VkePresenter r);
    bool         vke_presenter_draw              (VkePresenter r);
    bool         vke_presenter_acquireNextImage  (VkePresenter r, VkFence fence, VkSemaphore semaphore);
    void         vke_presenter_build_blit_cmd    (VkePresenter r, VkImage blitSource, uint32_t width, uint32_t height);
    void         vke_presenter_create_swapchain  (VkePresenter r);
    void	        vke_presenter_get_size		    (VkePresenter r, uint32_t* pWidth, uint32_t* pHeight);
};


struct VkeSamples {
    VkSampleCountFlagBits max_samples(VkSampleCountFlagBits counts);
};


vke_public
VkeImage vke_image_import       (VkeDevice pDev, VkImage vkImg, VkFormat format, uint32_t width, uint32_t height);


vke_public
VkeImage vke_image_create(VkeDevice pDev, VkImageType imageType,
		VkFormat format, uint32_t width, uint32_t height,
		VkeMemoryUsage memprops, VkImageUsageFlags usage,
		VkSampleCountFlagBits samples, VkImageTiling tiling,
		uint32_t mipLevels, uint32_t arrayLayers);

vke_public
VkeImage vke_image_ms_create    (VkeDevice pDev, VkFormat format, VkSampleCountFlagBits num_samples, uint32_t width, uint32_t height,
                                                                        VkeMemoryUsage memprops, VkImageUsageFlags usage);
vke_public
VkeImage vke_tex2d_array_create (VkeDevice pDev, VkFormat format, uint32_t width, uint32_t height, uint32_t layers,
                                                                        VkeMemoryUsage memprops, VkImageUsageFlags usage);
vke_public
void vke_image_set_sampler      (VkeImage img, VkSampler sampler);

vke_public
void vke_image_create_descriptor(VkeImage img, VkImageViewType viewType, VkImageAspectFlags aspectFlags, VkFilter magFilter, VkFilter minFilter,
                                                                        VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode);
vke_public
void vke_image_create_view      (VkeImage img, VkImageViewType viewType, VkImageAspectFlags aspectFlags);
vke_public
void vke_image_create_sampler   (VkeImage img, VkFilter magFilter, VkFilter minFilter,
                                                                        VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode);
vke_public
void vke_image_set_layout       (VkCommandBuffer cmdBuff, VkeImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout,
                                                                        VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages);
vke_public
void vke_image_set_layout_subres(VkCommandBuffer cmdBuff, VkeImage image, VkImageSubresourceRange subresourceRange, VkImageLayout old_image_layout,
                                                                        VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages);
void     vke_image_destroy_sampler(VkeImage img);
VkeImage vke_image_drop           (VkeImage img);
void     vke_image_reference		 (VkeImage img);
void*    vke_image_map            (VkeImage img);
void     vke_image_unmap          (VkeImage img);
void     vke_image_set_name       (VkeImage img, const char* name);
uint64_t vke_image_get_stride	 (VkeImage img);

vke_public
VkImage                 vke_image_get_vkimage   (VkeImage img);
vke_public
VkImageView             vke_image_get_view      (VkeImage img);
vke_public
VkImageLayout           vke_image_get_layout    (VkeImage img);
vke_public
VkSampler               vke_image_get_sampler   (VkeImage img);
vke_public
VkDescriptorImageInfo   vke_image_get_descriptor(VkeImage img, VkImageLayout imageLayout);

/*************
 * VkeBuffer *
 *************/
vke_public
void		vke_buffer_init		(VkeDevice pDev, VkBufferUsageFlags usage,
                                                                        VkeMemoryUsage memprops, VkDeviceSize size, VkeBuffer buff, bool mapped);
vke_public
VkeBuffer   vke_buffer_create   (VkeDevice pDev, VkBufferUsageFlags usage,
                                                                        VkeMemoryUsage memprops, VkDeviceSize size);

vke_public
void		vke_buffer_resize	(VkeBuffer buff, VkDeviceSize newSize, bool mapped);
vke_public
void		vke_buffer_reset	(VkeBuffer buff);
vke_public
VkResult    vke_buffer_map      (VkeBuffer buff);
vke_public
void        vke_buffer_unmap    (VkeBuffer buff);
vke_public
void		vke_buffer_flush	(VkeBuffer buff);

vke_public
VkBuffer    vke_buffer_get_vkbuffer			(VkeBuffer buff);
vke_public
void*       vke_buffer_get_mapped_pointer	(VkeBuffer buff);

vke_public
VkFence         vke_fence_create			(VkeDevice dev);
vke_public
VkFence         vke_fence_create_signaled	(VkeDevice dev);
vke_public
VkSemaphore     vke_semaphore_create		(VkeDevice dev);
vke_public
VkSemaphore		vke_timeline_create			(VkeDevice dev, uint64_t initialValue);
vke_public
VkResult		vke_timeline_wait			(VkeDevice dev, VkSemaphore timeline, const uint64_t wait);
vke_public
void			vke_cmd_submit_timelined	(VkeQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore timeline,
                                                                                                 const uint64_t wait, const uint64_t signal);
vke_public
void			vke_cmd_submit_timelined2	(VkeQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore timelines[2],
                                                                                                const uint64_t waits[2], const uint64_t signals[2]);
vke_public
VkEvent			vke_event_create				(VkeDevice dev);

vke_public
VkCommandPool   vke_cmd_pool_create (VkeDevice dev, uint32_t qFamIndex, VkCommandPoolCreateFlags flags);
vke_public
VkCommandBuffer vke_cmd_buff_create (VkeDevice dev, VkCommandPool cmdPool, VkCommandBufferLevel level);
vke_public
void vke_cmd_buffs_create (VkeDevice dev, VkCommandPool cmdPool, VkCommandBufferLevel level, uint32_t count, VkCommandBuffer* cmdBuffs);
vke_public
void vke_cmd_begin  (VkCommandBuffer cmdBuff, VkCommandBufferUsageFlags flags);
vke_public
void vke_cmd_end    (VkCommandBuffer cmdBuff);
vke_public
void vke_cmd_submit (VkeQueue queue, VkCommandBuffer *pCmdBuff, VkFence fence);
vke_public
void vke_cmd_submit_with_semaphores(VkeQueue queue, VkCommandBuffer *pCmdBuff, VkSemaphore waitSemaphore,
                                                                        VkSemaphore signalSemaphore, VkFence fence);

vke_public
void vke_cmd_label_start   (VkCommandBuffer cmd, const char* name, const float color[4]);
vke_public
void vke_cmd_label_end     (VkCommandBuffer cmd);
vke_public
void vke_cmd_label_insert  (VkCommandBuffer cmd, const char* name, const float color[4]);

vke_public
VkShaderModule vke_load_module(VkeDevice vke, const char* path);

vke_public
bool        vke_memory_type_from_properties(VkPhysicalDeviceMemoryProperties* memory_properties, uint32_t typeBits,
                                                                                VkeMemoryUsage requirements_mask, uint32_t *typeIndex);
vke_public
char *      read_spv(const char *filename, size_t *psize);
vke_public
uint32_t*   readFile(uint32_t* length, const char* filename);

vke_public
void dumpLayerExts ();

vke_public
void        set_image_layout(VkCommandBuffer cmdBuff, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout,
                                          VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages);
vke_public
void        set_image_layout_subres(VkCommandBuffer cmdBuff, VkImage image, VkImageSubresourceRange subresourceRange, VkImageLayout old_image_layout,
                                          VkImageLayout new_image_layout, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages);
/////////////////////
vke_public
VkeQueue    vke_queue_create    (VkeDevice dev, uint32_t familyIndex, uint32_t qIndex);
vke_public
VkeQueue    vke_queue_drop      (VkeQueue queue);
VkeQueue    vke_queue_grab      (VkeQueue queue);
//VkeQueue    vke_queue_find      (VkeDevice dev, VkQueueFlags flags);
/////////////////////

vke_public
bool vke_instance_extension_supported (const char* instanceName);
vke_public
void vke_instance_extensions_check_init ();
vke_public
void vke_instance_extensions_check_release ();
vke_public
bool vke_layer_is_present (const char* layerName);
vke_public
void vke_layers_check_init ();
vke_public
void vke_layers_check_release ();
#ifdef __cplusplus
}

#endif


#define FENCE_TIMEOUT 100000000

typedef struct _vk_engine_t* VkEngine;

/// deprecate vk_engine_t 
typedef struct _vk_engine_t {
	VkeApp app;
} vk_engine_t;


/**
vk_engine_t*        vkengine_create                     (VkPhysicalDeviceType preferedGPU, VkPresentModeKHR presentMode, uint32_t width, uint32_t height);
void                vkengine_dump_available_layers      ();
bool                vkengine_try_get_phyinfo            (VkePhyInfo* phys, uint32_t phyCount, VkPhysicalDeviceType gpuType, VkePhyInfo* phy);
void                vkengine_destroy		            (VkEngine e);
bool                vkengine_should_close	            (VkEngine e);
void                vkengine_close			            (VkEngine e);

void                vkengine_dump_Infos      	        (VkEngine e);
void                vkengine_set_title		            (VkEngine e, const char* title);
VkInstance			vkengine_get_instance		        (VkEngine e);
VkDevice			vkengine_get_device			        (VkEngine e);
VkPhysicalDevice	vkengine_get_physical_device        (VkEngine e);
VkQueue				vkengine_get_queue			        (VkEngine e);
uint32_t			vkengine_get_queue_fam_idx	        (VkEngine e);
void                vkengine_get_queues_properties      (vk_engine_t* e, VkQueueFamilyProperties** qFamProps, uint32_t* count);
void                vkengine_set_key_callback			(VkEngine e, GLFWkeyfun key_callback);
void                vkengine_set_mouse_but_callback	    (VkEngine e, GLFWmousebuttonfun onMouseBut);
void                vkengine_set_cursor_pos_callback	(VkEngine e, GLFWcursorposfun onMouseMove);
void                vkengine_set_scroll_callback		(VkEngine e, GLFWscrollfun onScroll);
void                vkengine_set_char_callback			(VkEngine e, GLFWcharfun onChar);

void vkengine_wait_idle (VkEngine e);
*/

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



VkeWindow   vke_create_window        (VkeDevice, VkeMonitor, int, int, const char *, VkeResizeEvent, VkeKeyEvent, VkeCursorEvent, VkeCharEvent, VkeButtonEvent);
bool        vke_should_close         (VkeWindow);
bool        vke_destroy_window       (VkeWindow);
const char *vke_clipboard            (VkeWindow); /// user data integrity in streaming should be preserved; having a window is top level requirement but i think there would be more to do to facilitate
void        vke_set_clipboard_string (VkeWindow, const char*);
void        vke_set_window_title     (VkeWindow, const char*);
void        vke_show_window          (VkeWindow win);
void        vke_hide_window          (VkeWindow win);
void       *vke_window_user_data     (VkeWindow win);
void        vke_poll_events();

#endif
