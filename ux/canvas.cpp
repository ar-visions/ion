#define CANVAS_IMPL

#include <GLFW/glfw3.h>

#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#ifdef WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include <GLFW/glfw3native.h>

/// X11 globals that conflict with Dawn
#undef None
#undef Success
#undef Always
#undef Bool

#include <vector>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#include "dawn/samples/SampleUtils.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/SystemUtils.h"
#include "dawn/utils/WGPUHelpers.h"

#include <dawn/webgpu_cpp.h>
#include "dawn/common/Assert.h"
#include "dawn/common/Log.h"
#include "dawn/common/Platform.h"
#include "dawn/common/SystemUtils.h"
#include "dawn/common/Constants.h"
#include "dawn/dawn_proc.h"
#include "webgpu/webgpu_glfw.h"
#include <dawn/native/DawnNative.h>

#include "include/gpu/graphite/dawn/DawnUtils.h"
#include "include/gpu/graphite/dawn/DawnTypes.h"

#include <gpu/graphite/BackendTexture.h>
#include <gpu/graphite/dawn/DawnBackendContext.h>
#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Surface.h>
#include <gpu/graphite/Surface_Graphite.h>
#include <gpu/GrContextOptions.h>
#include <tools/GpuToolUtils.h>
#include <tools/window/WindowContext.h>
#include <tools/graphite/ContextFactory.h>

#include "core/SkPath.h"
#include "core/SkFont.h"
#include "core/SkRRect.h"
#include "core/SkBitmap.h"
#include "core/SkCanvas.h"
#include "core/SkColorSpace.h"
#include "core/SkSurface.h"
#include "core/SkFontMgr.h"
#include "ports/SkFontMgr_empty.h"
#include "core/SkFontMetrics.h"
#include "core/SkPathMeasure.h"
#include "core/SkPathUtils.h"
#include "utils/SkParsePath.h"
#include "core/SkTextBlob.h"
#include "effects/SkGradientShader.h"
#include "effects/SkImageFilters.h"
#include "effects/SkDashPathEffect.h"
#include "core/SkStream.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "modules/svg/include/SkSVGNode.h"
#include "core/SkAlphaType.h"
#include "core/SkColor.h"
#include "core/SkColorType.h"
#include "core/SkImageInfo.h"
#include "core/SkRefCnt.h"
#include "core/SkTypes.h"

#include <mx/mx.hpp>
#include <media/image.hpp>
#include <ux/webgpu.hpp>
#include <ux/gltf.hpp>
#include <ux/canvas.hpp>

using namespace ion;

#define MAX_ATTRIBUTES 8

namespace ion {

/// better form of enum here, even if it is two/three part.
enums_define (TextureFormat2, WGPUTextureFormat)

struct IDevice {
    WGPUDevice device;
    WGPUQueue  queue;
    register(IDevice)
};

/// for now leave these in here; it should eventually go in a Dawn module
struct ITexture {
    WGPUDevice          device;
    WGPUTexture         texture;
    WGPUTextureView     view;
    WGPUSampler         sampler;
    vec2i               sz;
    Asset               asset_type;
    bool                updated;

    ITexture() { }

    void update_image(ion::image &img) {
        updated = true;
    }

    /// image can be created with nothing specified, in which case we produce an rgba8 based on Asset
    void set_content(mx content, Asset type = Asset::undefined) {
        /// otherwise create a blank image; based on asset type, it will be black or white
        ion::image img;
        if (content.type() == typeof(ion::image))
            img = content.hold();
        else if (content.type() == typeof(ion::Canvas)) {
            /// get screenshot of the Canvas
            assert(false);
        }
        if (!img) {
            img = ion::size { 2, 2 };
            ion::rgba8 *pixels = img.data;
            if (type == Asset::env) /// a light map contains white by default
                memset(pixels, 255, sizeof(ion::rgba8) * (img.width() * img.height()));
        }
        ion::rgba8 *pixels = img.data;
        vec2i       sz     = { int(img.width()), int(img.height()) };

        update_image(img);
        asset_type = type;
    }

    /// needs a format specifier
    void create_image(vec2i size) {
        sz = size;
    }

    operator bool() { return bool(texture); }
    type_register(ITexture);
};

static WGPUTextureFormat preferred_swapchain_format() {
    return WGPUTextureFormat_BGRA8Unorm;
}

static bool is_wayland() {
    const char* session_type = std::getenv("XDG_SESSION_TYPE");
    return session_type && strcmp(session_type, "wayland") == 0;
}

ion::image Texture::asset_image(symbol name, Asset type) {
    array<ion::path> paths = {
        fmt {"models/{0}.{1}.png", { name, type.symbol() }},
        fmt {"models/{0}.png",     { type.symbol() }}
    };
    for (ion::path &p: paths) {
        if (p.exists())
            return ion::image(p);
    }
    //assert(type != Asset::color); /// color is required asset; todo: normal should create a 255, 128, 0 constant upon failure
    return null;
}

Texture Texture::from_image(Device &dev, image img, Asset type) {
    Texture tx;
    tx->device = dev->device;
    tx->set_content(img, type);
    return tx;
}

Texture Texture::of_size(Device &dev, vec2i size, TextureFormat2 f) {
    WGPUDevice              device  = dev->device;
    Texture                 texture;
    WGPUTextureFormat       format  = f.convert();
    WGPUTextureDescriptor   tx_desc {
        .usage                      = WGPUTextureUsage_CopyDst        | 
                                      WGPUTextureUsage_TextureBinding | 
                                      WGPUTextureUsage_RenderAttachment,
        .dimension                  = WGPUTextureDimension_2D,
        .size                       = { (u32)size.x, (u32)size.y, 1 },
        .format                     = preferred_swapchain_format(),
        .mipLevelCount              = 1,
        .sampleCount                = 1
    };
    WGPUTextureViewDescriptor vdesc {
        .format                     = preferred_swapchain_format(),
        .dimension                  = WGPUTextureViewDimension_2D,
        .baseMipLevel               = 0,
        .mipLevelCount              = 1,
        .baseArrayLayer             = 0,
        .arrayLayerCount            = 1
    };

    texture->device                 = device;
    texture->sz                     = size;
    texture->texture                = wgpuDeviceCreateTexture(device, &tx_desc);
    texture->view                   = wgpuTextureCreateView(texture->texture, &vdesc);
    
    WGPUSamplerDescriptor sampler_desc {
        .addressModeU               = WGPUAddressMode_Repeat,
        .addressModeV               = WGPUAddressMode_Repeat,
        .addressModeW               = WGPUAddressMode_Repeat,
        .magFilter                  = WGPUFilterMode_Linear,
        .minFilter                  = WGPUFilterMode_Linear,
        .mipmapFilter               = WGPUMipmapFilterMode_Linear,
        .lodMinClamp                = 0.0f,
        .lodMaxClamp                = FLT_MAX,
        .compare                    = WGPUCompareFunction_Undefined
    };
    
    texture->sampler                = wgpuDeviceCreateSampler(device, &sampler_desc);
    return texture;
}

Texture Texture::of_size(Device &dev, vec2i size, TextureFormat2 f) {
    WGPUTextureFormat        format = f.convert();
    WGPUTextureDescriptor   tx_desc {
        .dimension                  = WGPUTextureDimension_2D,
        .size.width                 = (u32)size.x,
        .size.height                = (u32)size.y,
        .size.depthOrArrayLayers    = 1,
        .sampleCount                = 1,
        .format                     = preferred_swapchain_format(),
        .mipLevelCount              = 1,
        .usage                      = WGPUTextureUsage_CopyDst        | 
                                      WGPUTextureUsage_TextureBinding | 
                                      WGPUTextureUsage_RenderAttachment
    };

    WGPUTextureViewDescriptor vdesc {
        .format                     = preferred_swapchain_format(),
        .dimension                  = WGPUTextureViewDimension_2D,
        .baseMipLevel               = 0,
        .mipLevelCount              = 1,
        .baseArrayLayer             = 0,
        .arrayLayerCount            = 1
    };

    device                          = dev->device;
    sz                              = size;
    texture                         = WGPUCreateTexture(device, &tx_desc);
    view                            = WGPUCreateTextureView(texture, &vdesc);
    
    WGPUSamplerDescriptor     sdesc {
        .addressModeU               = WGPUAddressMode_Repeat,
        .addressModeV               = WGPUAddressMode_Repeat,
        .addressModeW               = WGPUAddressMode_Repeat,
        .magFilter                  = WGPUFilterMode_Linear,
        .minFilter                  = WGPUFilterMode_Linear,
        .mipmapFilter               = WGPUFilterMode_Linear,
        .lodMinClamp                = 0.0f,
        .lodMaxClamp                = FLT_MAX,
        .compare                    = WGPUCompareFunction_Undefined
    };
    
    sampler                         = wgpuDeviceCreateSampler(device, &sampler_desc);
}

/// make this not a static method; change the texture already in memory
Texture Texture::load(Device &dev, symbol name, Asset type) {
    return from_image(dev, asset_image(name, type), type);
}

void Texture::update(image img) {
    data->update_image(img);
}

struct IDawn {
    #if defined(WIN32)
        WGPUBackendType backend_type = WGPUBackendType_D3D12;
    #elif defined(__APPLE__)
        WGPUBackendType backend_type = WGPUBackendType_Metal;
    #elif defined(__linux__)
        WGPUBackendType backend_type = WGPUBackendType_Vulkan;
    #endif
    DawnProcTable   procs;
    WGPUInstance    instance;
    register(IDawn);
};

struct IPipeline {
    mx                          uniform_data;
    type_t                      vertex_type;
    mx                          user; // user data

    WGPURenderPipeline          pipeline;
    WGPUBindGroup               bind_group;
    WGPUBindGroupLayout         bgl;
    WGPUBuffer                  index_buffer;
    WGPUBuffer                  vertex_buffer;
    WGPUPipelineLayout          pl;

