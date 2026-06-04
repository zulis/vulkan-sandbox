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
    window->eventCallback = [](const SDL_Event* e) {
        ImGui_ImplSDL3_ProcessEvent(e);
    };

    // --- Initialize ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // SDL3 backend
    ImGui_ImplSDL3_InitForVulkan(window->getSDLWindow());

    // Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = renderer->getInstance();
    initInfo.PhysicalDevice = renderer->getPhysicalDevice();
    initInfo.Device = renderer->getDevice();
    initInfo.QueueFamily = renderer->getGraphicsQueueFamily();
    initInfo.Queue = renderer->getGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE
                                + IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.PipelineInfoMain.RenderPass = renderer->getRenderPass();
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}

BaseApp::~BaseApp()
{
    // Shutdown ImGui before destroying the Vulkan device
    if (renderer && renderer->getDevice() != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(renderer->getDevice());
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
    }
    ImGui::DestroyContext();

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

        // --- ImGui frame start ---
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        drawUI();               // Build ImGui widgets

        ImGui::Render();        // Finalize draw data

        // --- Vulkan frame ---
        VkCommandBuffer cmd = renderer->beginFrame();
        if (cmd != VK_NULL_HANDLE)
        {
            draw();                                               // User draw calls
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd); // Render ImGui
            renderer->endFrame();
        }
    }
}

void BaseApp::quit()
{
    m_running = false;
}
