#include <dawn/dawn.hpp>

#include <trinity/trinity.hpp>
#include <trinity/gltf.hpp>
#include <trinity/subdiv.hpp>

using namespace ion;

namespace ion {

struct IDawn;
struct Dawn:mx {
    void process_events();
    static float get_dpi();
    mx_declare(Dawn, mx, IDawn);
};

template <> struct is_singleton<IDawn> : true_type { };
template <> struct is_singleton<Dawn>  : true_type { }; // this one, the above is not a flag to make the data singular; redundant

struct IDevice {
    wgpu::Adapter       adapter;
    wgpu::Device        wgpu;
    wgpu::Queue         queue;

    wgpu::Buffer create_buffer(mx mx_data, wgpu::BufferUsage usage) {
        namespace WGPU = wgpu;
        wgpu::Device wgpu_device = this->wgpu;
        uint64_t size = mx_data.total_size();
        void    *data = mx_data.mem->origin;
        WGPU::BufferDescriptor descriptor {
            .usage = usage | WGPU::BufferUsage::CopyDst,
            .size  = size
        };
        WGPU::Buffer buffer = wgpu_device.CreateBuffer(&descriptor);
        queue.WriteBuffer(buffer, 0, data, size);
        return buffer;
    }

    register(IDevice)
};


void *Device::handle() {
    return &data->wgpu;
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

struct ITexture {
    Device              device;
    wgpu::Texture       texture;
    wgpu::TextureFormat format;
    wgpu::TextureView   view;
    wgpu::Sampler       sampler;
    wgpu::TextureUsage  usage;
    vec2i               sz;
    Asset               asset_type;
    map<Texture::OnTextureResize> resize_fns;

    wgpu::TextureFormat asset_format(Asset &asset) {
        switch (asset.value) {
            case Asset::depth_stencil: return wgpu::TextureFormat::Depth24PlusStencil8;
            case Asset::env: return wgpu::TextureFormat::RGBA16Float; /// load with OpenEXR
            default:         return wgpu::TextureFormat::BGRA8Unorm;
        }
    }

    wgpu::TextureDimension asset_dimension(Asset &asset) {
        return wgpu::TextureDimension::e2D;
    }

    ITexture() { }

    /// image can be created with nothing specified, in which case we produce an rgba8 based on Asset
    /// otherwise create a blank image; based on asset type, it will be black or white
    void set_content(mx content) {
        ion::image img;
        if (content.type() == typeof(ion::image))
            img = content.hold();

        if (!img) {
            img = ion::size { 2, 2 };
            ion::rgba8 *pixels = img.data;
            if (asset_type == Asset::env) /// a light map contains white by default
                memset(pixels, 255, sizeof(ion::rgba8) * (img.width() * img.height()));
        }
        ion::rgba8 *pixels = img.data;
        /// update size
        sz = { int(img.width()), int(img.height()) };
        assert(format == asset_format(asset_type));
        
    }

    /// needs some controls on mip levels for view
    void create(Device &device, int w, int h, int array_layers, Asset asset_type, wgpu::TextureUsage usage) {
        assert(device->wgpu);
        /// skia requires this.  it likely has its own swap chain
        if (asset_type == Asset::attachment)
            usage |= wgpu::TextureUsage::RenderAttachment;

        wgpu::TextureDescriptor tx_desc;
        tx_desc.dimension               = asset_dimension(asset_type);
        tx_desc.size.width              = (u32)w;
        tx_desc.size.height             = (u32)h;
        tx_desc.size.depthOrArrayLayers = (u32)array_layers;
        tx_desc.sampleCount             = 1;
        tx_desc.format                  = asset_format(asset_type);
        tx_desc.mipLevelCount           = 1;
        tx_desc.usage                   = usage;

        this->device    = device;
        this->asset_type = asset_type;
        this->texture   = device->wgpu.CreateTexture(&tx_desc);
        if (usage & (wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment))
            view        = texture.CreateView();
        if (usage & (wgpu::TextureUsage::TextureBinding))
            sampler     = device->wgpu.CreateSampler();
        this->format    = asset_format(asset_type);
        this->sz        = vec2i(w, h);
        this->usage     = usage;
        for (field<Texture::OnTextureResize> &f: resize_fns)
            f.value(sz);
    }

    operator bool() { return bool(texture); }
    type_register(ITexture);
};

static wgpu::TextureFormat preferred_swapchain_format() {
    return wgpu::TextureFormat::BGRA8Unorm;
}

Device &Texture::device() {
    return data->device;
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

void Texture::cleanup_resize(str user) {
    assert(data->resize_fns->remove(user));
}


void Texture::resize(vec2i sz) {
    data->create(data->device, sz.x, sz.y, 1, data->asset_type, data->usage);
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

Texture Texture::from_image(Device &device, image img, Asset asset_type) {
    Texture tx;
    vec2i sz(img.width(), img.height());
    tx->create(device, sz.x, sz.y, 1, asset_type,
        wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding);
    tx->set_content(img);
    return tx;
}

/// make this not a static method; change the texture already in memory
Texture Texture::load(Device &dev, symbol name, Asset type) {
    return from_image(dev, asset_image(name, type), type);
}

/// formats and usage not needed because we have asset
Texture Device::create_texture(vec2i sz, Asset asset_type) {
    Texture tx;
    tx->create(*this, sz.x, sz.y, 1, asset_type,
        wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding);
    return tx;
}

void Texture::set_content(mx content) {
    data->set_content(content);
}

struct IDawn {
    #if defined(WIN32)
        wgpu::BackendType backend_type = wgpu::BackendType::D3D12;
    #elif defined(__APPLE__)
        wgpu::BackendType backend_type = wgpu::BackendType::Metal;
    #elif defined(__linux__)
        wgpu::BackendType backend_type = wgpu::BackendType::Vulkan;
    #endif
    wgpu::Instance  instance;
    DawnProcTable   procs;
    Device          device;

    static void adapter_ready(WGPURequestAdapterStatus status, WGPUAdapter wgpu_adapter, const char* message, void* userdata) {
        if (status == WGPURequestAdapterStatus_Success) {
            wgpu::Adapter* adapter = (wgpu::Adapter*)userdata;
            *adapter = wgpu::Adapter::Acquire(wgpu_adapter);
        } else {
            console.fault("adapter not ready");
        }
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

    void init() {
        procs = dawn::native::GetProcs();
        dawnProcSetProcs(&procs);
        wgpu::InstanceDescriptor desc {
            .features = { .timedWaitAnyEnable = true }
        };
        instance = wgpu::CreateInstance(&desc);

        wgpu::RequestAdapterOptions options {
            .powerPreference = wgpu::PowerPreference::HighPerformance,
            .backendType = backend_type
        };
        
        wgpu::Adapter adapter;
        instance.RequestAdapter(&options, adapter_ready, (void*)&adapter);

        std::vector<const char*> enableToggleNames = {
            "allow_unsafe_apis",  "use_user_defined_labels_in_backend" // allow_unsafe_apis = Needed for dual-source blending, BufferMapExtendedUsages.
        };
        std::vector<const char*> disabledToggleNames;
        wgpu::DawnTogglesDescriptor toggles;
        toggles.enabledToggles       = enableToggleNames.data();
        toggles.enabledToggleCount   = enableToggleNames.size();
        toggles.disabledToggles      = disabledToggleNames.data();
        toggles.disabledToggleCount  = disabledToggleNames.size();

        std::vector<wgpu::FeatureName> requiredFeatures;
        requiredFeatures.push_back(wgpu::FeatureName::SurfaceCapabilities);

        wgpu::DeviceDescriptor deviceDesc {
            .nextInChain            = &toggles,
            .requiredFeatureCount   = requiredFeatures.size(),
            .requiredFeatures       = requiredFeatures.data()
        };

        device->wgpu    = adapter.CreateDevice(&deviceDesc);
        device->adapter = adapter;
        device->queue   = device->wgpu.GetQueue();
        WGPUDevice dev  = device->wgpu.Get();
        procs.deviceSetUncapturedErrorCallback(dev, PrintDeviceError, nullptr);
        procs.deviceSetDeviceLostCallback(dev, DeviceLostCallback, nullptr);
        procs.deviceSetLoggingCallback(dev, DeviceLogCallback, nullptr);

        #ifdef __APPLE__
        AllowKeyRepeats(); // can go in ux module perhaps
        #endif
        assert(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
        glfwSetErrorCallback(PrintGLFWError);
    }
    register(IDawn);
};

struct UniformData {
    Uniform      u;
    wgpu::Buffer buffer;
    register(UniformData);
};

struct IPipeline {
    static inline ion::map<wgpu::ShaderModule> mod_cache;

    wgpu::RenderPipeline        pipeline;
    wgpu::BindGroup             bind_group;
    wgpu::BindGroupLayout       bgl;
    wgpu::Buffer                index_buffer;
    wgpu::Buffer                vertex_buffer;
    wgpu::PipelineLayout        pl;
    wgpu::ShaderModule          mod;
    array<UniformData>          uniforms;

    str                         name; // Pipelines represent 'parts' of models and must have a name
    Device                      device;
    Graphics                    gfx;
    gltf::Model                 m;
    size_t                      indices_count;
    Texture                     textures[Asset::count - 1];

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
            mod_cache[gfx->shader] = dawn::utils::CreateShaderModule(device->wgpu, shader_code);
        }
        mod = mod_cache[gfx->shader];
    }

    /// loads/associates uniforms here (change from vk where that was a separate process)
    void load_bindings(Graphics &gfx) {
        wgpu::Device device = this->device->wgpu;
        array<wgpu::BindGroupEntry>       bind_values (gfx->bindings.count());
        array<wgpu::BindGroupLayoutEntry> bind_entries(gfx->bindings.count());
        size_t bind_id = 0;
        Texture tx;

        uniforms.clear();
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
                    load_bindings(gfx);
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
            } else if (binding.type() == typeof(Uniform)) {
                Uniform u = binding.hold();
                size_t data_sz = u->uniform_data.type()->base_sz; /// fix this name

                entry.buffer.type = wgpu::BufferBindingType::Uniform;
                wgpu::BufferDescriptor desc = {
                    .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                    .size  = data_sz
                };
                bind_value.buffer = device.CreateBuffer(&desc); // this needs to be updated each frame
                uniforms += { u, bind_value.buffer };
            } else {
                assert(false);
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
        wgpu::Device  device        = this->device->wgpu;
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
        render_desc.cTargets[0].blend = &blend;
        render_desc.EnableDepthStencil(wgpu::TextureFormat::Depth24PlusStencil8);

        pipeline = device.CreateRenderPipeline(&render_desc);
        Dawn dawn;
        dawn.process_events();
    }

    /// create pipeline with shader, textures, vbo/ibo, bindings, attribs, blending and stencil info
    void reload() {
        str            part   = gfx->name;
        type_t         vtype  = gfx->vtype;
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

        index_buffer  = device->create_buffer(mx_ibuffer, wgpu::BufferUsage::Index);
        vertex_buffer = device->create_buffer(mx_vbuffer, wgpu::BufferUsage::Vertex);
        indices_count = mx_ibuffer.count();

        load_shader(gfx);
        load_bindings(gfx);
        create_with_attrs(gfx);
    }

    void submit(wgpu::TextureView color_view, wgpu::TextureView depth_stencil, states<Clear> clear_states, rgbaf clear_color) {
        for (UniformData &udata: uniforms) {
            u64 sz = udata.buffer.GetSize();
            assert(sz == udata.u.size());
            device->queue.WriteBuffer(udata.buffer, 0, udata.u.address(), sz);
        }

        wgpu::RenderPassColorAttachment color_attachment {
            .view = color_view,
            .loadOp = clear_states[Clear::Color] ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = wgpu::Color { clear_color.r, clear_color.g, clear_color.b, clear_color.a }
        };
        wgpu::RenderPassDepthStencilAttachment depth_attachment {
            .view = depth_stencil,
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
            .depthStencilAttachment = depth_stencil ? &depth_attachment : null
        };

        wgpu::Queue queue = device->queue;
        wgpu::CommandEncoder encoder = device->wgpu.CreateCommandEncoder();
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
        for (auto p: pipelines)
            p->submit(color_view, depth_stencil_view, clear_states, clear_color);
    }
    register(IPipes);
};

Pipes::Pipes(Device &device, symbol model, array<Graphics> parts):Pipes() {
    using namespace gltf;
    data->device = device;
    data->model = model;

    if (model) {
        path gltf_path = fmt {"models/{0}.gltf", { model }};
        data->m = Model::load(gltf_path);
    }

    for (Graphics &gfx: parts) {
        Pipeline p;
        p->gfx            = gfx;
        p->device         = device;
        p->name           = gfx->name;
        p->m              = data->m;
        data->pipelines  += p;
    }

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
    Texture                     depth_stencil;
    wgpu::Surface               surface;
    wgpu::SwapChain             swapchain;
    array<Presentation>         presentations;
    vec2i                       sz;
    void                       *user_data;

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

    void update_depth_stencil() {
        depth_stencil->create(dawn->device, sz.x, sz.y, 1, 
            Asset::depth_stencil,
            wgpu::TextureUsage::RenderAttachment);
    }

    void update_swap() {
        surface = wgpu::glfw::CreateSurfaceForWindow(dawn->instance, handle); /// this needs some work because this is called more than once.

        wgpu::SwapChainDescriptor swapChainDesc {
            .usage          = wgpu::TextureUsage::RenderAttachment,
            .format         = preferred_swapchain_format(),
            .width          = (u32)sz.x,
            .height         = (u32)sz.y,
            .presentMode    = wgpu::PresentMode::Mailbox
        };

        swapchain = dawn->device->wgpu.CreateSwapChain(surface, &swapChainDesc);
        update_depth_stencil();
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
                    pipeline->submit(swap_view, depth_stencil->view, clear_states, clear_color);
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
        update_swap();
    }

    static inline int count = 0;

    ~IWindow() {
        close();
    }

    void close() {
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
    return bool(data->dawn->device);
}

Device Window::device() {
    return data->dawn->device;
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
    data->instance.ProcessEvents();
}

float Dawn::get_dpi() {
    glfwInit();
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    float xscale, yscale;
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    return xscale;
}

mx_implement(Dawn,      mx, IDawn);
mx_implement(Device,    mx, IDevice);
mx_implement(Texture,   mx, ITexture);
mx_implement(Pipeline,  mx, IPipeline);
mx_implement(Pipes,     mx, IPipes);
mx_implement(Window,    mx, IWindow);
}