    str                         name; // Pipelines represent 'parts' of models and must have a name
    Device                      device;
    memory*                     gmem; // graphics memory (grabbed)
    Graphics                    gfx;
    size_t                      indices_count;
    lambda<void(IPipeline&)>    reload;
    lambda<void(memory*)>       uniform_update;
    bool                        init;
    watch                       watcher;
    bool                        updated;
    bool                        blending = true;
    Texture                     textures[Asset::count - 1];

    static WGPUBuffer create_index_buffer(WGPUDevice device, mx indexData) {
        WGPUBufferDescriptor descriptor = {};
        descriptor.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
        descriptor.size = indexData.count() * indexData.type()->base_sz;
        descriptor.mappedAtCreation = true;
        WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);
        void* mappedMemory = wgpuBufferGetMappedRange(buffer, 0, descriptor.size);
        memcpy(mappedMemory, indexData.mem->origin, descriptor.size);
        wgpuBufferUnmap(buffer);
        return buffer;
    }

    static WGPUBuffer create_vertex_buffer(WGPUDevice device, mx vertexData) {
        WGPUBufferDescriptor descriptor = {};
        descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
        descriptor.size = vertexData.count() * vertexData.type()->base_sz;
        descriptor.mappedAtCreation = true;
        WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);
        void* mappedMemory = wgpuBufferGetMappedRange(buffer, 0, descriptor.size);
        memcpy(mappedMemory, vertexData.mem->origin, descriptor.size);
        wgpuBufferUnmap(buffer);
        return buffer;
    }

    /// load shader, textures, create vbo/ibo
    void assemble_graphics(gltf::Model &m, Graphics &gfx) {
        using namespace gltf;

        str part = gfx->name;
        type_t vtype = gfx->vtype;
        WGPUDevice device = this->device->device;

        mx              mx_vbuffer;
        mx              mx_ibuffer;
        array<image>    images;
        
        /// load shaders (cache these modules)
        path shader_path = fmt { "shaders/{0}.wgl", { gfx->shader } };
        str  shader_code = shader_path.read<str>();
        //wgpu::ShaderModule mod = dawn::utils::CreateShaderModule(device, shader_code);

        WGPUShaderModuleDescriptor descriptor {};
        WGPUShaderModuleWGSLDescriptor wgslDescriptor {
            .chain = { .sType = WGPUSType_ShaderModuleWGSLDescriptor },
            .code  = (const char *)shader_code.cs()
        };

        descriptor.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&wgslDescriptor);
        WGPUShaderModule mod = wgpuDeviceCreateShaderModule(device, &descriptor);

        /// load vbo/ibo data:
        /// if gen is set, we load from this, otherwise its a part from gltf model
        if (gfx->gen) {
            gfx->gen(mx_vbuffer, mx_ibuffer, images);
        } else {
            /// must have been loaded
            assert(m->nodes);

            /// load from gltf (single object)
            for (Node &node: m->nodes) {
                /// load specific node name from node group
                if (node->name != part) continue;
                assert(node->mesh >= 0);

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
                        str       prop_bind      = f.key.hold();
                        symbol    prop_sym       = symbol(prop_bind);
                        num       accessor_index = num(f.value);
                        Accessor &accessor       = m->accessors[accessor_index];

                        /// the src stride is the size of struct_type[n_components]
                        assert(vtype->meta_map);
                        prop*  p = (*vtype->meta_map)[prop_sym];
                        assert(p);

                        vstride &stride    = strides[pcount++];
                        stride.prop        = p;
                        stride.compound_type = p->type; /// native glm-type or float
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
                    mx_vbuffer = verts;

                    /// indices data = indexing mesh-primitive->indices
                    Accessor &a_indices = m->accessors[prim->indices];
                    BufferView &view = m->bufferViews[ a_indices->bufferView ];
                    Buffer      &buf = m->buffers    [ view->buffer ];
                    memory *mem_indices;

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
                    mx_ibuffer = mx(mem_indices);
                    break;
                }
                break;
            }
        }

        index_buffer  = create_index_buffer(device, mx_ibuffer);
        vertex_buffer = create_vertex_buffer(device, mx_vbuffer);
        indices_count = mx_ibuffer.count();
        
        /// setup bindings
        array<WGPUBindGroupEntry>       bind_values (gfx->bindings.count()); // this creates with a reserve
        array<WGPUBindGroupLayoutEntry> bind_entries(gfx->bindings.count()); //
        size_t bind_id = 0;
        Texture tx;
        for (mx& binding: gfx->bindings) { /// array of mx can tell us what type of data it is
            WGPUBindGroupLayoutEntry entry {
                .binding    = u32(bind_id),
                .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment /// probably best to look through the shader code after @fragment and @vertex
            };
            WGPUBindGroupEntry bind_value {
                .binding    = u32(bind_id)
            };

            if (binding.type() == typeof(Texture)) {
                tx = binding.hold();
                entry.texture.multisampled  = false;
                entry.texture.sampleType    = WGPUTextureSampleType_Float;
                entry.texture.viewDimension = WGPUTextureViewDimension_2D;
                bind_value.textureView      = tx->view;
            } else if (binding.type() == typeof(Sampling::etype)) {
                /// this must be specified immediately after the texture
                assert(tx);
                Sampling sampling(binding.hold());
                entry.sampler.type          = sampling == Sampling::nearest ? WGPUSamplerBindingType_NonFiltering : WGPUSamplerBindingType_Filtering;
                bind_value.sampler          = tx->sampler;
            } else {
                /// other objects are Uniform buffers
                entry.buffer.type = WGPUBufferBindingType_Uniform;

                size_t sz = binding.type()->base_sz; /// was UniformBufferObject
                WGPUBufferDescriptor desc = {
                    .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                    .size = sz
                };
                bind_value.buffer = wgpuDeviceCreateBuffer(device, &desc);
            }

            bind_entries.push(entry);
            bind_values.push(bind_value);
            bind_id++;
        }

        WGPUBindGroupLayoutDescriptor ld {
            .entryCount         = size_t(bind_id),
            .entries            = bind_entries.data
        };
        bgl = wgpuDeviceCreateBindGroupLayout(device, &ld);

        WGPUBindGroupDescriptor bg {
            .layout             = bgl,
            .entryCount         = size_t(bind_id),
            .entries            = bind_values.data
        };
        bind_group = device.CreateBindGroup(&bg);
        pl = dawn::utils::MakeBasicPipelineLayout(device, &bgl);

        size_t        attrib_count = 0;
        doubly<prop> &props = *(doubly<prop>*)vtype->meta;
        auto get_wgpu_format = [](prop &p) {
            if (p.type == typeof(glm::vec2))  return WGPUVertexFormat_Float32x2;
            if (p.type == typeof(glm::vec3))  return WGPUVertexFormat_Float32x3;
            if (p.type == typeof(glm::vec4))  return WGPUVertexFormat_Float32x4;
            if (p.type == typeof(glm::ivec4)) return WGPUVertexFormat_Sint32x4;
            if (p.type == typeof(float))      return WGPUVertexFormat_Float32;
            ///
            assert(false);
            return WGPUVertexFormat_Undefined;
        };

        for (prop &p: props) {
            attributes[attrib_count].shaderLocation = attrib_count;
            attributes[attrib_count].format         = get_wgpu_format(p);
            attributes[attrib_count].offset         = p.offset;
            attrib_count++;
        }

        /// blending could be a series of flattened enums
        wgpu::BlendState blend = {
            .color = {
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::SrcAlpha,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha
            },
            .alpha = {
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha
            }
        };

        render_desc.cBuffers[0].attributeCount = attrib_count;
        render_desc.cFragment.module = mod;
        render_desc.cTargets[0].format = preferred_swapchain_format();
        render_desc.cTargets[0].blend = blending ? &blend : null;
        render_desc.EnableDepthStencil(wgpu::TextureFormat::Depth24PlusStencil8);

        pipeline = device.CreateRenderPipeline(&render_desc);
        Dawn dawn;
        dawn.process_events();
    }

    void submit(wgpu::TextureView color_view, wgpu::TextureView depth_stencil_view, states<Clear> clear_states, rgbaf clear_color) {
        wgpu::RenderPassColorAttachment color_attachment {
            .view = color_view,
            .loadOp = clear_states[Clear::Color] ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = wgpu::Color { clear_color.r, clear_color.g, clear_color.b, clear_color.a }
        };
        wgpu::RenderPassDepthStencilAttachment depth_attachment {
            .view = depth_stencil_view,
            .depthLoadOp = clear_states[Clear::Depth] ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
            .depthStoreOp = wgpu::StoreOp::Store,
            .depthClearValue = 1.0f,
            .depthReadOnly = false,
            .stencilLoadOp = clear_states[Clear::Stencil] ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
            .stencilStoreOp = wgpu::StoreOp::Store,
            .stencilReadOnly = false
        };
        wgpu::RenderPassDescriptor render_desc {
            .colorAttachmentCount = 1,
            .colorAttachments = &color_attachment,
            .depthStencilAttachment = depth_stencil_view ? &depth_attachment : null
        };

        wgpu::Queue queue = device->device.GetQueue();
        wgpu::CommandEncoder encoder = device->device.CreateCommandEncoder();
        wgpu::RenderPassEncoder render = encoder.BeginRenderPass(&render_desc);
        render.SetPipeline(pipeline);
        render.SetBindGroup(0, bind_group);
        render.SetVertexBuffer(0, vertex_buffer);
        render.SetIndexBuffer(index_buffer, wgpu::IndexFormat::Uint32);
        render.DrawIndexed(indices_count);
        render.End();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    void start() {
        if (!is_debug()) {
            reload(*this);
            init = true;
        } else {
            // while debugging we can adjust resources at runtime
            // compile .spv on load if its not there
            static lambda<void(bool, array<path_op> &)> rld;
            rld = [data=this](bool first, array<path_op> &ops) {
                console.log("compiling shaders");
                for (path_op &op: ops) {
                    str ext = op->path.ext4();
                    if (ext == ".wgsl") {
                        str s_path = op->path;
                        // we want to update an instance of this shader name op->path.stem() as identity
                    }
                }
                /// if we already loaded before, cleanup resources
                if (data->init) {
                    data->cleanup();
                }
                data->reload((IPipeline&)*data);
                data->init = true;
            };

            /// spawn watcher
            watcher = watch::spawn(
                {"./shaders", "./models"},
                {".wgl", ".png", ".gltf"},
                {},
                rld);
        }
    }

    void cleanup() {
        for (int i = 0; i < Asset::count; i++)
            textures[i] = Texture();
    }

    ~IPipeline() {
        cleanup();
        gmem->drop();
    }

    register(IPipeline);
};

