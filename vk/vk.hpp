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
#include <vk/tiny_obj_loader.h>

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
    struct impl {
        int v_major = 1;
        int v_minor = 0;
        uint32_t version = VK_MAKE_VERSION(1, 0, 0);
        VkApplicationInfo app_info;
        void init();
        ~impl();
        VkInstance inst();
        bool check_validation();
        std::vector<symbol> getRequiredExtensions();
        operator bool();
        type_register(impl);
    };
    mx_declare(Vulkan, mx, impl);

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

    struct impl {
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

        ~impl();
        operator bool() { return phys != VK_NULL_HANDLE; }
        type_register(impl);

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
    
    mx_object(Window, mx, impl);
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

struct Device:mx {
    struct impl {
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

        void drawFrame(array<Pipeline> &pipelines);
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
        ~impl();
        void loop(lambda<void(array<Pipeline>&)>);
        operator bool() { return device != VK_NULL_HANDLE; }
        type_register(impl);
    };

    static Device create(Window &gpu);

    operator VkDevice();
    mx_object(Device, mx, impl);
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
    struct impl {
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
        void create_image(array<ion::path> texture_paths, Asset type); // VK_FORMAT_R8G8B8A8_SRGB
        void create_image(vec2i sz);
        void update_image(ion::image &img);
        
        ~impl();
        operator bool() { return image != VK_NULL_HANDLE; }
        type_register(impl);
    };
    mx_object(Texture, mx, impl);

    static Texture load(Device &dev, symbol name, Asset type);
    void update(image img);
};

struct GraphicsData {
    Device device;
    symbol shader;
    symbol model;
    type_t u_type;
    type_t v_type;
    ///
    type_register(GraphicsData);
};

template <typename U, typename V>
struct Graphics:mx {
    Graphics(Device device, symbol model, symbol shader = "pbr") : Graphics() {
        data->device = device;
        data->shader = shader;
        data->model  = model;
        data->u_type = typeof(U);
        data->v_type = typeof(V);
    }
    mx_object(Graphics, mx, GraphicsData);
};

// Now, create a template that will derive from yes or no based on whether T has a member named "tangent"
template <typename T, typename V = void>
struct has_tangent : std::false_type {};

// Specialize a version of the template that will only be used if T has a member named "tangent"
template <typename T>
struct has_tangent<T, decltype((void) T::tangent, void())> : std::true_type {};

struct Pipeline:mx {
    struct impl {
        mx                          user; // user data
        Device                      device;
        memory*                     gmem; // graphics memory (grabbed)
        GraphicsData*               gfx;
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
        lambda<void()>              reload;
        lambda<void()>              uniform_update;
        bool                        init;
        watch                       watcher;

        std::vector<VkDescriptorSet>                   descriptorSets;
        VkVertexInputBindingDescription                binding_desc;
        std::vector<VkVertexInputAttributeDescription> attr_desc;

        Texture textures[Asset::count];

        void start();
        void cleanup(); /// impl calls cleanup, but cleanup is called prior to a reload
        void createUniformBuffers();

        ~impl();

        VkShaderModule createShaderModule(const array<char>& code);

        void createDescriptorSetLayout();
        void createDescriptorSets();
        void updateDescriptorSets();
        void createGraphicsPipeline();

        static void tangents(
            glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec2 uv0, glm::vec2 uv1, glm::vec2 uv2,
            glm::vec3 &tangent) {
            // Calculate the edge vectors of the triangle.
            glm::vec3 edge0 = v1 - v0;
            glm::vec3 edge1 = v2 - v0;

            // Calculate the UV delta vectors.
            glm::vec2 deltaUV0 = uv1 - uv0;
            glm::vec2 deltaUV1 = uv2 - uv0;

            // Calculate the tangent
            float r   = 1.0f / (deltaUV0.x * deltaUV1.y - deltaUV0.y * deltaUV1.x);
            tangent   = (edge0 * deltaUV1.y - edge1 * deltaUV0.y) * r;

            // Normalize the tangent
            tangent   = glm::normalize(tangent);
        }

        /// loadGltf requires you to map the vertex group to a V type.
        /// convert to use just one type fpr a specific node group

        static map<Pipeline> 

        static void loadGltf(Pipeline &pipeline, ion::path path, str part, map<type_t> node_groups) {
            using namespace gltf;
            Model m = Model::load(path);
            array<u32> indices;
            
            /// iterate through node groups; we have a list of nodes to load from this model
            for (field<type_t> f: node_groups) {
                str    name  = f.key.grab();
                type_t vtype = f.value;

                /// its not terrible in that you could have all actual levels in this file
                /// its just not lending itself interface-wise to a load specific model with parts in it
                /// we only want a bunch of parts to a model though; apply the transform too.. (scale & translation)
                for (Scene &s: m->scenes) {
                    /// nodes schmodes.  parts?.. i would say parts.
                    for (size_t inode: s->nodes) {
                        Node &node = m->nodes [inode];
                        /// load specific node name from node group
                        if (node->name != name) continue;
                        Mesh &mesh = m->meshes[node->mesh];
                        for (Primitive &prim: mesh->primitives) {

                            /// for each attrib we fill out the vstride
                            struct vstride {
                                ion::prop      *prop;
                                type_t          compound_type;
                                AssignFn<void>  assign;
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

                                /// all accessors are same length
                                /// same on our dst
                                /// the src stride is the size of struct_type[n_components]
                                
                                assert(vtype->meta_map);
                                prop*  p = (*vtype->meta_map)[prop_sym];
                                assert(p);

                                vstride &stride    = strides[pcount];
                                stride.prop        = p;
                                stride.compound_type = p->member_type; /// native glm-type or float
                                stride.assign      = p->member_type->functions->assign;
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
                            u8 *vbuf = (u8*)vtype->functions->valloc(null, null, vlen);
                            u8 *dst  = vbuf;

                            /// copy data into vbuf
                            for (vstride &stride: strides) {
                                /// offset into src buffer
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
                                }
                                /// next vertex
                                dst += vtype->base_sz;
                            }
                            /// create vertex buffer by wrapping what we've copied from allocation (we have a primitive array)
                            mx verts { memory::wrap(vtype, vbuf, pcount) }; /// load indices (always store 32bit uint)
                            pipeline.createVertexBuffer(verts);

                            /// indices data = indexing mesh-primitive->indices
                            Accessor &a_indices = m->accessors[prim->indices];
                            pipeline.indicesSize = a_indices->count;
                            ///
                            type_t    a_type = typeof(u32);
                            BufferView &view =        m->bufferViews[ a_indices->bufferView ];
                            Buffer      &buf =        m->buffers    [ view->buffer ];

                            /// root type would have to be component type for the singular u16, u32
                            u32  *u32_window = (u32*)&buf->uri.data [ view->byteOffset ];
                            assert(a_indices->componentType == ComponentType::UNSIGNED_INT);
                            memory *mem_indices = memory::window(typeof(u32), u32_window, a_indices->count);
                            pipeline.createIndexBuffer(mx(mem_indices));
                            break;
                        }
                        break;
                    }
                }
            }
        }

        template <typename V>
        void loadModel(cstr obj) {
            tinyobj::attrib_t                   attrib;
            std::vector<tinyobj::shape_t>       shapes;
            std::vector<tinyobj::material_t>    materials;
            std::string                         warn, err;

            if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, (symbol)obj)) {
                throw std::runtime_error(warn + err);
            }

            std::vector<V>        vertices(attrib.vertices.size());
            std::vector<bool>     emitted(attrib.vertices.size());
            std::vector<uint32_t> indices (indicesSize);
            size_t                cur = 0;
            auto                  uniqueVertices = std::unordered_map<V, uint32_t> { };

            for (const auto& shape : shapes) {
                for (const auto& index : shape.mesh.indices) {
                    V vertex {
                        attrib.vertices.data(),  index.vertex_index,
                        attrib.texcoords.data(), index.texcoord_index,
                        attrib.normals.data(),   index.normal_index };

                    if (uniqueVertices.count(vertex) == 0) {
                        uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                        vertices.push_back(vertex);
                    }
                    indices.push_back(uniqueVertices[vertex]);
                }
            }

            if constexpr (has_tangent<V>::value)
                for (size_t i = 0; i < indices.size(); i += 3) {
                    V &v0 = vertices[indices[i + 0]];
                    V &v1 = vertices[indices[i + 1]];
                    V &v2 = vertices[indices[i + 2]];

                    glm::vec3 tangent;
                    tangents(v0.pos, v1.pos, v2.pos,
                             v0.uv,  v1.uv,  v2.uv,
                            tangent);

                    v0.tangent = tangent;
                    v1.tangent = tangent;
                    v2.tangent = tangent;
                }

            indicesSize = indices.size();
            createVertexBuffer(vertices);
            createIndexBuffer(indices);
        }

        void createVertexBuffer(mx vertices) {
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
        
        template <typename V>
        void createVertexBuffer(std::vector<V> &vertices) {
            VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);
            void* vdata;
            vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &vdata);
                memcpy(vdata, vertices.data(), (size_t) bufferSize);
            vkUnmapMemory(device, stagingBufferMemory);
            device->createBuffer(bufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
            device->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        }

        void createIndexBuffer(mx indices);
        void createIndexBuffer(std::vector<uint32_t> &indices);

        VkVertexInputBindingDescription getBindingDescription();
        std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        operator bool() { return graphicsPipeline != VK_NULL_HANDLE; }
        type_register(impl);
    };

    mx_object(Pipeline, mx, impl);

    /// pipelines track themselves, and notify the user (the app that a reload is happening [likely needed for status/logging alone?])
    /// important to note that Device is not reloaded
    /// if you have 40 pipelines and 1 file changes associated to one of them, we are not rebuilding all 40, just 1.
    template <typename U, typename V>
    Pipeline(Graphics<U,V> graphics):Pipeline() { /// instead of a texture it needs flags for the resources to load
        data->gmem           = graphics.grab(); /// just so we can hold onto the data; we must drop this in our dtr
        data->gfx            = graphics.data;
        data->device         = data->gfx->device;
        
        data->uniform_update = [data=data]() {
            U ubo {};
            ubo.update(data);
            memcpy(data->uniformBuffersMapped[data->device->currentFrame], &ubo, sizeof(ubo));
        };

        data->reload       = [data=data]() { /// this will need a proper cleanup proc in itsself
            data->binding_desc = data->getBindingDescription();
            data->attr_desc    = data->getAttributeDescriptions();
            
            data->createUniformBuffers();
            data->createDescriptorSetLayout();
            data->createGraphicsPipeline();

            /// load texture assets for all enumerables (minus undefined)
            for (size_t i = 0; i < Asset::count; i++)
                data->textures[i] = Texture::load(
                    data->gfx->device, data->gfx->model, Asset(i));

            str model = data->gfx->model;
            path g = fmt {"models/{0}.gltf", { model }};
            if (g.exists()) {
                map<type_t> parts = {
                    { model, typeof(V) }
                };
                data->loadGltf(g, parts);
            } else {
                path p = fmt {"models/{0}.obj",  { str(data->gfx->model) }};
                console.test(p.exists(), "model resource not found");
                data->template loadModel<V>(p.cs());
            }

            data->createDescriptorSets();
        };

        /// start monitoring for changes, also recompiles shaders each time (if a shader does not compile on first go, it will error right away and thats useful)
        data->start(); /// the monitor needs the shader and the model so it can extrapolate what to listen for
    }
};

}