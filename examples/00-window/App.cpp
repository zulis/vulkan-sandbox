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
}

void App::onResize(int width, int height)
{
    (void)width;
    (void)height;
    // Swapchain recreation is handled by renderer->handleResize()
    // which is called by BaseApp's resizeEvent callback.
}