struct IPipes {
    Device                      device;
    ion::gltf::Model            m;
    symbol                      model;
    array<Texture>              textures { Asset::count };
    array<Pipeline>             pipelines;
    lambda<void(IPipeline&)>    reload;
    lambda<void(memory*)>       uniform_update;
    
    void submit(wgpu::TextureView color_view, wgpu::TextureView depth_stencil_view, states<Clear> clear_states, rgbaf clear_color) {
        for (auto p: pipelines) {
            p->submit(color_view, depth_stencil_view, clear_states, clear_color);
        }
    }
    register(IPipes);
};

Pipeline::Pipeline(Device &device, Graphics graphics):Pipeline() { /// instead of a texture it needs flags for the resources to load
    data->gmem           = graphics.hold(); /// just so we can hold onto the data; we must drop this in our dtr
    data->gfx            = graphics.data;
    data->device         = device;
    data->name           = graphics->name;
}

Pipes::Pipes(Device &device, symbol model, array<Graphics> parts):Pipes() {
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
        if (!loaded && model) { /// add watch ability back to this with a cache
            for (size_t i = 0; i < Asset::count; i++) /// todo: check against usage map to see if the texture applies and should be loaded
                data->textures[i] = Texture::load(data->device, model, Asset(i));
            loaded = true;
        }
    };

    data->reload = [data=data, reload_textures=reload_textures](IPipeline &p) { /// this will need a proper cleanup proc in itsself
        reload_textures();
        p.updated = true;
        p.assemble_graphics(data->m, p.gfx);
    };

    for (Pipeline &p: data->pipelines) {
        p->reload = data->reload;
        p->start();
    }
    reload_textures();
}

Pipeline &Pipes::operator[](str s) {
    for (Pipeline &p: data->pipelines) {
        if (s == p->name)
            return p;
    }
    assert(false);
    static Pipeline def;
    return def;
}

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

#ifdef __APPLE__
extern "C" void AllowKeyRepeats(void);
#endif

struct IWindow {
    Dawn dawn;
    ion::str title;
    GLFWwindow* handle;
    wgpu::AdapterType           adapter_type = wgpu::AdapterType::Unknown;
    Window::OnResize            resize;
    Window::OnCursorEnter       cursor_enter;
    Window::OnCursorScroll      cursor_scroll;
    Window::OnCursorPos         cursor_pos;
    Window::OnCursorButton      cursor_button;
    Window::OnKeyChar           key_char;
    Window::OnKeyScanCode       key_scancode;
    Window::OnCanvasRender      fn_canvas_render;
    Window::OnSceneRender       fn_scene_render;

    WGPUDevice                  gpu;
    Device                      device;
    Texture                     texture;
    wgpu::Sampler               sampler; /// 'device' default sampler
    wgpu::Queue                 queue;
    wgpu::SwapChain             swapchain;
    wgpu::TextureView           depth_stencil_view;
    
    Pipes                       canvas_pipeline;
    Canvas                      canvas;
    Scene                       scene;

    struct Attribs {
        glm::vec4 pos;
        glm::vec2 uv;
        doubly<prop> meta() {
            return {
                { "pos", pos },
                { "uv",  uv  }
            };
        }
    };

    vec2i                       sz;
    void                       *user_data;

    /// no need to expose a 'device' but we can configure it through window
    void process_scene() {
        if (fn_scene_render)
            scene = fn_scene_render();
    }

    static void PrintDeviceError(WGPUErrorType errorType, const char* message, void*) {
        const char* errorTypeName = "";
        switch (errorType) {
            case WGPUErrorType_Validation:
                errorTypeName = "Validation";
                break;
            case WGPUErrorType_OutOfMemory:
                errorTypeName = "Out of memory";
                break;
            case WGPUErrorType_Unknown:
                errorTypeName = "Unknown";
                break;
            case WGPUErrorType_DeviceLost:
                errorTypeName = "Device lost";
                break;
            default:
                DAWN_UNREACHABLE();
                return;
        }
        dawn::ErrorLog() << errorTypeName << " error: " << message;
    }

    static void DeviceLostCallback(WGPUDeviceLostReason reason, const char* message, void*) {
        dawn::ErrorLog() << "Device lost: " << message;
    }

    static void PrintGLFWError(int code, const char* message) {
        dawn::ErrorLog() << "GLFW error: " << code << " - " << message;
    }

    static void DeviceLogCallback(WGPULoggingType type, const char* message, void*) {
        dawn::ErrorLog() << "Device log: " << message;
    }

    static IWindow *iwindow(GLFWwindow *window) {
        return (IWindow*)glfwGetWindowUserPointer(window);
    }

    static void on_resize(GLFWwindow* window, int width, int height) {
        IWindow *iwin = iwindow(window);
        iwin->sz.x = width;
        iwin->sz.y = height;
        iwin->update_swap();
        iwin->update_texture();
        if (iwin->resize) iwin->resize(width, height);
    }

    static void on_cursor_scroll(GLFWwindow* window, double x, double y) {
        IWindow *iwin = iwindow(window);
        if (iwin->cursor_scroll) iwin->cursor_scroll(x, y);
    }

    static void on_cursor_enter(GLFWwindow* window, int enter) {
        IWindow *iwin = iwindow(window);
        if (iwin->cursor_enter) iwin->cursor_enter(enter);
    }

    static void on_cursor_pos(GLFWwindow* window, double x, double y) {
        IWindow *iwin = iwindow(window);
        if (iwin->cursor_pos) iwin->cursor_pos(x, y);
    }

    static void on_cursor_button(GLFWwindow* window, int button, int action, int mods) {
        IWindow *iwin = iwindow(window);
        if (iwin->cursor_button) iwin->cursor_button(button, action, mods);
    }

    static void on_key_char(GLFWwindow* window, u32 code, int mods) {
        IWindow *iwin = iwindow(window);
        if (iwin->key_char) iwin->key_char(code, mods);
    }

    static void on_key_scancode(GLFWwindow *window, int key, int scancode, int action, int mods) {
        IWindow *iwin = iwindow(window);
        if (iwin->key_scancode) iwin->key_scancode(key, scancode, action, mods);
    }

    wgpu::TextureView create_depth_stencil_view() {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size.width = (u32)sz.x;
        descriptor.size.height = (u32)sz.y;
        descriptor.size.depthOrArrayLayers = 1;
        descriptor.sampleCount = 1;
        descriptor.format = WGPUTextureFormat_Depth24PlusStencil8;
        descriptor.mipLevelCount = 1;
        descriptor.usage = WGPUTextureUsage_RenderAttachment;

        WGPUTexture depthStencilTexture = wgpuDeviceCreateTexture(device, &descriptor);
        WGPUTextureViewDescriptor viewDescriptor {
            .format = descriptor.format,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1
        };

        WGPUTextureView depthStencilView = wgpuTextureCreateView(depthStencilTexture, &viewDescriptor);
        wgpuTextureRelease(depthStencilTexture);
        return depthStencilView;
    }*/

    void update_texture() {
        WGPUTextureDescriptor tx_desc {};
        tx_desc.dimension               = WGPUTextureDimension_2D;
        tx_desc.size.width              = (u32)sz.x;
        tx_desc.size.height             = (u32)sz.y;
        tx_desc.size.depthOrArrayLayers = 1;
        tx_desc.sampleCount             = 1;
        tx_desc.format                  = preferred_swapchain_format();
        tx_desc.mipLevelCount           = 1;
        tx_desc.usage                   = WGPUTextureUsage_CopyDst | 
                                          WGPUTextureUsage_TextureBinding | 
                                          WGPUTextureUsage_RenderAttachment;

        WGPUTextureViewDescriptor vdesc = {0};
        vdesc.format          = preferred_swapchain_format();
        vdesc.dimension       = WGPUTextureViewDimension_2D;
        vdesc.baseMipLevel    = 0;
        vdesc.mipLevelCount   = 1;
        vdesc.baseArrayLayer  = 0;
        vdesc.arrayLayerCount = 1;

        texture->device  = device->device;
        texture->texture = wgpuDeviceCreateTexture(texture->device, &tx_desc);
        texture->view    = wgpuTextureCreateView(texture->texture, &vdesc);
        texture->sz      = sz;

        WGPUSamplerDescriptor sampler_desc = {
            .addressModeU   = WGPUAddressMode_Repeat,
            .addressModeV   = WGPUAddressMode_Repeat,
            .addressModeW   = WGPUAddressMode_Repeat,
            .magFilter      = WGPUFilterMode_Linear,
            .minFilter      = WGPUFilterMode_Linear,
            .mipmapFilter   = WGPUMipmapFilterMode_Linear,
            .lodMinClamp    = 0.0f,
            .lodMaxClamp    = FLT_MAX,
            .compare        = WGPUCompareFunction_Undefined,
            .maxAnisotropy  = 1
        };
        texture->sampler = wgpuDeviceCreateSampler(texture->device, &sampler_desc);
        canvas    = Canvas(device, texture, true);
    }

