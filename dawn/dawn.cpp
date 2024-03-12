#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#endif

#ifdef WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
/// X11 globals that conflict with Dawn
#undef None
#undef Success
#undef Always
#undef Bool

#include <mx/mx.hpp>
#include <media/image.hpp>
#include <dawn/gltf.hpp>

#include <dawn/webgpu_cpp.h>
#include <webgpu/webgpu.h>
#include <dawn/dawn_proc.h>
#include <webgpu/webgpu_glfw.h>
#include <dawn/native/DawnNative.h>
#include <dawn/utils/ComboRenderPipelineDescriptor.h>
#include <dawn/samples/SampleUtils.h>
#include <dawn/utils/SystemUtils.h>
#include <dawn/utils/WGPUHelpers.h>
#include <dawn/common/Log.h>
#include <dawn/common/Platform.h>
#include <dawn/common/SystemUtils.h>
#include <dawn/dawn.hpp>

using namespace ion;

namespace ion {

struct IDevice {
    wgpu::Device device;
    register(IDevice)
};

enums_define(TextureFormat, WGPUTextureFormat)

/// for now leave these in here; it should eventually go in a Dawn module
struct ITexture {
    wgpu::Device        device;
    wgpu::Texture       texture;
    wgpu::TextureView   view;
    wgpu::Sampler       sampler;
    vec2i               sz;
    Asset               asset_type;
    bool                updated;
    map<Texture::OnTextureResize> resize_fns;

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

static wgpu::TextureFormat preferred_swapchain_format() {
    return wgpu::TextureFormat::BGRA8Unorm;
}

void *Device::handle() {
    return &data->device;
}

void Device::get_dpi(float *xscale, float *yscale) {
    int mcount;
    static int dpi_index = 0;
    *xscale = 1.0f;
    *yscale = 1.0f;
	GLFWmonitor** monitors = glfwGetMonitors(&mcount);
    if (mcount > 0) {
        glfwGetMonitorContentScale(monitors[0], xscale, yscale);
    }
}

Texture Device::create_texture(vec2i sz) {
    Texture texture;
    texture->device = data->device;
    texture.resize(sz);
    return texture; 
}

vec2i Texture::size() {
    return data->sz;
}

void *Texture::handle() {
    return &data->texture;
}

void Texture::on_resize(str user, OnTextureResize fn) {
    data->resize_fns[user] = fn;
}

void Texture::resize(vec2i sz) {
    wgpu::TextureDescriptor tx_desc;
    tx_desc.dimension               = wgpu::TextureDimension::e2D;
    tx_desc.size.width              = (u32)sz.x;
    tx_desc.size.height             = (u32)sz.y;
    tx_desc.size.depthOrArrayLayers = 1;
    tx_desc.sampleCount             = 1;
    tx_desc.format                  = preferred_swapchain_format();
    tx_desc.mipLevelCount           = 1;
    tx_desc.usage                   = wgpu::TextureUsage::CopyDst | 
                                      wgpu::TextureUsage::TextureBinding | 
                                      wgpu::TextureUsage::RenderAttachment;

    data->texture = data->device.CreateTexture(&tx_desc);
    data->view    = data->texture.CreateView();
    data->sz      = sz;
    data->sampler = data->device.CreateSampler();
    for (field<Texture::OnTextureResize> &f: data->resize_fns) {
        f.value(sz);
    }
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

Texture Texture::of_size(Device &dev, vec2i size, TextureFormat f) {
    wgpu::Device            device  = dev->device;
    Texture                 texture;
    WGPUTextureFormat       format  = f.convert();
    wgpu::TextureDescriptor   tx_desc {
        .usage                      = wgpu::TextureUsage::CopyDst        | 
                                      wgpu::TextureUsage::TextureBinding | 
                                      wgpu::TextureUsage::RenderAttachment,
        .dimension                  = wgpu::TextureDimension::e2D,
        .size                       = { (u32)size.x, (u32)size.y, 1 },
        .format                     = preferred_swapchain_format(),
        .mipLevelCount              = 1,
        .sampleCount                = 1
    };
    wgpu::TextureViewDescriptor vdesc {
        .format                     = preferred_swapchain_format(),
        .dimension                  = wgpu::TextureViewDimension::e2D,
        .baseMipLevel               = 0,
        .mipLevelCount              = 1,
        .baseArrayLayer             = 0,
        .arrayLayerCount            = 1
    };

    texture->device                 = device;
    texture->sz                     = size;
    texture->texture                = device.CreateTexture(&tx_desc);
    texture->view                   = texture->texture.CreateView(&vdesc);
    
    wgpu::SamplerDescriptor sampler_desc {
        .addressModeU               = wgpu::AddressMode::Repeat,
        .addressModeV               = wgpu::AddressMode::Repeat,
        .addressModeW               = wgpu::AddressMode::Repeat,
        .magFilter                  = wgpu::FilterMode::Linear,
        .minFilter                  = wgpu::FilterMode::Linear,
        .mipmapFilter               = wgpu::MipmapFilterMode::Linear,
        .lodMinClamp                = 0.0f,
        .lodMaxClamp                = FLT_MAX,
        .compare                    = wgpu::CompareFunction::Undefined
    };
    
    texture->sampler                = device.CreateSampler(&sampler_desc);
    return texture;
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
        wgpu::BackendType backend_type = wgpu::BackendType::D3D12;
    #elif defined(__APPLE__)
        wgpu::BackendType backend_type = wgpu::BackendType::Metal;
    #elif defined(__linux__)
        wgpu::BackendType backend_type = wgpu::BackendType::Vulkan;
    #endif
    std::unique_ptr<dawn::native::Instance> instance;
    register(IDawn);
};

struct IPipeline {
    static inline ion::map<wgpu::ShaderModule> mod_cache;

    mx                          uniform_data;
    type_t                      vertex_type;
    mx                          user; // user data

    wgpu::RenderPipeline        pipeline;
    wgpu::BindGroup             bind_group;
    wgpu::BindGroupLayout       bgl;
    wgpu::Buffer                index_buffer;
    wgpu::Buffer                vertex_buffer;
    wgpu::PipelineLayout        pl;
    wgpu::ShaderModule          mod;

    str                         name; // Pipelines represent 'parts' of models and must have a name
    Device                      device;
    memory*                     gmem; // graphics memory (grabbed)
    Graphics                    gfx;
    gltf::Model                 m;
    size_t                      indices_count;
    bool                        init;
    watch                       watcher;
    bool                        updated;
    Texture                     textures[Asset::count - 1];

    static wgpu::Buffer create_buffer(wgpu::Device device, mx mx_data, wgpu::BufferUsage usage) {
        uint64_t size = mx_data.total_size();
        void    *data = mx_data.mem->origin;
        wgpu::BufferDescriptor descriptor {
            .usage = usage | wgpu::BufferUsage::CopyDst,
            .size  = size
        };
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
        device.GetQueue().WriteBuffer(buffer, 0, data, size);
        return buffer;
    }

    void load_from_gltf(gltf::Model &m, str &part, mx &mx_vbuffer, mx &mx_ibuffer) {
        /// model must have been loaded
        assert(m->nodes);
        using namespace gltf;
        /// load single part from gltf (a Pipes object effectively calls this multiple times for multiple Pipelines)
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
                    assert(gfx->vtype->meta_map);
                    prop*  p = (*gfx->vtype->meta_map)[prop_sym];
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
                u8 *vbuf = (u8*)calloc(vlen, gfx->vtype->base_sz);

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
                        dst += gfx->vtype->base_sz;
                    }
                }
                /// create vertex buffer by wrapping what we've copied from allocation (we have a primitive array)
                mx verts { memory::wrap(gfx->vtype, vbuf, vlen) }; /// load indices (always store 32bit uint)
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

    void load_shader(Graphics &gfx) {
        if (!mod_cache->count(gfx->shader)) {
            path shader_path = fmt { "shaders/{0}.wgl", { gfx->shader } };
            str  shader_code = shader_path.read<str>();
            mod_cache[gfx->shader] = dawn::utils::CreateShaderModule(device->device, shader_code);
        }
        mod = mod_cache[gfx->shader];
    }

    /// loads/associates uniforms here (change from vk where that was a separate process)
    void load_bindings(Graphics &gfx) {
        /// setup bindings
        wgpu::Device device = this->device->device;
        array<wgpu::BindGroupEntry>       bind_values (gfx->bindings.count());
        array<wgpu::BindGroupLayoutEntry> bind_entries(gfx->bindings.count());
        size_t bind_id = 0;
        Texture tx;

        for (mx& binding: gfx->bindings) { /// array of mx can tell us what type of data it is
            wgpu::BindGroupLayoutEntry entry {
                .binding    = u32(bind_id),
                .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment /// probably best to look through the shader code after @fragment and @vertex
            };
            wgpu::BindGroupEntry bind_value {
                .binding    = u32(bind_id)
            };

            if (binding.type() == typeof(Texture)) {
                tx = binding.hold();
                tx.on_resize(name, [&](vec2i sz) {
                    reload();
                });
                entry.texture.multisampled  = false;
                entry.texture.sampleType    = wgpu::TextureSampleType::Float;
                entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
                bind_value.textureView      = tx->view;
            } else if (binding.type() == typeof(Sampling::etype)) {
                /// this must be specified immediately after the texture
                assert(tx);
                Sampling sampling(binding.hold());
                entry.sampler.type          = sampling == Sampling::nearest ? wgpu::SamplerBindingType::NonFiltering : wgpu::SamplerBindingType::Filtering;
                bind_value.sampler          = tx->sampler;
            } else {
                /// other objects are Uniform buffers
                entry.buffer.type = wgpu::BufferBindingType::Uniform;

                size_t sz = binding.type()->base_sz; /// was UniformBufferObject
                wgpu::BufferDescriptor desc = {
                    .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                    .size = sz
                };
                bind_value.buffer = device.CreateBuffer(&desc);
            }

            bind_entries.push(entry);
            bind_values.push(bind_value);
            bind_id++;
        }

        wgpu::BindGroupLayoutDescriptor ld {
            .entryCount         = size_t(bind_id),
            .entries            = bind_entries.data
        };
        /// create bind group layout
        bgl = device.CreateBindGroupLayout(&ld);

        wgpu::BindGroupDescriptor bg {
            .layout             = bgl,
            .entryCount         = size_t(bind_id),
            .entries            = bind_values.data
        };

        /// create bind group, and pipeline layout (stored)
        bind_group = device.CreateBindGroup(&bg);
        pl = dawn::utils::MakeBasicPipelineLayout(device, &bgl);
    }

    /// get meta attributes on the Vertex type (all must be used in shader)
    void create_with_attrs(Graphics &gfx) {
        wgpu::Device  device        = this->device->device;
        size_t        attrib_count  = 0;
        doubly<prop> &props         = *(doubly<prop>*)gfx->vtype->meta;

        auto get_wgpu_format = [](prop &p) {
            if (p.type == typeof(glm::vec2))  return wgpu::VertexFormat::Float32x2;
            if (p.type == typeof(glm::vec3))  return wgpu::VertexFormat::Float32x3;
            if (p.type == typeof(glm::vec4))  return wgpu::VertexFormat::Float32x4;
            if (p.type == typeof(glm::ivec4)) return wgpu::VertexFormat::Sint32x4;
            if (p.type == typeof(float))      return wgpu::VertexFormat::Float32;
            ///
            assert(false);
            return wgpu::VertexFormat::Undefined;
        };

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

        dawn::utils::ComboRenderPipelineDescriptor render_desc;
        render_desc.layout = pl;
        render_desc.vertex.module = mod;
        render_desc.vertex.bufferCount = 1;
        render_desc.cBuffers[0].arrayStride = gfx->vtype->base_sz;// sizeof(Attribs);
        for (prop &p: props) {
            render_desc.cAttributes[attrib_count].shaderLocation = attrib_count;
            render_desc.cAttributes[attrib_count].format         = get_wgpu_format(p);
            render_desc.cAttributes[attrib_count].offset         = p.offset;
            attrib_count++;
        }
        render_desc.cBuffers[0].attributeCount = attrib_count;
        render_desc.cFragment.module = mod;
        render_desc.cTargets[0].format = preferred_swapchain_format();
        render_desc.EnableDepthStencil(wgpu::TextureFormat::Depth24PlusStencil8);

        pipeline = device.CreateRenderPipeline(&render_desc);
        Dawn dawn;
        dawn.process_events();
    }

    /// create pipeline with shader, textures, vbo/ibo, bindings, attribs, blending and stencil info
    void reload() {
        str            part   = gfx->name;
        type_t         vtype  = gfx->vtype;
        wgpu::Device   device = this->device->device;
        mx             mx_vbuffer;
        mx             mx_ibuffer;
        array<image>   images;

        /// load vbo/ibo data:
        if (gfx->gen) {
            /// generate based on user routine
            gfx->gen(mx_vbuffer, mx_ibuffer, images);
        } else {
            /// otherwise load from gltf
            load_from_gltf(m, part, mx_vbuffer, mx_ibuffer);
        }

        index_buffer  = create_buffer(device, mx_ibuffer, wgpu::BufferUsage::Index);
        vertex_buffer = create_buffer(device, mx_vbuffer, wgpu::BufferUsage::Vertex);
        indices_count = mx_ibuffer.count();

        load_shader(gfx);
        load_bindings(gfx);
        create_with_attrs(gfx);
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

    void submit(wgpu::TextureView color_view, wgpu::TextureView depth_stencil_view, states<Clear> clear_states, rgbaf clear_color) {
        for (auto p: pipelines) {
            p->submit(color_view, depth_stencil_view, clear_states, clear_color);
        }
    }
    register(IPipes);
};

Pipeline::Pipeline(Device &device, gltf::Model &m, Graphics graphics):Pipeline() { /// instead of a texture it needs flags for the resources to load
    data->gmem           = graphics.hold(); /// just so we can hold onto the data; we must drop this in our dtr
    data->gfx            = graphics.data;
    data->device         = device;
    data->name           = graphics->name;
    data->m              = m;
}

Pipes::Pipes(Device &device, symbol model, array<Graphics> parts):Pipes() {
    using namespace gltf;
    data->device = device;
    data->model = model;

    if (model) {
        path gltf_path = fmt {"models/{0}.gltf", { model }};
        data->m = Model::load(gltf_path);
    }

    for (Graphics &gfx: parts)
        data->pipelines += Pipeline(device, data->m, gfx);

    for (Pipeline &p: data->pipelines)
        p->reload();
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

struct Presentation {
    OnWindowPresent on_present;
    OnWindowResize  on_resize;
};

struct IWindow {
    Dawn                        dawn;
    ion::str                    title;
    GLFWwindow*                 handle;
    wgpu::AdapterType           adapter_type = wgpu::AdapterType::Unknown;
    Window::OnCursorEnter       cursor_enter;
    Window::OnCursorScroll      cursor_scroll;
    Window::OnCursorPos         cursor_pos;
    Window::OnCursorButton      cursor_button;
    Window::OnKeyChar           key_char;
    Window::OnKeyScanCode       key_scancode;
    Window::OnCanvasRender      fn_canvas_render;
    Window::OnSceneRender       fn_scene_render;

    DawnProcTable               procs;
    WGPUDevice                  gpu;
    Device                      device;
    Texture                     texture;
    wgpu::Sampler               sampler; /// 'device' default sampler
    wgpu::Queue                 queue;
    wgpu::SwapChain             swapchain;
    wgpu::TextureView           depth_stencil_view;
    
    array<Presentation>         presentations;
    //Pipes                       canvas_pipeline;
    //Canvas                      canvas;
    //Scene                       scene;

    vec2i                       sz;
    void                       *user_data;

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
        vec2i sz { width, height };
        for (Presentation &p: iwin->presentations)
            p.on_resize(sz);
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
        descriptor.format = wgpu::TextureFormat::Depth24PlusStencil8;
        descriptor.mipLevelCount = 1;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment;
        auto depthStencilTexture = device->device.CreateTexture(&descriptor);
        return depthStencilTexture.CreateView();
    }

    void update_swap() {
        static std::unique_ptr<wgpu::ChainedStruct> surfaceChainedDesc;
        surfaceChainedDesc = wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(handle); /// this needs some work because this is called more than once.

        WGPUSurfaceDescriptor surfaceDesc;
        surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(surfaceChainedDesc.get());
        WGPUSurface surface = procs.instanceCreateSurface(dawn->instance->Get(), &surfaceDesc);

        WGPUSwapChainDescriptor swapChainDesc = {};
        swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
        swapChainDesc.format = static_cast<WGPUTextureFormat>(preferred_swapchain_format());
        swapChainDesc.width = (u32)sz.x;
        swapChainDesc.height = (u32)sz.y;
        swapChainDesc.presentMode = WGPUPresentMode_Mailbox;
        WGPUSwapChain backendSwapChain =
            procs.deviceCreateSwapChain(gpu, surface, &swapChainDesc);
        swapchain = wgpu::SwapChain::Acquire(backendSwapChain);

        depth_stencil_view = create_depth_stencil_view();
    }

    void create_resources() {
        if (!dawn->instance) {
            WGPUInstanceDescriptor instanceDescriptor{};
            instanceDescriptor.features.timedWaitAnyEnable = true;
            dawn->instance = std::make_unique<dawn::native::Instance>(&instanceDescriptor);
        }
        wgpu::RequestAdapterOptions options = {};
        options.backendType = dawn->backend_type;

        // Get an adapter for the backend to use, and create the device.
        auto adapters = dawn->instance->EnumerateAdapters(&options);
        wgpu::DawnAdapterPropertiesPowerPreference power_props{};
        wgpu::AdapterProperties adapterProperties{};
        adapterProperties.nextInChain = &power_props;
        // Find the first adapter which satisfies the adapterType requirement.
        auto isAdapterType = [&](const auto& adapter) -> bool {
            // picks the first adapter when adapterType is unknown.
            if (adapter_type == wgpu::AdapterType::Unknown) {
                return true;
            }
            adapter.GetProperties(&adapterProperties);
            return adapterProperties.adapterType == adapter_type;
        };
        auto preferredAdapter = std::find_if(adapters.begin(), adapters.end(), isAdapterType);
        if (preferredAdapter == adapters.end()) {
            fprintf(stderr, "Failed to find an adapter! Please try another adapter type.\n");
            return;
        }

        std::vector<const char*> enableToggleNames = {
            "allow_unsafe_apis",  "use_user_defined_labels_in_backend" // allow_unsafe_apis = Needed for dual-source blending, BufferMapExtendedUsages.
        };
        std::vector<const char*> disabledToggleNames;
        WGPUDawnTogglesDescriptor toggles;
        toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
        toggles.chain.next = nullptr;
        toggles.enabledToggles = enableToggleNames.data();
        toggles.enabledToggleCount = enableToggleNames.size();
        toggles.disabledToggles = disabledToggleNames.data();
        toggles.disabledToggleCount = disabledToggleNames.size();

        WGPUDeviceDescriptor deviceDesc = {};

        std::vector<wgpu::FeatureName> requiredFeatures;
        requiredFeatures.push_back(wgpu::FeatureName::SurfaceCapabilities);
        //if (adapter.HasFeature(wgpu::FeatureName::BufferMapExtendedUsages)) {
        //    requiredFeatures.push_back(wgpu::FeatureName::BufferMapExtendedUsages);
        //}

        deviceDesc.requiredFeatures = (WGPUFeatureName*)requiredFeatures.data();
        deviceDesc.requiredFeatureCount = requiredFeatures.size();

        deviceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&toggles);

        gpu = preferredAdapter->CreateDevice(&deviceDesc);

        procs = dawn::native::GetProcs();
        dawnProcSetProcs(&procs);
        procs.deviceSetUncapturedErrorCallback(gpu, PrintDeviceError, nullptr);
        procs.deviceSetDeviceLostCallback(gpu, DeviceLostCallback, nullptr);
        procs.deviceSetLoggingCallback(gpu, DeviceLogCallback, nullptr);
        device->device = wgpu::Device::Acquire(gpu);
        queue  = device->device.GetQueue();

        update_swap();
    }

    void register_presentation(OnWindowPresent on_present, OnWindowResize on_resize) {
        presentations += Presentation { on_present, on_resize };
        on_resize(sz);
    }

    bool process() {
        if (glfwWindowShouldClose(handle))
            return false;
        glfwPollEvents();
        dawn.process_events();

        wgpu::TextureView swap_view = swapchain.GetCurrentTextureView();
        states<Clear> clear_states { Clear::Color, Clear::Depth };
        rgbaf clear_color { 0.0f, 0.0f, 0.5f, 1.0f };

        for (Presentation p: presentations) {
            Scene scene = p.on_present();
            for (Pipes &p: scene) {
                for (Pipeline &pipeline: p->pipelines) {
                    pipeline->submit(swap_view, depth_stencil_view, clear_states, clear_color);
                    //clear_states[Clear::Color] = false;
                    //clear_states[Clear::Depth] = false;
                    //clear_states[Clear::Stencil] = false;
                }
            }
        }

        swapchain.Present();
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

void Window::register_presentation(OnWindowPresent on_present, OnWindowResize on_resize) {
    data->register_presentation(on_present, on_resize);
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

Texture Window::texture() {
    return data->texture;
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
    dawn::native::InstanceProcessEvents(data->instance->Get());
}

float Dawn::get_dpi() {
    glfwInit();
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    float xscale, yscale;
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    return xscale;
}

mx_implement(Dawn, mx, IDawn);
mx_implement(Device, mx, IDevice);
mx_implement(Texture, mx, ITexture);
mx_implement(Pipeline, mx, IPipeline);
mx_implement(Pipes, mx, IPipes);
mx_implement(Window, mx, IWindow);

}