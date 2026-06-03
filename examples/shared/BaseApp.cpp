#include "BaseApp.h"

#include <chrono>

using namespace library;

BaseApp::BaseApp(const char* title)
{
    window = new Window();
    window->setTitle(title);

    renderer = RendererVulkan::create(window);

    window->closeEvent = [this] { quit(); };
    window->resizeEvent = [this](int w, int h) {
        onResize(w, h);
        if (renderer) renderer->handleResize();
    };
}

BaseApp::~BaseApp()
{
    delete renderer;
    delete window;
    renderer = nullptr;
    window = nullptr;
}

void BaseApp::run()
{
    m_running = true;
    onResize(window->getWidth(), window->getHeight());

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_running)
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        if (!window->pollEvents())
        {
            m_running = false;
            break;
        }

        update(deltaTime);

        VkCommandBuffer cmd = renderer->beginFrame();
        if (cmd != VK_NULL_HANDLE)
        {
            draw();
            drawUI();
            renderer->endFrame();
        }
    }
}

void BaseApp::quit()
{
    m_running = false;
}
