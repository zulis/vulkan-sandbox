#pragma once

#include <volk.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <Window.h>
#include <RendererVulkan.h>

class BaseApp
{
public:
    BaseApp(const char* title);
    virtual ~BaseApp();

    void run();
    void quit();

    virtual void update(float deltaTime) = 0;
    virtual void draw() = 0;
    virtual void onResize(int width, int height) {}
    virtual void drawUI() = 0;

    library::Window*          window = nullptr;
    library::RendererVulkan*  renderer = nullptr;

private:
    bool m_running = false;
};