    void update_swap() {
        WGPUDevice device = this->device->device;
        WGPUSurfaceDescriptor baseSurfaceDesc = {};
        #ifdef __APPLE__
            WGPUSurfaceDescriptorFromMetalLayer metalSurfaceDesc = {};
            metalSurfaceDesc.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
            metalSurfaceDesc.layer = glfwGetMetalLayer(handle);
            baseSurfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&metalSurfaceDesc);
        #elif defined(__linux__)
            WGPUSurfaceDescriptorFromXlibWindow xlibSurfaceDesc = {};
            xlibSurfaceDesc.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
            xlibSurfaceDesc.display = glfwGetX11Display();
            xlibSurfaceDesc.window = static_cast<uint64_t>(glfwGetX11Window(handle));
            baseSurfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&xlibSurfaceDesc);
        #elif defined(WIN32)
            WGPUSurfaceDescriptorFromWindowsHWND hwndSurfaceDesc = {};
            hwndSurfaceDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
            hwndSurfaceDesc.hwnd = glfwGetWin32Window(handle);
            baseSurfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&hwndSurfaceDesc);
        #endif

        WGPUSurface surface = wgpuInstanceCreateSurface(dawn->instance, &baseSurfaceDesc);
        WGPUSwapChainDescriptor swapChainDesc {
            .usage = WGPUTextureUsage_RenderAttachment,
            .format = static_cast<WGPUTextureFormat>(preferred_swapchain_format()),
            .width = (u32)sz.x,
            .height = (u32)sz.y,
            .presentMode = WGPUPresentMode_Mailbox
        };
        swapchain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
        assert(swapchain);
    }

    static void adapter_callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) {
        if (status == WGPURequestAdapterStatus_Success) {
            // Store the received adapter in the userdata pointer
            *(WGPUAdapter*)userdata = adapter;
        } else {
            fprintf(stderr, "Failed to get an adapter: %s\n", message);
        }
    }

    void create_resources() {
        if (!dawn->instance) {
            dawn->procs = dawn::native::GetProcs();
            dawnProcSetProcs(&dawn->procs);

            WGPUInstanceDescriptor inst_desc {
                .features = { .timedWaitAnyEnable = true }
            };
            dawn->instance = wgpuCreateInstance(&inst_desc);
        }

        array<const char*> enableToggleNames {
            "allow_unsafe_apis",  "use_user_defined_labels_in_backend" // allow_unsafe_apis = Needed for dual-source blending, BufferMapExtendedUsages.
        };
        array<const char*> disabledToggleNames { };

        WGPURequestAdapterOptions options {
            .powerPreference        = WGPUPowerPreference_HighPerformance,
            .backendType            = dawn->backend_type
        };
        WGPUAdapter adapter = NULL;
        wgpuInstanceRequestAdapter(dawn->instance, &options, adapter_callback, &adapter);

        WGPUDawnTogglesDescriptor togglesDesc {
            .chain                  = { .sType = WGPUSType_DawnTogglesDescriptor },
            .enabledToggleCount     = enableToggleNames.count(),
            .enabledToggles         = enableToggleNames.data,
            .disabledToggleCount    = disabledToggleNames.count(),
            .disabledToggles        = disabledToggleNames.data
        };
  
        array<WGPUFeatureName> requiredFeatures {WGPUFeatureName_SurfaceCapabilities};
        WGPUDeviceDescriptor deviceDescriptor {
            .nextInChain            = (WGPUChainedStruct*)&togglesDesc,
            .requiredFeatureCount   = requiredFeatures.count(),
            .requiredFeatures       = requiredFeatures.data
        };

        WGPUDevice device = wgpuAdapterCreateDevice(adapter, &deviceDescriptor);
        assert(device);
        this->device->device = device;
        this->device->queue  = wgpuDeviceGetQueue(device);
        wgpuDeviceSetUncapturedErrorCallback(device, PrintDeviceError, nullptr);
        wgpuDeviceSetDeviceLostCallback(device, DeviceLostCallback, nullptr);
        wgpuDeviceSetLoggingCallback(device, DeviceLogCallback, nullptr);

        update_swap();

        depth_stencil_view = create_depth_stencil_view();
        update_texture();

        canvas_pipeline = Pipes(this->device, null, array<Graphics> {
            Graphics {
                "canvas", typeof(Attribs), { texture, Sampling::linear }, "canvas", 
                [](mx &vbo, mx &ibo, array<image> &images) {
                    static const uint32_t indexData[6] = {
                        0, 1, 2,
                        2, 1, 3
                    };
                    static const Attribs vertexData[4] = {
                        {{ -1.0f, -1.0f, 0.0f, 1.0f }, {  0.0f, 0.0f }},
                        {{  1.0f, -1.0f, 0.0f, 1.0f }, {  1.0f, 0.0f }},
                        {{ -1.0f,  1.0f, 0.0f, 1.0f }, { -1.0f, 1.0f }},
                        {{  1.0f,  1.0f, 0.0f, 1.0f }, {  1.0f, 1.0f }}
                    };

                    ibo = mx::wrap<u32>((void*)indexData, 6); // must be a COPY.
                    vbo = mx::wrap<Attribs>((void*)vertexData, 4);
                    // set vbo and ibo
                }
            }
        });
    }

    bool process() {
        if (glfwWindowShouldClose(handle))
            return false;
        glfwPollEvents();
        dawn.process_events();

        wgpu::TextureView swap_view = swapchain.GetCurrentTextureView();
        states<Clear> clear_states { Clear::Color, Clear::Depth };
        rgbaf clear_color { 0.0f, 0.0f, 0.5f, 1.0f };

        /// render webgpu
        if (fn_scene_render) {
            Scene scene = fn_scene_render();
            for (Pipes &p: scene) {
                for (Pipeline &pipeline: p->pipelines) {
                    pipeline->submit(swap_view, depth_stencil_view, clear_states, clear_color);
                    clear_states[Clear::Color] = false;
                    clear_states[Clear::Depth] = false;
                    clear_states[Clear::Stencil] = false;
                }
            }
        }
        
        /// render skia texture to window
        if (fn_canvas_render) {
            fn_canvas_render(canvas);
            canvas.flush();
            canvas_pipeline->submit(swap_view, depth_stencil_view, clear_states, clear_color);
        }

        wgpuSwapChainPresent(swapchain);
        wgpuTextureViewRelease(swap_view);
        return true;
    }

    void create(str title, vec2i sz) {
        static bool init;
        if (!init) {
            #ifdef __APPLE__
            AllowKeyRepeats();
            #endif
            assert(glfwInit());
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
            glfwSetErrorCallback(PrintGLFWError);
            init = true;
        }
        this->sz = sz;
        this->title = title;
        this->handle = glfwCreateWindow(sz.x, sz.y, title.cs(), null, null);
        
        glfwSetWindowUserPointer        (handle, this);
        glfwSetKeyCallback              (handle, IWindow::on_key_scancode);
        glfwSetCharModsCallback         (handle, IWindow::on_key_char);
        glfwSetMouseButtonCallback      (handle, IWindow::on_cursor_button);
        glfwSetFramebufferSizeCallback  (handle, IWindow::on_resize);
        glfwSetCursorPosCallback        (handle, IWindow::on_cursor_pos);
        glfwSetScrollCallback           (handle, IWindow::on_cursor_scroll);
        glfwSetCursorEnterCallback      (handle, IWindow::on_cursor_enter);
        count++;
        create_resources();
    }

    static inline int count = 0;

    ~IWindow() {
        close();
    }

    void close() {
        //device.release();
        glfwDestroyWindow(handle);
        if (--count == 0)
            glfwTerminate();
    }

    register(IWindow);
};

Canvas Window::get_canvas() {
    return data->canvas;
}

void Window::set_on_canvas_render(OnCanvasRender fn_canvas_render) {
    data->fn_canvas_render = fn_canvas_render;
}

void Window::set_on_scene_render(OnSceneRender fn_scene_render) {
    data->fn_scene_render = fn_scene_render;
}

void Window::set_on_resize(OnResize resize) {
    data->resize = resize;
}

void Window::set_on_cursor_enter(OnCursorEnter cursor_enter) {
    data->cursor_enter = cursor_enter;
}

void Window::set_on_cursor_pos(OnCursorPos cursor_pos) {
    data->cursor_pos = cursor_pos;
}

void Window::set_on_cursor_scroll(OnCursorScroll cursor_scroll) {
    data->cursor_scroll = cursor_scroll;
}

void Window::set_on_cursor_button(OnCursorButton cursor_button) {
    data->cursor_button = cursor_button;
}

void Window::set_on_key_char(OnKeyChar key_char) {
    data->key_char = key_char;
}

void Window::set_on_key_scancode(OnKeyScanCode key_scancode) {
    data->key_scancode = key_scancode;
}

void *Window::handle() {
    return data->handle;
}

Window Window::create(str title, vec2i sz) {
    Window w;
    w->create(title, sz);
    return w;
}

Window::operator bool() {
    return bool(data->device);
}

Device Window::device() {
    return data->device;
}

void Window::set_visibility(bool v) {
    if (v)
        glfwShowWindow(data->handle);
    else
        glfwHideWindow(data->handle);
}

bool Window::process() {
    return data->process();
}

void Window::close() {
    data->close();
}

str Window::title() {
    return data->title;
}

void Window::set_title(str title) {
    glfwSetWindowTitle(data->handle, title.cs());
    data->title = title;
}

void Window::run() {
    while (!glfwWindowShouldClose(data->handle)) {
        data->process();
    }
}

void *Window::user_data() {
    return data->user_data;
}

void Window::set_user_data(void *user_data) {
    data->user_data = user_data;
}

ion::vec2i Window::size() {
    return data->sz;
}

void Dawn::process_events() {
    dawn::native::InstanceProcessEvents(data->instance);
}

float Dawn::get_dpi() {
    glfwInit();
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    float xscale, yscale;
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    return xscale;
}


