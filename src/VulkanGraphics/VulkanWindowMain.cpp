#include "VulkanGraphics/VulkanGraphics.h"

#include <chrono>
#include <thread>

int main() {
    VulkanGraphics app;
    app.Show();

    while (!app.ShouldClose()) {
        glfwPollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    app.Close();
    return 0;
}
