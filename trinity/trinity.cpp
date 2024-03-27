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

template <> struct is_singleton<Dawn>  : true_type { };

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
    wgpu::TextureDimension dim;
    wgpu::TextureFormat format;
    wgpu::TextureView   view;
    //wgpu::Sampler       sampler; (not associated here, its bound in Pipeline)
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

    wgpu::TextureViewDimension view_dimension() {
        switch (dim) {
            case wgpu::TextureDimension::e1D: return wgpu::TextureViewDimension::e1D;
            case wgpu::TextureDimension::e2D: return wgpu::TextureViewDimension::e2D;
            case wgpu::TextureDimension::e3D: return wgpu::TextureViewDimension::e3D;
        }
        return wgpu::TextureViewDimension::Undefined;
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
    void create(Device &device, int w, int h, int array_layers, int sample_count, Asset asset_type, wgpu::TextureUsage usage) {
        assert(device->wgpu);
        /// skia requires this.  it likely has its own swap chain
        if (asset_type == Asset::attachment)
            usage |= wgpu::TextureUsage::RenderAttachment;

        wgpu::TextureDescriptor tx_desc;
        tx_desc.dimension               = asset_dimension(asset_type);
        tx_desc.size.width              = (u32)w;
        tx_desc.size.height             = (u32)h;
        tx_desc.size.depthOrArrayLayers = (u32)array_layers;
        tx_desc.sampleCount             = sample_count;
        tx_desc.format                  = asset_format(asset_type);
        tx_desc.mipLevelCount           = 1;
        tx_desc.usage                   = usage;

        this->device    = device;
        this->dim       = tx_desc.dimension;
        this->asset_type = asset_type;
        this->texture   = device->wgpu.CreateTexture(&tx_desc);
        if (usage & (wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment))
            view        = texture.CreateView();
        //if (usage & (wgpu::TextureUsage::TextureBinding))
        //    sampler     = device->wgpu.CreateSampler();
        this->format    = asset_format(asset_type);
        this->sz        = vec2i(w, h);
        this->usage     = usage;
        for (field<Texture::OnTextureResize> &f: resize_fns)
            f.value(sz);
    }

    operator bool() { return bool(texture); }
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
    data->create(data->device, sz.x, sz.y, 1, 1, data->asset_type, data->usage);
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
    tx->create(device, sz.x, sz.y, 1, 1, asset_type,
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
    tx->create(*this, sz.x, sz.y, 1, 1, asset_type,
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
        //AllowKeyRepeats(); // can go in ux module perhaps
        #endif
        assert(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
        glfwSetErrorCallback(PrintGLFWError);
    }
};

struct IRenderable {
    str name; /// needs the same identification
    array<mx> var_data; /// from ShaderVar::alloc()
};

struct IObject {
    Model model;
    array<IRenderable> renderables;
};

struct IVar {
    wgpu::Buffer      buffer;
    wgpu::Sampler     sampler;
    ShaderVar         svar;
    lambda<mx()>      instance; /// user-defined routine for instancing mx data
    lambda<mx(mx&)>   update;   /// user-defined routine for updating mx data
};

struct IPipeline {
    static inline ion::map<wgpu::ShaderModule> mod_cache;

    str                         name;
    
    wgpu::Buffer                triangle_buffer;
    wgpu::Buffer                line_buffer;
    wgpu::Buffer                vertex_buffer;
    array<IVar>                 ivars;

    wgpu::RenderPipeline        pipeline, pipeline_wire;
    wgpu::BindGroup             bind_group;
    wgpu::BindGroupLayout       bgl;
    wgpu::PipelineLayout        pipeline_layout;
    wgpu::ShaderModule          mod;

    rgbaf                       clear_color { 0.0f, 0.0f, 0.5f, 1.0f };
    
    Device                      device;
    Graphics                    gfx;
    gltf::Model                 m;
    size_t                      triangle_count, line_count;
    Texture                     textures[Asset::count - 1];

    void load_from_gltf(gltf::Model &m, str &part, mx &vertices, array<u32> &tris, gltf::Joints &joints) {
        /// model must have been loaded
        assert(m->nodes);

        /// load single part from gltf (Model calls this multiple times)
        gltf::Node &node = m[part];
        assert(node->mesh >= 0);

        gltf::Mesh &mesh = m->meshes[node->mesh];

        for (gltf::Primitive &prim: mesh->primitives) {
            /// for each attrib
            struct vstride {
                ion::prop            *prop;
                type_t                compound_type;
                gltf::Accessor::M    *accessor;
                gltf::Buffer::M      *buffer;
                gltf::BufferView::M  *buffer_view;
                num                   offset;
            };

            array<vstride> strides { prim->attributes->count() };
            size_t pcount = 0;
            size_t vlen = 0;
            for (field<mx> f: prim->attributes) {
                str       prop_bind      = f.key.hold();
                symbol    prop_sym       = symbol(prop_bind);
                num       accessor_index = num(f.value);
                gltf::Accessor &accessor       = m->accessors[accessor_index];

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
            /// we need to perform conversion on u16 to u32
            for (vstride &stride: strides) {
                u8 *dst        = vbuf;
                u8 *src        = (u8*)&stride.buffer->uri.data[stride.buffer_view->byteOffset];
                num member_sz  = stride.compound_type->base_sz; /// size of member / accessor compound-type

                size_t src_vcount       = stride.accessor->vcount();
                size_t src_component_sz = stride.accessor->component_size();
                size_t src_sz           = src_vcount * src_component_sz;

                if (src_sz != member_sz) {
                    /// supports VEC2 -> MAT16 conversion for u8, s8, u16, s16 -> u32
                    switch (stride.accessor->componentType) {
                        case gltf::ComponentType::BYTE:
                        case gltf::ComponentType::UNSIGNED_BYTE:
                            for (num i = 0; i < vlen; i++) {
                                u32 *member = (u32*)&dst[stride.offset];
                                u8  *bsrc   =  (u8*)&src[src_sz * i];
                                for (int m = 0; m < src_vcount; m++)
                                    member[m] = bsrc[m];
                                dst += gfx->vtype->base_sz;
                            }
                            break;
                        case gltf::ComponentType::SHORT:
                        case gltf::ComponentType::UNSIGNED_SHORT:
                            for (num i = 0; i < vlen; i++) {
                                u32 *member = (u32*)&dst[stride.offset];
                                u16 *bsrc   = (u16*)&src[src_sz * i];
                                for (int m = 0; m < src_vcount; m++)
                                    member[m] = bsrc[m];
                                dst += gfx->vtype->base_sz;
                            }
                            break;
                        default:
                            console.fault("glTF attribute conversion not implemented: {0} -> {1}",
                                { stride.accessor->componentType, str(stride.compound_type->name) });
                            break;
                    }

                } else {
                    /// perform simple copies
                    for (num i = 0; i < vlen; i++) {
                        u8 *member = &dst[stride.offset];
                        u8 *bsrc   = &src[src_sz * i];
                        memcpy(member, bsrc, src_sz);
                        dst += gfx->vtype->base_sz; /// next vert
                    }
                }
            }
            /// create vertex buffer by wrapping what we've copied from allocation (we have a primitive array)
            vertices = mx { memory::wrap(gfx->vtype, vbuf, vlen) }; /// load indices (always store 32bit uint)

            /// indices data = indexing mesh-primitive->indices
            gltf::Accessor &a_indices = m->accessors[prim->indices];
            gltf::BufferView &view = m->bufferViews[ a_indices->bufferView ];
            gltf::Buffer      &buf = m->buffers    [ view->buffer ];
            memory *mem_indices;

            if (a_indices->componentType == gltf::ComponentType::UNSIGNED_SHORT) {
                u16 *u16_window = (u16*)(buf->uri.data + view->byteOffset);
                u32 *u32_alloc  = (u32*)calloc(sizeof(u32), a_indices->count);
                for (int i = 0; i < a_indices->count; i++)
                    u32_alloc[i] = u32(u16_window[i]);
                mem_indices = memory::wrap(typeof(u32), u32_alloc, a_indices->count);
            } else {
                assert(a_indices->componentType == gltf::ComponentType::UNSIGNED_INT);
                u32 *u32_window = (u32*)(buf->uri.data + view->byteOffset);
                mem_indices = memory::window(typeof(u32), u32_window, a_indices->count);
            }
            tris = mem_indices->hold();

            /// load joints for this node (may be default or null state)
            joints = m.joints(node);
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
    void load_bindings(Graphics &gfx, gltf::Joints joints) {
        wgpu::Device device = this->device->wgpu;
        array<wgpu::BindGroupEntry>       bind_values (gfx->bindings.count());
        array<wgpu::BindGroupLayoutEntry> bind_entries(gfx->bindings.count());
        size_t bind_id = 0;

        /// iterate through ShaderVar bindings
        ivars.clear();
        for (ShaderVar& binding: gfx->bindings) {
            wgpu::BindGroupLayoutEntry entry { .binding = u32(bind_id) };
            wgpu::BindGroupEntry bind_value  { .binding = u32(bind_id) };
            type_t btype = binding->type;

            if (btype == typeof(Texture)) {

                binding->tx.on_resize(name, [this, gfx, joints](vec2i sz) mutable {
                    load_bindings(gfx, joints);
                });
                entry.texture.multisampled  = false;
                entry.texture.sampleType    = wgpu::TextureSampleType::Float;
                entry.texture.viewDimension = binding->tx->view_dimension();
                bind_value.textureView      = binding->tx->view;

            } else if (btype == typeof(Sampling)) {

                bool nearest = binding->sampling == Sampling::nearest;
                wgpu::FilterMode filter_mode = nearest ?
                    wgpu::FilterMode::Nearest : wgpu::FilterMode::Linear;
                wgpu::SamplerDescriptor desc = {
                    .magFilter    = filter_mode,
                    .minFilter    = filter_mode,
                    .mipmapFilter = nearest ? wgpu::MipmapFilterMode::Nearest : wgpu::MipmapFilterMode::Linear
                };
                entry.sampler.type = nearest ?
                    wgpu::SamplerBindingType::NonFiltering : wgpu::SamplerBindingType::Filtering;
                bind_value.sampler = device.CreateSampler(&desc);
                
            } else {

                bool      is_uniform   = binding->flags & ShaderVar::Flag::uniform;
                bool      read_only    = binding->flags & ShaderVar::Flag::read_only;

                entry.buffer.type =
                    is_uniform ? wgpu::BufferBindingType::Uniform         :
                    read_only  ? wgpu::BufferBindingType::ReadOnlyStorage : 
                                 wgpu::BufferBindingType::Storage;

                wgpu::BufferDescriptor desc = {
                    .usage = (is_uniform ? wgpu::BufferUsage::Uniform : wgpu::BufferUsage::Storage) |
                        wgpu::BufferUsage::CopyDst,
                    .size  = binding.size()
                };

                bind_value.buffer = device.CreateBuffer(&desc); /// updated with mx given from IRenderable, in IPipeline
                binding.prepare_data();
            }

            ivars += IVar {
                .buffer  = bind_value.buffer,
                .sampler = bind_value.sampler,
                .svar    = binding
            };

            if (binding->flags & ShaderVar::Flag::vertex)   entry.visibility |= wgpu::ShaderStage::Vertex;
            if (binding->flags & ShaderVar::Flag::fragment) entry.visibility |= wgpu::ShaderStage::Fragment;

            bind_entries.push(entry);
            bind_values. push(bind_value);
            bind_id++;
        }

        /// add joints data as binding to shader (Vertex only)
        if (joints.count()) {
            wgpu::BindGroupLayoutEntry entry { .binding = u32(bind_id) };
            wgpu::BindGroupEntry bind_value  { .binding = u32(bind_id) };

            wgpu::BufferDescriptor desc = {
                .usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst,
                .size  = joints.total_size()
            };
            bind_value.buffer = device.CreateBuffer(&desc);
            entry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
            entry.visibility  = wgpu::ShaderStage::Vertex;

            ivars += IVar {
                .buffer  = bind_value.buffer,
                .svar    = ShaderVar(typeof(glm::mat4x4), joints.count(),
                    ShaderVar::Flag::object | ShaderVar::Flag::read_only),
                .instance = [joints]() -> mx {
                    return joints.copy();
                },
                .update = [](mx &mx_joints) -> mx {
                    /// use this instead of var_data
                    /// assert it matches the size provided above
                    gltf::Joints joints(mx_joints.hold());
                    return joints->states;
                }
            };

            bind_entries.push(entry);
            bind_values. push(bind_value);
            bind_id++;
        }

        wgpu::BindGroupLayoutDescriptor ld {
            .entryCount         = size_t(bind_id),
            .entries            = bind_entries.data
        };
        bgl = device.CreateBindGroupLayout(&ld);

        wgpu::BindGroupDescriptor bg {
            .layout             = bgl,
            .entryCount         = size_t(bind_id),
            .entries            = bind_values.data
        };

        bind_group = device.CreateBindGroup(&bg);
        pipeline_layout = dawn::utils::MakeBasicPipelineLayout(device, &bgl);
    }

    /// get meta attributes on the Vertex type (all must be used in shader)
    void create_with_attrs(Graphics &gfx, bool wire) {
        wgpu::Device  device        = this->device->wgpu;
        size_t        attrib_count  = 0;
        doubly<prop> &props         = *(doubly<prop>*)gfx->vtype->meta;

        auto get_wgpu_format = [](prop &p) {
            if (p.type == typeof(glm::vec2))  return wgpu::VertexFormat::Float32x2;
            if (p.type == typeof(glm::vec3))  return wgpu::VertexFormat::Float32x3;
            if (p.type == typeof(glm::vec4))  return wgpu::VertexFormat::Float32x4;
            if (p.type == typeof(glm::ivec4)) return wgpu::VertexFormat::Sint32x4;
            if (p.type == typeof(float))      return wgpu::VertexFormat::Float32;
            if (p.type == typeof(float[4]))   return wgpu::VertexFormat::Float32x4;
          //if (p.type == typeof(u16[4]))     return wgpu::VertexFormat::Uint16x4; # not supported in shader (still used in glTF buffer-view; convert when loading)
            if (p.type == typeof(u32[4]))     return wgpu::VertexFormat::Uint32x4;

            ///
            console.fault("type not found when parsing vertex: {0}", {p.type->name});
            return wgpu::VertexFormat::Undefined;
        };

        dawn::utils::ComboRenderPipelineDescriptor render_desc;
        render_desc.layout = pipeline_layout;

        render_desc.multisample.count = 4;

        if (wire) {
            render_desc.primitive.topology = wgpu::PrimitiveTopology::LineList;
        } else {
            render_desc.primitive.frontFace = wgpu::FrontFace::CCW;
            render_desc.primitive.cullMode = wgpu::CullMode::Back;
            render_desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        }

        render_desc.cFragment.module = mod;
        render_desc.cFragment.entryPoint = wire ? "fragment_wire" : "fragment_main";

        render_desc.vertex.module = mod;
        render_desc.vertex.entryPoint = wire ? "vertex_wire" : "vertex_main";
        render_desc.vertex.bufferCount = 1;
        
        /// todo: stop using this structure, as its confusing
        render_desc.cDepthStencil.depthWriteEnabled = true;

        if (wire)
            render_desc.cDepthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
        else
            render_desc.cDepthStencil.depthCompare = wgpu::CompareFunction::Less;
        
        render_desc.EnableDepthStencil(wgpu::TextureFormat::Depth24PlusStencil8);

        for (prop &p: props) {
            render_desc.cAttributes[attrib_count].shaderLocation = attrib_count;
            render_desc.cAttributes[attrib_count].format         = get_wgpu_format(p);
            render_desc.cAttributes[attrib_count].offset         = p.offset;
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

        render_desc.cBuffers[0].arrayStride = gfx->vtype->base_sz;// sizeof(Attribs);
        render_desc.cBuffers[0].attributeCount = attrib_count;
        render_desc.cTargets[0].format = preferred_swapchain_format();
        render_desc.cTargets[0].blend = &blend; /// needs a way to disable blending

        if (wire)
            pipeline_wire = device.CreateRenderPipeline(&render_desc);
        else
            pipeline = device.CreateRenderPipeline(&render_desc);
        Dawn dawn;
        dawn.process_events();
        debug_break();
    }

    void submit(IRenderable &renderable, wgpu::TextureView &swap_view, wgpu::TextureView &color_view, wgpu::TextureView &depth_stencil, states<Clear> &clear_states, rgbaf &clear_color) {
        /// update ivar buffers where defined
        for (int i = 0; i < ivars.count(); i++) {
            IVar &ivar = ivars[i];
            wgpu::Buffer &b = ivar.buffer;
            if (!b) continue;
            if (ivar.update)
                debug_break();
            
            mx data = ivar.update ? ivar.update(renderable.var_data[i]) : renderable.var_data[i];
            size_t bsz = b.GetSize();
            size_t data_sz = data.total_size();
            assert(b.GetSize() == data.total_size());
            device->queue.WriteBuffer(b, 0, data.origin<void>(), data.total_size());
        }

        wgpu::CommandEncoder encoder = device->wgpu.CreateCommandEncoder();

        auto render_encoder = [&](bool wire) -> wgpu::RenderPassEncoder { /// needs to support a singular wire pass where the buffer is cleared
            wgpu::RenderPassColorAttachment color_attachment {
                .loadOp = (!wire && clear_states[Clear::Color]) ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
                .storeOp = wgpu::StoreOp::Store,
                .clearValue = wgpu::Color { clear_color.r, clear_color.g, clear_color.b, 0.0f }
            };
            if (color_view) {
                color_attachment.view = color_view;
                color_attachment.resolveTarget = swap_view;
            } else {
                color_attachment.view = swap_view;
            }
            wgpu::RenderPassDepthStencilAttachment depth_attachment {
                .view = depth_stencil,
                .depthLoadOp = (!wire && clear_states[Clear::Depth]) ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
                .depthStoreOp = wgpu::StoreOp::Store,
                .depthClearValue = 1.0f,
                .depthReadOnly = false,
                .stencilLoadOp = (!wire && clear_states[Clear::Stencil]) ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
                .stencilStoreOp = wgpu::StoreOp::Store,
                .stencilReadOnly = false
            };
            wgpu::RenderPassDescriptor render_desc {
                .colorAttachmentCount = 1,
                .colorAttachments = &color_attachment,
                .depthStencilAttachment = depth_stencil ? &depth_attachment : null
            };
            return encoder.BeginRenderPass(&render_desc);
        };

        wgpu::Queue queue = device->queue;
        
        wgpu::RenderPassEncoder render = render_encoder(false);
        render.SetPipeline(pipeline);
        render.SetBindGroup(0, bind_group);
        render.SetVertexBuffer(0, vertex_buffer);
        render.SetIndexBuffer(triangle_buffer, wgpu::IndexFormat::Uint32);
        render.DrawIndexed(triangle_count * 3);
        render.End();


        wgpu::RenderPassEncoder wire_render = render_encoder(true);
        wire_render.SetPipeline(pipeline_wire);
        wire_render.SetBindGroup(0, bind_group);
        wire_render.SetVertexBuffer(0, vertex_buffer);
        wire_render.SetIndexBuffer(line_buffer, wgpu::IndexFormat::Uint32);
        wire_render.DrawIndexed(line_count * 2);
        wire_render.End();
        
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
};

struct IModel {
    Device                      device;
    ion::gltf::Model            m;
    symbol                      model;
    array<Pipeline>             pipelines;

    void submit(IRenderable &renderable, wgpu::TextureView swap_view, wgpu::TextureView color_view, wgpu::TextureView depth_stencil_view, states<Clear> clear_states, rgbaf clear_color) {
        for (auto p: pipelines)
            p->submit(renderable, swap_view, color_view, depth_stencil_view, clear_states, clear_color);
    }

    /// create pipeline with shader, textures, vbo/ibo, bindings, attribs, blending and stencil info
    void reload() {
        for (Pipeline &sub: pipelines) {
            Graphics &     gfx    = sub->gfx;
            array<u32>     quads, triangles;
            mx             vertices, bones;
            array<image>   images;
            gltf::Joints   joints;
            int            subdiv = 0;
            if (gfx->gen)
                gfx->gen(vertices, triangles, images); /// generate vbo/ibo based on user routine
            else {
                sub->load_from_gltf(m, gfx->name, vertices, triangles, joints); /// otherwise load from gltf
                subdiv = 0;
            }
            /// sub-divide and tessellate
            Mesh mesh = Mesh::import_vbo(vertices, triangles, true); /// our model is stored as triangles, but it would be nice to parameterize this as soon as glTF quads support works
            while (subdiv-- > 0)
                mesh.catmull_clark();
            mesh.export_vbo(vertices, quads, triangles, true);

            array<u32> lines(quads.count() / 4 * 8);
            u32 *quad_data = quads.origin<u32>();
            for (int i = 0; i < quads.count(); i += 4) {
                lines += quad_data[i + 0];
                lines += quad_data[i + 1];
                lines += quad_data[i + 1];
                lines += quad_data[i + 2];
                lines += quad_data[i + 2];
                lines += quad_data[i + 3];
                lines += quad_data[i + 3];
                lines += quad_data[i + 0];
            }

            sub->triangle_buffer = device->create_buffer(triangles, wgpu::BufferUsage::Index);
            sub->line_buffer     = device->create_buffer(lines,     wgpu::BufferUsage::Index);
            sub->vertex_buffer   = device->create_buffer(vertices,  wgpu::BufferUsage::Vertex);
            sub->triangle_count  = triangles.len() / 3;
            sub->line_count      = lines.len() / 2;
            sub->load_shader(gfx);
            sub->load_bindings(gfx, joints);
            sub->create_with_attrs(gfx, false);
            sub->create_with_attrs(gfx, true);
        }
    }
};

Model::Model(Device &device, symbol model, array<Graphics> select):Model() {
    data->device = device;
    data->model = model;

    if (model) {
        path gltf_path = fmt {"models/{0}.gltf", { model }};
        data->m = gltf::Model::load(gltf_path);
    }

    for (Graphics &gfx: select) {
        Pipeline p;
        p->gfx            = gfx;
        p->device         = device;
        p->name           = gfx->name;
        p->m              = data->m;
        data->pipelines += p;
    }

    data->reload();
}

/// Joints use-case: returns an instance of gltf::Joints
/// that copy is made from a user-defined instancing function provided by the binding function
mx &object_data(IObject *o, str name, type_t type) {
    bool name_found = false;
    for (IRenderable &renderable: o->renderables)
        if (renderable.name == name) {
            name_found = true;
            for (mx &v: renderable.var_data)
                if (v.type() == type)
                    return v;
            break;
        }
    
    if (name_found)
        console.fault("object_data: type not found for sub-object: {0}/{1}", { name, str(type->name) });
    else
        console.fault("object_data: sub-object not found: {0}/{1}", { name, str(type->name) });
    
    static mx null;
    return null;
}

Object Model::instance() {
    Object o;

    size_t n_pipelines = data->pipelines.count();
    o->model = *this;
    o->renderables = array<IRenderable>(n_pipelines);

    for (Pipeline &pl: data->pipelines) {
        IRenderable &renderable = o->renderables.push();
        renderable.name = pl->name;
        for (IVar &ivar: pl->ivars) {
            if (ivar.buffer) {
                size_t buffer_sz = ivar.buffer.GetSize();
                assert(buffer_sz == ivar.svar.size());
            }
            /// instance and update can be stored on ShaderVar but i think it may be too much for it
            mx m = ivar.instance ? ivar.instance() : ivar.svar.alloc();
            renderable.var_data.push(m);
        }
    }
    return o;
}

/// we need a way for the user to obtain typed data reference

Model::operator Object() {
    return instance();
}

Pipeline &Model::operator[](str s) {
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
    Texture                     color;

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

    void update_attachments() {
        depth_stencil->create(dawn->device, sz.x, sz.y, 1, 4,
            Asset::depth_stencil,
            wgpu::TextureUsage::RenderAttachment);
        
        color->create(dawn->device, sz.x, sz.y, 1, 4,
            Asset::multisample,
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
        update_attachments();
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
        for (Presentation p: presentations) {
            Scene scene = p.on_present();
            for (Object &o: scene) { /// this will change to Object/Entity/Visual iteration
                int ipipeline = 0;
                assert(o->renderables.count() == o->model->pipelines.count());
                for (IRenderable &renderable: o->renderables) {
                    Pipeline &pl = o->model->pipelines[ipipeline];
                    pl->submit(renderable, swap_view, color->view, depth_stencil->view, clear_states, pl->clear_color);
                    clear_states[Clear::Color] = false;
                    clear_states[Clear::Depth] = false;
                    clear_states[Clear::Stencil] = false;
                    ipipeline++;
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
};

float Window::aspect() {
    return float(data->sz.x) / float(data->sz.y);
}

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
mx_implement(Model,     mx, IModel);
mx_implement(Object,    mx, IObject);
mx_implement(Window,    mx, IWindow);

}
