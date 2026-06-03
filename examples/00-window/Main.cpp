#include "../shared/BaseApp.h"

using namespace library;

// ---------------------------------------------------------------------------
// App — thin layer over BaseApp; Window handles all Vulkan + SDL3
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
        // Window::beginFrame() already clears the screen.
        // Add draw calls here once pipelines/geometry are set up.
    }

    void drawUI() override
    {
    }

    void onResize(int width, int height) override
    {
        // Swapchain recreation is handled by Window::pollEvents()
        (void)width;
        (void)height;
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