/// canvas





static SkPoint rotate90(const SkPoint& p) { return {p.fY, -p.fX}; }
static SkPoint rotate180(const SkPoint& p) { return p * -1; }
static SkPoint setLength(SkPoint p, float len) {
    if (!p.setLength(len)) {
        SkDebugf("Failed to set point length\n");
    }
    return p;
}
static bool isClockwise(const SkPoint& a, const SkPoint& b) { return a.cross(b) > 0; }

/** Helper class for constructing paths, with undo support */
class PathRecorder {
public:
    SkPath getPath() const {
        return SkPath::Make(fPoints.data(), fPoints.size(), fVerbs.data(), fVerbs.size(), nullptr,
                            0, SkPathFillType::kWinding);
    }

    void moveTo(SkPoint p) {
        fVerbs.push_back(SkPath::kMove_Verb);
        fPoints.push_back(p);
    }

    void lineTo(SkPoint p) {
        fVerbs.push_back(SkPath::kLine_Verb);
        fPoints.push_back(p);
    }

    void close() { fVerbs.push_back(SkPath::kClose_Verb); }

    void rewind() {
        fVerbs.clear();
        fPoints.clear();
    }

    int countPoints() const { return fPoints.size(); }

    int countVerbs() const { return fVerbs.size(); }

    bool getLastPt(SkPoint* lastPt) const {
        if (fPoints.empty()) {
            return false;
        }
        *lastPt = fPoints.back();
        return true;
    }

    void setLastPt(SkPoint lastPt) {
        if (fPoints.empty()) {
            moveTo(lastPt);
        } else {
            fPoints.back().set(lastPt.fX, lastPt.fY);
        }
    }

    const std::vector<uint8_t>& verbs() const { return fVerbs; }

    const std::vector<SkPoint>& points() const { return fPoints; }

private:
    std::vector<uint8_t> fVerbs;
    std::vector<SkPoint> fPoints;
};

/// this was more built into skia before, now it is in tools
class SkPathStroker2 {
public:
    // Returns the fill path
    SkPath getFillPath(const SkPath& path, const SkPaint& paint);

private:
    struct PathSegment {
        SkPath::Verb fVerb;
        SkPoint fPoints[4];
    };

    float fRadius;
    SkPaint::Cap fCap;
    SkPaint::Join fJoin;
    PathRecorder fInner, fOuter;

    // Initialize stroker state
    void initForPath(const SkPath& path, const SkPaint& paint);

    // Strokes a line segment
    void strokeLine(const PathSegment& line, bool needsMove);

    // Adds an endcap to fOuter
    enum class CapLocation { Start, End };
    void endcap(CapLocation loc);

    // Adds a join between the two segments
    void join(const PathSegment& prev, const PathSegment& curr);

    // Appends path in reverse to result
    static void appendPathReversed(const PathRecorder& path, PathRecorder* result);

    // Returns the segment unit normal
    static SkPoint unitNormal(const PathSegment& seg, float t);

    // Returns squared magnitude of line segments.
    static float squaredLineLength(const PathSegment& lineSeg);
};

void SkPathStroker2::initForPath(const SkPath& path, const SkPaint& paint) {
    fRadius = paint.getStrokeWidth() / 2;
    fCap = paint.getStrokeCap();
    fJoin = paint.getStrokeJoin();
    fInner.rewind();
    fOuter.rewind();
}

SkPath SkPathStroker2::getFillPath(const SkPath& path, const SkPaint& paint) {
    initForPath(path, paint);

    // Trace the inner and outer paths simultaneously. Inner will therefore be
    // recorded in reverse from how we trace the outline.
    SkPath::Iter it(path, false);
    PathSegment segment, prevSegment;
    bool firstSegment = true;
    while ((segment.fVerb = it.next(segment.fPoints)) != SkPath::kDone_Verb) {
        // Join to the previous segment
        if (!firstSegment) {
            join(prevSegment, segment);
        }

        // Stroke the current segment
        switch (segment.fVerb) {
            case SkPath::kLine_Verb:
                strokeLine(segment, firstSegment);
                break;
            case SkPath::kMove_Verb:
                // Don't care about multiple contours currently
                continue;
            default:
                SkDebugf("Unhandled path verb %d\n", segment.fVerb);
                break;
        }

        std::swap(segment, prevSegment);
        firstSegment = false;
    }

    // Open contour => endcap at the end
    const bool isClosed = path.isLastContourClosed();
    if (isClosed) {
        SkDebugf("Unhandled closed contour\n");
    } else {
        endcap(CapLocation::End);
    }

    // Walk inner path in reverse, appending to result
    appendPathReversed(fInner, &fOuter);
    endcap(CapLocation::Start);

    return fOuter.getPath();
}

void SkPathStroker2::strokeLine(const PathSegment& line, bool needsMove) {
    const SkPoint tangent = line.fPoints[1] - line.fPoints[0];
    const SkPoint normal = rotate90(tangent);
    const SkPoint offset = setLength(normal, fRadius);
    if (needsMove) {
        fOuter.moveTo(line.fPoints[0] + offset);
        fInner.moveTo(line.fPoints[0] - offset);
    }
    fOuter.lineTo(line.fPoints[1] + offset);
    fInner.lineTo(line.fPoints[1] - offset);
}

void SkPathStroker2::endcap(CapLocation loc) {
    const auto buttCap = [this](CapLocation loc) {
        if (loc == CapLocation::Start) {
            // Back at the start of the path: just close the stroked outline
            fOuter.close();
        } else {
            // Inner last pt == first pt when appending in reverse
            SkPoint innerLastPt;
            fInner.getLastPt(&innerLastPt);
            fOuter.lineTo(innerLastPt);
        }
    };

    switch (fCap) {
        case SkPaint::kButt_Cap:
            buttCap(loc);
            break;
        default:
            SkDebugf("Unhandled endcap %d\n", fCap);
            buttCap(loc);
            break;
    }
}

void SkPathStroker2::join(const PathSegment& prev, const PathSegment& curr) {
    const auto miterJoin = [this](const PathSegment& prev, const PathSegment& curr) {
        // Common path endpoint of the two segments is the midpoint of the miter line.
        const SkPoint miterMidpt = curr.fPoints[0];

        SkPoint before = unitNormal(prev, 1);
        SkPoint after = unitNormal(curr, 0);

        // Check who's inside and who's outside.
        PathRecorder *outer = &fOuter, *inner = &fInner;
        if (!isClockwise(before, after)) {
            std::swap(inner, outer);
            before = rotate180(before);
            after = rotate180(after);
        }

        const float cosTheta = before.dot(after);
        if (SkScalarNearlyZero(1 - cosTheta)) {
            // Nearly identical normals: don't bother.
            return;
        }

        // Before and after have the same origin and magnitude, so before+after is the diagonal of
        // their rhombus. Origin of this vector is the midpoint of the miter line.
        SkPoint miterVec = before + after;

        // Note the relationship (draw a right triangle with the miter line as its hypoteneuse):
        //     sin(theta/2) = strokeWidth / miterLength
        // so miterLength = strokeWidth / sin(theta/2)
        // where miterLength is the length of the miter from outer point to inner corner.
        // miterVec's origin is the midpoint of the miter line, so we use strokeWidth/2.
        // Sqrt is just an application of half-angle identities.
        const float sinHalfTheta = sqrtf(0.5 * (1 + cosTheta));
        const float halfMiterLength = fRadius / sinHalfTheta;
        miterVec.setLength(halfMiterLength);  // TODO: miter length limit

        // Outer: connect to the miter point, and then to t=0 (on outside stroke) of next segment.
        const SkPoint dest = setLength(after, fRadius);
        outer->lineTo(miterMidpt + miterVec);
        outer->lineTo(miterMidpt + dest);

        // Inner miter is more involved. We're already at t=1 (on inside stroke) of 'prev'.
        // Check 2 cases to see we can directly connect to the inner miter point
        // (midpoint - miterVec), or if we need to add extra "loop" geometry.
        const SkPoint prevUnitTangent = rotate90(before);
        const float radiusSquared = fRadius * fRadius;
        // 'alpha' is angle between prev tangent and the curr inwards normal
        const float cosAlpha = prevUnitTangent.dot(-after);
        // Solve triangle for len^2:  radius^2 = len^2 + (radius * sin(alpha))^2
        // This is the point at which the inside "corner" of curr at t=0 will lie on a
        // line connecting the inner and outer corners of prev at t=0. If len is below
        // this threshold, the inside corner of curr will "poke through" the start of prev,
        // and we'll need the inner loop geometry.
        const float threshold1 = radiusSquared * cosAlpha * cosAlpha;
        // Solve triangle for len^2:  halfMiterLen^2 = radius^2 + len^2
        // This is the point at which the inner miter point will lie on the inner stroke
        // boundary of the curr segment. If len is below this threshold, the miter point
        // moves 'inside' of the stroked outline, and we'll need the inner loop geometry.
        const float threshold2 = halfMiterLength * halfMiterLength - radiusSquared;
        // If a segment length is smaller than the larger of the two thresholds,
        // we'll have to add the inner loop geometry.
        const float maxLenSqd = std::max(threshold1, threshold2);
        const bool needsInnerLoop =
                squaredLineLength(prev) < maxLenSqd || squaredLineLength(curr) < maxLenSqd;
        if (needsInnerLoop) {
            // Connect to the miter midpoint (common path endpoint of the two segments),
            // and then to t=0 (on inside) of the next segment. This adds an interior "loop" of
            // geometry that handles edge cases where segment lengths are shorter than the
            // stroke width.
            inner->lineTo(miterMidpt);
            inner->lineTo(miterMidpt - dest);
        } else {
            // Directly connect to inner miter point.
            inner->setLastPt(miterMidpt - miterVec);
        }
    };

    switch (fJoin) {
        case SkPaint::kMiter_Join:
            miterJoin(prev, curr);
            break;
        default:
            SkDebugf("Unhandled join %d\n", fJoin);
            miterJoin(prev, curr);
            break;
    }
}

