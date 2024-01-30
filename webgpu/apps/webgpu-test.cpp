#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>

wgpu::Instance instance;

const uint32_t kWidth = 512;
const uint32_t kHeight = 512;

void Start() {
    if (!glfwInit()) {
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window =
        glfwCreateWindow(kWidth, kHeight, "WebGPU window", nullptr, nullptr);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    // TODO: Render a triangle using WebGPU.
    }
}

int main() {
    instance = wgpu::CreateInstance();
    Start();
}