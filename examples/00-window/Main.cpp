#include "../shared/BaseApp.h"

using namespace library;

// ---------------------------------------------------------------------------
// App — thin layer over BaseApp
// ---------------------------------------------------------------------------
class App : public BaseApp
{
public:
    App()
        : BaseApp("SDL3 + Vulkan + Volk")
    {
    }

    void update(float /*deltaTime*/) override
    {
    }

    void draw() override
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

    void drawUI() override
    {
    }

    void onResize(int width, int height) override
    {
        (void)width;
        (void)height;
        // Swapchain recreation is handled by renderer->handleResize()
        // which is called by BaseApp's resizeEvent callback.
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
    App app;
    app.run();
    return 0;
}