void SkPathStroker2::appendPathReversed(const PathRecorder& path, PathRecorder* result) {
    const int numVerbs = path.countVerbs();
    const int numPoints = path.countPoints();
    const std::vector<uint8_t>& verbs = path.verbs();
    const std::vector<SkPoint>& points = path.points();

    for (int i = numVerbs - 1, j = numPoints; i >= 0; i--) {
        auto verb = static_cast<SkPath::Verb>(verbs[i]);
        switch (verb) {
            case SkPath::kLine_Verb: {
                j -= 1;
                SkASSERT(j >= 1);
                result->lineTo(points[j - 1]);
                break;
            }
            case SkPath::kMove_Verb:
                // Ignore
                break;
            default:
                SkASSERT(false);
                break;
        }
    }
}

SkPoint SkPathStroker2::unitNormal(const PathSegment& seg, float t) {
    if (seg.fVerb != SkPath::kLine_Verb) {
        SkDebugf("Unhandled verb for unit normal %d\n", seg.fVerb);
    }

    (void)t;  // Not needed for lines
    const SkPoint tangent = seg.fPoints[1] - seg.fPoints[0];
    const SkPoint normal = rotate90(tangent);
    return setLength(normal, 1);
}

float SkPathStroker2::squaredLineLength(const PathSegment& lineSeg) {
    SkASSERT(lineSeg.fVerb == SkPath::kLine_Verb);
    const SkPoint diff = lineSeg.fPoints[1] - lineSeg.fPoints[0];
    return diff.dot(diff);
}

inline SkColor sk_color(rgbad c) {
    rgba8 i = {
        u8(math::round(c.r * 255)), u8(math::round(c.g * 255)),
        u8(math::round(c.b * 255)), u8(math::round(c.a * 255)) };
    auto sk = SkColor(uint32_t(i.b)        | (uint32_t(i.g) << 8) |
                     (uint32_t(i.r) << 16) | (uint32_t(i.a) << 24));
    return sk;
}

inline SkColor sk_color(rgba8 c) {
    auto sk = SkColor(uint32_t(c.b)        | (uint32_t(c.g) << 8) |
                     (uint32_t(c.r) << 16) | (uint32_t(c.a) << 24));
    return sk;
}

SVG::SVG(path p) : SVG() {
    SkStream* stream = new SkFILEStream((symbol)p.cs());
    data->svg_dom = new sk_sp<SkSVGDOM>(SkSVGDOM::MakeFromStream(*stream));
    SkSize size = (*data->svg_dom)->containerSize();
    data->w = size.fWidth;
    data->h = size.fHeight;
    delete stream;
}

SVG::SVG(cstr p) : SVG(path(p)) { }

struct ICanvas {
    std::unique_ptr<skgpu::graphite::Context>  fGraphiteContext;
    std::unique_ptr<skgpu::graphite::Recorder> fGraphiteRecorder;
    //DisplayParams     params;

    bool use_hidpi;

    Texture texture;
    sk_sp<SkSurface> sk_surf = null;
    SkCanvas      *sk_canvas = null;
    vec2i                 sz = { 0, 0 };
    vec2i             sz_raw;
    vec2d          dpi_scale = { 1, 1 };


    struct state {
        ion::image  img;
        double      outline_sz = 0.0;
        double      font_scale = 1.0;
        double      opacity    = 1.0;
        m44d        m;
        rgbad       color;
        graphics::shape clip;
        vec2d       blur;
        ion::font   font;
        SkPaint     ps;
        glm::mat4   model, view, proj;
    };

    state *top = null;
    doubly<state> stack;

    void outline_sz(double sz) {
        top->outline_sz = sz;
    }

    void color(rgbad &c) {
        top->color = c;
    }

    void opacity(double o) {
        top->opacity = o;
    }

    void canvas_new_texture(int width, int height) {
        dpi_scale = 1.0f;
        identity();
    }

    SkPath *sk_path(graphics::shape &sh) {
        graphics::shape::sdata &shape = sh.ref<graphics::shape::sdata>();
        // shape.sk_path 
        if (!shape.sk_path) {
            shape.sk_path = new SkPath { };
            SkPath &p     = *(SkPath*)shape.sk_path;

            /// efficient serialization of types as Skia does not spend the time to check for these primitives
            if (shape.type == typeof(rectd)) {
                rectd &m = sh->bounds;
                SkRect r = SkRect {
                    float(m.x), float(m.y), float(m.x + m.w), float(m.y + m.h)
                };
                p.Rect(r);
            } else if (shape.type == typeof(Rounded<double>)) {
                Rounded<double>::rdata &m = sh->bounds.mem->ref<Rounded<double>::rdata>();
                p.moveTo  (m.tl_x.x, m.tl_x.y);
                p.lineTo  (m.tr_x.x, m.tr_x.y);
                p.cubicTo (m.c0.x,   m.c0.y, m.c1.x, m.c1.y, m.tr_y.x, m.tr_y.y);
                p.lineTo  (m.br_y.x, m.br_y.y);
                p.cubicTo (m.c0b.x,  m.c0b.y, m.c1b.x, m.c1b.y, m.br_x.x, m.br_x.y);
                p.lineTo  (m.bl_x.x, m.bl_x.y);
                p.cubicTo (m.c0c.x,  m.c0c.y, m.c1c.x, m.c1c.y, m.bl_y.x, m.bl_y.y);
                p.lineTo  (m.tl_y.x, m.tl_y.y);
                p.cubicTo (m.c0d.x,  m.c0d.y, m.c1d.x, m.c1d.y, m.tl_x.x, m.tl_x.y);
            } else {
                graphics::shape::sdata &m = *sh.data;
                for (mx &o:m.ops) {
                    type_t t = o.type();
                    if (t == typeof(Movement)) {
                        Movement m(o);
                        p.moveTo(m->x, m->y);
                    } else if (t == typeof(Line)) {
                        Line l(o);
                        p.lineTo(l->origin.x, l->origin.y); /// todo: origin and to are swapped, i believe
                    }
                }
                p.close();
            }
        }

        /// issue here is reading the data, which may not be 'sdata', but Rect, Rounded
        /// so the case below is 
        if (bool(shape.sk_offset) && shape.cache_offset == shape.offset)
            return (SkPath*)shape.sk_offset;
        ///
        if (!std::isnan(shape.offset) && shape.offset != 0) {
            assert(shape.sk_path); /// must have an actual series of shape operations in skia
            ///
            delete (SkPath*)shape.sk_offset;

            SkPath *o = (SkPath*)shape.sk_offset;
            shape.cache_offset = shape.offset;
            ///
            SkPath  fpath;
            SkPaint cp = SkPaint(top->ps);
            cp.setStyle(SkPaint::kStroke_Style);
            cp.setStrokeWidth(std::abs(shape.offset) * 2);
            cp.setStrokeJoin(SkPaint::kRound_Join);

            SkPathStroker2 stroker;
            SkPath offset_path = stroker.getFillPath(*(SkPath*)shape.sk_path, cp);
            shape.sk_offset = new SkPath(offset_path);
            
            auto vrbs = ((SkPath*)shape.sk_path)->countVerbs();
            auto pnts = ((SkPath*)shape.sk_path)->countPoints();
            std::cout << "sk_path = " << (void *)shape.sk_path << ", pointer = " << (void *)this << " verbs = " << vrbs << ", points = " << pnts << "\n";
            ///
            if (shape.offset < 0) {
                o->reverseAddPath(fpath);
                o->setFillType(SkPathFillType::kWinding);
            } else
                o->addPath(fpath);
            ///
            return o;
        }
        return (SkPath*)shape.sk_path;
    }

    void font(ion::font &f) { 
        top->font = f;
    }
    
    void save() {
        state &s = stack->push();
        if (top) {
            s = *top;
        } else {
            s.ps = SkPaint { };
        }
        sk_canvas->save();
        top = &s;
    }

    void identity() {
        sk_canvas->resetMatrix();
        sk_canvas->scale(dpi_scale.x, dpi_scale.y);
    }

    void set_matrix() {
    }

    m44d get_matrix() {
        SkM44 skm = sk_canvas->getLocalToDevice();
        m44d res(0.0);
        return res;
    }


    void    clear()        { sk_canvas->clear(SK_ColorTRANSPARENT); }
    void    clear(rgbad c) { sk_canvas->clear(sk_color(c)); }

    void    flush() {
        /// this is what i wasnt doing:
        std::unique_ptr<skgpu::graphite::Recording> recording = fGraphiteRecorder->snap();
        if (recording) {
            skgpu::graphite::InsertRecordingInfo info;
            info.fRecording = recording.get();
            fGraphiteContext->insertRecording(info);
            fGraphiteContext->submit(skgpu::graphite::SyncToCpu::kNo);
        }
    }

    void  restore() {
        stack->pop();
        top = stack->len() ? &stack->last() : null;
        sk_canvas->restore();
    }

    vec2i size() { return sz; }

    /// console would just think of everything in char units. like it is.
    /// measuring text would just be its length, line height 1.
    text_metrics measure(str &text) {
        SkFontMetrics mx;
        SkFont     &font = font_handle(top->font);
        auto         adv = font.measureText(text.cs(), text.len(), SkTextEncoding::kUTF8);
        auto          lh = font.getMetrics(&mx);

        return text_metrics {
            adv,
            abs(mx.fAscent) + abs(mx.fDescent),
            mx.fAscent,
            mx.fDescent,
            lh,
            mx.fCapHeight
        };
    }

    double measure_advance(char *text, size_t len) {
        SkFont     &font = font_handle(top->font);
        auto         adv = font.measureText(text, len, SkTextEncoding::kUTF8);
        return (double)adv;
    }

