#include <vk/vk.hpp>

using namespace ion;

/// standard viking arguments
const uint32_t    WIDTH         = 800;
const uint32_t    HEIGHT        = 600;
const symbol      MODEL_NAME    = "flower22";

/// Vertex need not be an mx-based class, however it does register its meta props, type and function table (register(Vertex))
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;

    Vertex() { }
    Vertex(float *vpos, int p_index, float *uv, int uv_index, float *npos, int n_index) {
        pos      = { vpos[3 * p_index  + 0], vpos[3 * p_index + 1], vpos[3 * p_index + 2] };
        texCoord = {   uv[2 * uv_index + 0], 1.0f - uv[2 * uv_index + 1] };
        color    = { 1.0f, 1.0f, 1.0f };
        normal   = { npos[3 * n_index  + 0], npos[3 * n_index + 1], npos[3 * n_index + 2] };
    }

    /// these properties can be anywhere inside the space.  its all just offset computed on a single instance when generating info.  only ran once
    doubly<prop> meta() const {
        return {
            prop { "pos",      pos      },
            prop { "color",    color    },
            prop { "texCoord", texCoord },
            prop { "normal",   normal   }
        };
    }

    operator bool() { return pos.length() > 0; } /// needs to be optional, but the design-time check doesnt seem to work

    type_register(Vertex);

    bool operator==(const Vertex& b) const {
        return pos == b.pos && color == b.color && texCoord == b.texCoord;
    }
};

/// still using std hash and std unordered_map
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

/// uniform has an update method with a pipeline arg
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 light_dir;
    alignas(16) glm::vec3 light_rgb;

    void update(void *data) {
        VkExtent2D &ext = ((Pipeline::impl*)data)->device->swapChainExtent;

        static auto startTime   = std::chrono::high_resolution_clock::now();
        auto        currentTime = std::chrono::high_resolution_clock::now();
        float       time        = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        
        model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f) * 0.1f, glm::vec3(0.0f, 0.0f, 1.0f));
        view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        proj  = glm::perspective(glm::radians(22.0f), ext.width / (float) ext.height, 0.1f, 10.0f);
        proj[1][1] *= -1;

        light_dir = glm::normalize(glm::vec3(1.0f, -1.0f, -1.0f));
        light_rgb = glm::vec3(1.0, 1.0, 1.0);
    }
};

struct HelloTriangleApplication:mx {
    struct impl {
        Vulkan    vk { 1, 0 }; /// this lazy loads 1.0 when GPU performs that action [singleton data]
        vec2i     sz;          /// store current window size
        Window    gpu;         /// GPU class, responsible for holding onto GPU, Surface and GLFWwindow
        Device    device;      /// Device created with GPU
        
        Pipeline pipeline; /// pipeline for single object scene

        static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
            auto *i = (HelloTriangleApplication::impl*)(glfwGetWindowUserPointer(window));
            i->sz = vec2i { width, height };
            i->device->framebufferResized = true;
        }

        /// initialize with size
        void init(vec2i &sz) {
            gpu = Window::select(sz);
            glfwSetWindowUserPointer(gpu->window, this);
            glfwSetFramebufferSizeCallback(gpu->window, framebufferResizeCallback);
            device = Device::create(gpu);
            pipeline = Pipeline(GraphicsDevice<UniformBufferObject, Vertex>(&device), "disney", MODEL_NAME);
        }

        /// run app (main loop, wait for idle)
        void run() {
            while (!glfwWindowShouldClose(gpu->window)) {
                glfwPollEvents();
                device->drawFrame(pipeline);
            }
            vkDeviceWaitIdle(device);
        }

        operator bool() { return sz.x > 0; }
        type_register(impl);
    };
    
    mx_object(HelloTriangleApplication, mx, impl);

    HelloTriangleApplication(vec2i sz):HelloTriangleApplication() {
        data->init(sz);
    }

    /// return the class in main() to loop and return exit-code
    operator int() {
        try {
            data->run();
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
};

int main() {
    return HelloTriangleApplication({ WIDTH, HEIGHT });
}