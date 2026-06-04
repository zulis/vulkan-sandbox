#include "App.h"

App::App() : BaseApp("Simple window")
{
}

void App::update(float deltaTime)
{
}

void App::draw()
{
    // renderer->beginFrame() already clears the screen.
    // Use renderer (RendererVulkan) to access Vulkan objects
    // for pipeline creation, command recording, etc.
    //
    // Example:
    //   VkCommandBuffer cmd = renderer->beginFrame(); // already called by BaseApp
    //   VkDevice device = renderer->getDevice();
    //   VkRenderPass rp = renderer->getRenderPass();
    //   ... record draw commands ...
}

void App::drawUI()
{
    static bool show_demo_window = true;
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::ShowDemoWindow(&show_demo_window);

    if (ImGui::Begin("FPS Monitor")) {
        float fps = ImGui::GetIO().Framerate;
        
        // Display FPS
        ImGui::Text("FPS: %.1f", fps);
        
        // Optional: Display frame time in milliseconds
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / (fps > 0 ? fps : 1));
    }
    ImGui::End();
}

void App::onResize(int width, int height)
{
    (void)width;
    (void)height;
    // Swapchain recreation is handled by renderer->handleResize()
    // which is called by BaseApp's resizeEvent callback.
}