    /// the text out has a rect, controls line height, scrolling offset and all of that nonsense we need to handle
    /// as a generic its good to have the rect and alignment enums given.  there simply isnt a user that doesnt benefit
    /// it effectively knocks out several redundancies to allow some components to be consolidated with style difference alone
    str ellipsis(str &text, rectd &rect, text_metrics &tm) {
        const str el = "...";
        str       cur, *p = &text;
        int       trim = p->len();
        tm             = measure((str &)el);
        
        if (tm.w >= rect.w)
            trim = 0;
        else
            for (;;) {
                tm = measure(*p);
                if (tm.w <= rect.w || trim == 0)
                    break;
                if (tm.w > rect.w && trim >= 1) {
                    cur = text.mid(0, --trim) + el;
                    p   = &cur;
                }
            }
        return (trim == 0) ? "" : (p == &text) ? text : cur;
    }


    void image(ion::SVG &image, rectd &rect, alignment &align, vec2d &offset) {
        SkPaint ps = SkPaint(top->ps);
        vec2d  pos = { 0, 0 };
        vec2i  isz = image.sz();
        
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        /// now its just of matter of scaling the little guy to fit in the box.
        real scx = rect.w / isz.x;
        real scy = rect.h / isz.y;
        real sc  = (scy > scx) ? scx : scy;
        
        /// no enums were harmed during the making of this function (again)
        pos.x = mix(rect.x, rect.x + rect.w - isz.x * sc, align.x);
        pos.y = mix(rect.y, rect.y + rect.h - isz.y * sc, align.y);
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        sk_canvas->scale(sc, sc);
        image.render(sk_canvas, rect.w, rect.h);
        sk_canvas->restore();
    }

    void image(ion::image &image, rectd &rect, alignment &align, vec2d &offset, bool attach_tx) {
        SkPaint ps = SkPaint(top->ps);
        vec2d  pos = { 0, 0 };
        vec2i  isz = image.sz();
        
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        /// cache SkImage using memory attachments
        attachment *att = image.mem->find_attachment("sk-image");
        sk_sp<SkImage> *im;
        if (!att) {
            SkBitmap bm;
            rgba8          *px = image.pixels();
            //memset(px, 255, 640 * 360 * 4);
            SkImageInfo   info = SkImageInfo::Make(isz.x, isz.y, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
            sz_t        stride = image.stride() * sizeof(rgba8);
            bm.installPixels(info, px, stride);
            sk_sp<SkImage> bm_image = bm.asImage();
            im = new sk_sp<SkImage>(bm_image);
            if (attach_tx)
                att = image.mem->attach("sk-image", im, [im]() { delete im; });
        }
        
        /// now its just of matter of scaling the little guy to fit in the box.
        real scx = rect.w / isz.x;
        real scy = rect.h / isz.y;
        
        if (!align.is_default) {
            scx   = scy = (scy > scx) ? scx : scy;
            /// no enums were harmed during the making of this function
            pos.x = mix(rect.x, rect.x + rect.w - isz.x * scx, align.x);
            pos.y = mix(rect.y, rect.y + rect.h - isz.y * scy, align.y);
            
        } else {
            /// if alignment is default state, scale directly by bounds w/h, position at bounds x/y
            pos.x = rect.x;
            pos.y = rect.y;
        }
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        SkCubicResampler cubicResampler { 1.0f/3, 1.0f/3 };
        SkSamplingOptions samplingOptions(cubicResampler);

        sk_canvas->scale(scx, scy);
        SkImage *img = im->get();
        sk_canvas->drawImage(img, SkScalar(0), SkScalar(0), samplingOptions);
        sk_canvas->restore();

        if (!attach_tx) {
            flush();
        }
    }

    /// the lines are most definitely just text() calls, it should be up to the user to perform multiline.
    void text(str &text, rectd &rect, alignment &align, vec2d &offset, bool ellip, rectd *placement) {
        SkPaint ps = SkPaint(top->ps);
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        SkFont  &f = font_handle(top->font);
        vec2d  pos = { 0, 0 };
        str  stext;
        str *ptext = &text;
        text_metrics tm;
        if (ellip) {
            stext  = ellipsis(text, rect, tm);
            ptext  = &stext;
        } else
            tm     = measure(*ptext);
        auto    tb = SkTextBlob::MakeFromText(ptext->cs(), ptext->len(), (const SkFont &)f, SkTextEncoding::kUTF8);
        pos.x = mix(rect.x, rect.x + rect.w - tm.w, align.x);
        pos.y = mix(rect.y, rect.y + rect.h - tm.h, align.y);
        double skia_y_offset = (tm.descent + -tm.ascent) / 1.5;
        /// set placement rect if given (last paint is what was rendered)
        if (placement) {
            placement->x = pos.x + offset.x;
            placement->y = pos.y + offset.y;
            placement->w = tm.w;
            placement->h = tm.h;
        }
        sk_canvas->drawTextBlob(
            tb, SkScalar(pos.x + offset.x),
                SkScalar(pos.y + offset.y + skia_y_offset), ps);
    }

    void clip(rectd &rect) {
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        sk_canvas->clipRect(r);
    }

    void outline(array<glm::vec2> &line, bool is_fill = false) {
        SkPaint ps = SkPaint(top->ps);
        ps.setAntiAlias(true);
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(is_fill ? 0 : top->outline_sz);
        ps.setStroke(!is_fill);
        glm::vec2 *a = null;
        SkPath path;
        for (glm::vec2 &b: line) {
            SkPoint bp = { b.x, b.y };
            if (a) {
                path.lineTo(bp);
            } else {
                path.moveTo(bp);
            }
            a = &b;
        }
        sk_canvas->drawPath(path, ps);
    }

    void projection(glm::mat4 &m, glm::mat4 &v, glm::mat4 &p) {
        top->model      = m;
        top->view       = v;
        top->proj       = p;
    }

    void outline(array<glm::vec3> &v3, bool is_fill = false) {
        glm::vec2 sz = { this->sz.x / this->dpi_scale.x, this->sz.y / this->dpi_scale.y };
        array<glm::vec2> projected { v3.len() };
        for (glm::vec3 &vertex: v3) {
            glm::vec4 cs  = top->proj * top->view * top->model * glm::vec4(vertex, 1.0f);
            glm::vec3 ndc = cs / cs.w;
            float screenX = ((ndc.x + 1) / 2.0) * sz.x;
            float screenY = ((1 - ndc.y) / 2.0) * sz.y;
            glm::vec2  v2 = { screenX, screenY };
            projected    += v2;
        }
        outline(projected, is_fill);
    }

    void line(glm::vec3 &a, glm::vec3 &b) {
        array<glm::vec3> ab { size_t(2) };
        ab.push(a);
        ab.push(b);
        outline(ab);
    }

    void arc(glm::vec3 position, real radius, real startAngle, real endAngle, bool is_fill = false) {
        const int segments = 36;
        ion::array<glm::vec3> arcPoints { size_t(segments) };
        float angleStep = (endAngle - startAngle) / segments;

        for (int i = 0; i <= segments; ++i) {
            float     angle = glm::radians(startAngle + angleStep * i);
            glm::vec3 point;
            point.x = position.x + radius * cos(angle);
            point.y = position.y;
            point.z = position.z + radius * sin(angle);
            glm::vec4 viewSpacePoint = top->view * glm::vec4(point, 1.0f);
            glm::vec3 clippingSp     = glm::vec3(viewSpacePoint);
            arcPoints += clippingSp;
        }
        arcPoints.set_size(segments);
        outline(arcPoints, is_fill);
    }

    void outline(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(true);
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(top->outline_sz);
        ps.setStroke(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        draw_rect(rect, ps);
    }

    void outline(graphics::shape &shape) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!shape.is_rect());
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(top->outline_sz);
        ps.setStroke(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->drawPath(*sk_path(shape), ps);
    }

    void cap(graphics::cap &c) {
        top->ps.setStrokeCap(c == graphics::cap::blunt ? SkPaint::kSquare_Cap :
                             c == graphics::cap::round ? SkPaint::kRound_Cap  :
                                                         SkPaint::kButt_Cap);
    }

    void join(graphics::join &j) {
        top->ps.setStrokeJoin(j == graphics::join::bevel ? SkPaint::kBevel_Join :
                              j == graphics::join::round ? SkPaint::kRound_Join  :
                                                          SkPaint::kMiter_Join);
    }

    void translate(vec2d &tr) {
        sk_canvas->translate(SkScalar(tr.x), SkScalar(tr.y));
    }

    void scale(vec2d &sc) {
        sk_canvas->scale(SkScalar(sc.x), SkScalar(sc.y));
    }

    void rotate(double degs) {
        sk_canvas->rotate(degs);
    }

    void draw_rect(rectd &rect, SkPaint &ps) {
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        
        if (rect.rounded) {
            SkRRect rr;
            SkVector corners[4] = {
                { float(rect.r_tl.x), float(rect.r_tl.y) },
                { float(rect.r_tr.x), float(rect.r_tr.y) },
                { float(rect.r_br.x), float(rect.r_br.y) },
                { float(rect.r_bl.x), float(rect.r_bl.y) }
            };
            rr.setRectRadii(r, corners);
            sk_canvas->drawRRect(rr, ps);
        } else {
            sk_canvas->drawRect(r, ps);
        }
    }

    void fill(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setColor(sk_color(top->color));
        ps.setAntiAlias(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));

        draw_rect(rect, ps);
    }

