#pragma once

#include <functional>
#include <vector>
#include <volk.h>

struct SDL_Window;

namespace library
{

class Window
{
public:
    Window();
    ~Window();

    // --- Window management ---
    void setTitle(const char* title);
    bool pollEvents();

    int getWidth() const;
    int getHeight() const;

    // --- SDL interop ---
    SDL_Window* getSDLWindow() const;

    // --- Vulkan accessors ---
    VkInstance       getInstance() const;
    VkPhysicalDevice getPhysicalDevice() const;
    VkDevice         getDevice() const;
    VkRenderPass     getRenderPass() const;
    VkExtent2D       getSwapchainExtent() const;
    VkFormat         getSwapchainFormat() const;

    // --- Frame management ---
    VkCommandBuffer beginFrame();
    void            endFrame();

    // --- Callbacks ---
    std::function<void()>             closeEvent;
    std::function<void(int, int)>     resizeEvent;

private:
    void initSDL();
    void initVulkan();
    void createSwapchain();
    void destroySwapchain();
    void recreateSwapchain();
    void createSyncObjects();
    void cleanupVulkan();
    void cleanupSDL();

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    SDL_Window*      m_sdlWindow = nullptr;

    VkInstance        m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR      m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice  m_physicalDevice = VK_NULL_HANDLE;
    VkDevice          m_device = VK_NULL_HANDLE;
    VkQueue           m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue           m_presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR    m_swapchain = VK_NULL_HANDLE;
    VkFormat          m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D        m_swapchainExtent = {0, 0};
    std::vector<VkImage>       m_swapchainImages;
    std::vector<VkImageView>   m_swapchainImageViews;
    std::vector<VkFramebuffer> m_swapchainFramebuffers;

    VkRenderPass      m_renderPass = VK_NULL_HANDLE;
    VkCommandPool     m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailable;
    std::vector<VkSemaphore> m_renderFinished;
    std::vector<VkFence>     m_inFlightFences;
    int m_currentFrame = 0;
    uint32_t m_currentImageIndex = 0;
    bool m_framebufferResized = false;

    int m_width = 1280;
    int m_height = 720;
};

} // namespace library