    // we are to put everything in path.
    void fill(graphics::shape &path) {
        if (path.is_rect())
            return fill(path->bounds);
        ///
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!path.is_rect());
        ps.setColor(sk_color(top->color));
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->drawPath(*sk_path(path), ps);
    }

    void clip(graphics::shape &path) {
        sk_canvas->clipPath(*sk_path(path));
    }

    void gaussian(vec2d &sz, rectd &crop) {
        SkImageFilters::CropRect crect = { };
        if (crop) {
            SkRect rect = { SkScalar(crop.x),          SkScalar(crop.y),
                            SkScalar(crop.x + crop.w), SkScalar(crop.y + crop.h) };
            crect       = SkImageFilters::CropRect(rect);
        }
        sk_sp<SkImageFilter> filter = SkImageFilters::Blur(sz.x, sz.y, nullptr, crect);
        top->blur = sz;
        top->ps.setImageFilter(std::move(filter));
    }

    SkFont &font_handle(ion::font &font) {
        if (!font->sk_font) {
            /// dpi scaling is supported at the SkTypeface level, just add the scale x/y
            path p = font.get_path();

            SkFILEStream input((symbol)p.cs());
            assert (input.isValid());

            sk_sp<SkFontMgr>    mgr = SkFontMgr_New_Custom_Empty();
            sk_sp<SkData> font_data = SkData::MakeFromStream(&input, input.getLength());
            sk_sp<SkTypeface>     t = mgr->makeFromData(font_data);

            font->sk_font = new SkFont(t);
            ((SkFont*)font->sk_font)->setSize(font->sz);
        }
        return *(SkFont*)font->sk_font;
    }
    type_register(ICanvas);
};

mx_implement(Canvas, mx, ICanvas);


/// swap handling usage:
/*
sk_sp<SkSurface> GraphiteDawnWindowContext::getBackbufferSurface() {
    auto texture = fSwapChain.GetCurrentTexture();
    skgpu::graphite::DawnTextureInfo info(1,
                                          skgpu::Mipmapped::kNo,
                                          fSwapChainFormat,
                                          texture.GetUsage(),
                                          wgpu::TextureAspect::All);
    skgpu::graphite::BackendTexture backendTex(texture.Get());
    SkASSERT(this->graphiteRecorder());
    auto surface = SkSurfaces::WrapBackendTexture(this->graphiteRecorder(),
                                                  backendTex,
                                                  kBGRA_8888_SkColorType,
                                                  fDisplayParams.fColorSpace,
                                                  &fDisplayParams.fSurfaceProps);
    SkASSERT(surface);
    return surface;
}

void GraphiteDawnWindowContext::onSwapBuffers() {
    if (fGraphiteContext) {
        SkASSERT(fGraphiteRecorder);
        std::unique_ptr<skgpu::graphite::Recording> recording = fGraphiteRecorder->snap();
        if (recording) {
            skgpu::graphite::InsertRecordingInfo info;
            info.fRecording = recording.get();
            fGraphiteContext->insertRecording(info);
            fGraphiteContext->submit(skgpu::graphite::SyncToCpu::kNo);
        }
    }

    fSwapChain.Present();
}

*/

/// create Canvas with a texture
Canvas::Canvas(Device device, Texture texture, bool use_hidpi) : Canvas() {
    data->texture = texture;

    skwindow::DisplayParams params { };
    params.fColorType = kRGBA_8888_SkColorType;
    params.fColorSpace = SkColorSpace::MakeSRGB();
    params.fMSAASampleCount = 0;
    params.fGrContextOptions = GrContextOptions();
    params.fSurfaceProps = SkSurfaceProps(0, kBGR_H_SkPixelGeometry);
    params.fDisableVsync = false;
    params.fDelayDrawableAcquisition = false;
    params.fEnableBinaryArchive = false;
    params.fCreateProtectedNativeBackend = false;
    params.fGraphiteContextOptions.fPriv.fStoreContextRefInRecorder = true; // Needed to make synchronous readPixels work:

    skgpu::graphite::DawnBackendContext backendContext;
    backendContext.fDevice = wgpu::Device::Acquire(device->device);
    backendContext.fQueue  = backendContext.fDevice.GetQueue();

    data->fGraphiteContext = skgpu::graphite::ContextFactory::MakeDawn(
            backendContext, params.fGraphiteContextOptions.fOptions);
    data->fGraphiteRecorder = data->fGraphiteContext->makeRecorder(ToolUtils::CreateTestingRecorderOptions());

    skgpu::graphite::BackendTexture backend_texture(texture->texture);
    data->sk_surf = SkSurfaces::WrapBackendTexture(
        data->fGraphiteRecorder.get(),
        backend_texture,
        kBGRA_8888_SkColorType,
        params.fColorSpace,
        &params.fSurfaceProps);

    float xscale = 1.0f, yscale = 1.0f;

    int mcount;
    static int dpi_index = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&mcount);
    if (mcount > 0 && use_hidpi) {
        glfwGetMonitorContentScale(monitors[0], &xscale, &yscale);
    } else if (use_hidpi) {
        xscale = 2.0f;
        yscale = 2.0f;
    }
    data->dpi_scale.x = xscale;
    data->dpi_scale.y = yscale;
    data->sz          = texture->sz; // use virtual functions to obtain the virtual size
    data->sk_canvas   = data->sk_surf->getCanvas();
    data->use_hidpi   = use_hidpi;
    data->save();
}


u32 Canvas::get_virtual_width()  { return data->sz.x / data->dpi_scale.x; }
u32 Canvas::get_virtual_height() { return data->sz.y / data->dpi_scale.y; }

//void Canvas::canvas_resize(VkhImage image, int width, int height) {
//    return data->canvas_resize(image, width, height);
//}

void Canvas::font(ion::font f) {
    return data->font(f);
}
void Canvas::save() {
    return data->save();
}

void Canvas::clear() {
    return data->clear();
}

void Canvas::clear(rgbad c) {
    return data->clear(c);
}

void Canvas::color(rgbad c) {
    data->color(c);
}

void Canvas::opacity(double o) {
    data->opacity(o);
}

void Canvas::flush() {
    return data->flush();
}

void Canvas::restore() {
    return data->restore();
}
vec2i   Canvas::size() {
    return data->size();
}

void Canvas::arc(glm::vec3 pos, real radius, real startAngle, real endAngle, bool is_fill) {
    return data->arc(pos, radius, startAngle, endAngle, is_fill);
}

text_metrics Canvas::measure(str text) {
    return data->measure(text);
}

double Canvas::measure_advance(char *text, size_t len) {
    return data->measure_advance(text, len);
}

str     Canvas::ellipsis(str text, rectd rect, text_metrics &tm) {
    return data->ellipsis(text, rect, tm);
}

void    Canvas::image(ion::SVG img, rectd rect, alignment align, vec2d offset) {
    return data->image(img, rect, align, offset);
}

void    Canvas::image(ion::image img, rectd rect, alignment align, vec2d offset, bool attach_tx) {
    return data->image(img, rect, align, offset, attach_tx);
}

void    Canvas::text(str text, rectd rect, alignment align, vec2d offset, bool ellip, rectd *placement) {
    return data->text(text, rect, align, offset, ellip, placement);
}

void    Canvas::clip(rectd path) {
    return data->clip(path);
}

void Canvas::outline_sz(double sz) {
    data->outline_sz(sz);
}

void Canvas::outline(rectd rect) {
    return data->outline(rect);
}

void    Canvas::cap(graphics::cap c) {
    return data->cap(c);
}

void    Canvas::join(graphics::join j) {
    return data->join(j);
}

void    Canvas::translate(vec2d tr) {
    return data->translate(tr);
}

void    Canvas::scale(vec2d sc) {
    return data->scale(sc);
}

void    Canvas::rotate(double degs) {
    return data->rotate(degs);
}

void    Canvas::outline(array<glm::vec3> v3) {
    return data->outline(v3);
}

void Canvas::line(glm::vec3 &a, glm::vec3 &b) {
    return data->line(a, b);
}

void    Canvas::outline(array<glm::vec2> line) {
    return data->outline(line);
}

void  Canvas::projection(glm::mat4 &m, glm::mat4 &v, glm::mat4 &p) {
    return data->projection(m, v, p);
}

void    Canvas::fill(rectd rect) {
    return data->fill(rect);
}

void    Canvas::fill(graphics::shape path) {
    return data->fill(path);
}

void    Canvas::clip(graphics::shape path) {
    return data->clip(path);
}

void    Canvas::gaussian(vec2d sz, rectd crop) {
    return data->gaussian(sz, crop);
}

SVG::M::operator bool() {
    return w > 0;
}

vec2i SVG::sz() { return { data->w, data->h }; }

void SVG::set_props(EProps &eprops) {
    // iterate through fields in map (->eprops)
    for (field<EStr> &f: eprops->eprops) {
        str id_attr = f.key.hold();
        str value   = str(f.value); /// call operator str()
        array<str> ida = id_attr.split(".");
        assert(ida.len() == 2);

        symbol id   = ida[0].cs();
        symbol attr = ida[1].cs();
        symbol val  = value.cs();
        
        sk_sp<SkSVGNode>* n = (*data->svg_dom)->findNodeById(id);
        assert(n);
        
        bool set = (*n)->setAttribute(attr, val);
        assert(set);
    }
}

void SVG::render(SkCanvas *sk_canvas, int w, int h) {
    if (w == -1) w = data->w;
    if (h == -1) h = data->h;
    if (data->rw != w || data->rh != h) {
        data->rw  = w;
        data->rh  = h;
        (*data->svg_dom)->setContainerSize(
            SkSize::Make(data->rw, data->rh));
    }
    (*data->svg_dom)->render(sk_canvas);
}

array<double> font::advances(Canvas& canvas, str text) {
    num l = text.len();
    array<double> res(l+1, l+1);
    ///
    for (num i = 0; i <= l; i++) {
        str    s   = text.mid(0, i);
        double adv = canvas.measure_advance(s.data, i);
        res[i]     = adv;
    }
    return res;
}

mx_implement(Dawn, mx, IDawn);
mx_implement(Device, mx, IDevice);
mx_implement(Texture, mx, ITexture);
mx_implement(Pipeline, mx, IPipeline);
mx_implement(Pipes, mx, IPipes);
mx_implement(Window, mx, IWindow);

}
