#include <RendererVulkan.h>
#include <Window.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <cstdint>

namespace library
{

// ---------------------------------------------------------------------------
// File-local Vulkan helpers
// ---------------------------------------------------------------------------
namespace
{
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;
        bool IsComplete() const { return graphics.has_value() && present.has_value(); }
    };

    static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        QueueFamilyIndices indices;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

        for (uint32_t i = 0; i < count; ++i)
        {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.graphics = i;

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport)
                indices.present = i;

            if (indices.IsComplete()) break;
        }
        return indices;
    }

    static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available)
    {
        for (const auto& fmt : available)
            if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
                fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return fmt;
        return available[0];
    }

    static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& available)
    {
        for (const auto& mode : available)
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return mode;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, SDL_Window* w)
    {
        if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
        int width, height;
        SDL_GetWindowSizeInPixels(w, &width, &height);
        return {
            std::clamp(static_cast<uint32_t>(width),  caps.minImageExtent.width,  caps.maxImageExtent.width),
            std::clamp(static_cast<uint32_t>(height), caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }
} // anonymous namespace

// ===================================================================
// RendererVulkanImpl — concrete Vulkan renderer
// ===================================================================
class RendererVulkanImpl : public RendererVulkan
{
public:
    explicit RendererVulkanImpl(Window* window)
        : m_window(window)
    {
        initVulkan();
        createSwapchain();
        createSyncObjects();

        std::cout << "Vulkan initialized successfully." << std::endl;
    }

    ~RendererVulkanImpl() override
    {
        cleanupVulkan();
    }

    // -------------------------------------------------------------------
    // RendererVulkan interface
    // -------------------------------------------------------------------
    VkCommandBuffer beginFrame() override
    {
        // Handle pending resize before acquiring
        if (m_framebufferResized)
        {
            recreateSwapchain();
            m_framebufferResized = false;
        }

        // Surface lost or swapchain invalid — wait for a valid resize event
        if (m_surfaceLost || m_swapchain == VK_NULL_HANDLE)
        {
            // Still poll the fence so we don't deadlock if something was submitted
            vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, 0);
            return VK_NULL_HANDLE;
        }

        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

        VkResult result = vkAcquireNextImageKHR(
            m_device, m_swapchain, UINT64_MAX,
            m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &m_currentImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapchain();
            return VK_NULL_HANDLE;
        }

        if (result == VK_ERROR_SURFACE_LOST_KHR)
        {
            m_surfaceLost = true;
            return VK_NULL_HANDLE;
        }

        VkCommandBuffer cmd = m_commandBuffers[m_currentImageIndex];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkClearValue clearValue{};
        clearValue.color = { { 0.39f, 0.58f, 0.93f, 1.0f } };

        VkRenderPassBeginInfo rpbi{};
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = m_renderPass;
        rpbi.framebuffer = m_swapchainFramebuffers[m_currentImageIndex];
        rpbi.renderArea.extent = m_swapchainExtent;
        rpbi.clearValueCount = 1;
        rpbi.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        return cmd;
    }

    void endFrame() override
    {
        VkCommandBuffer cmd = m_commandBuffers[m_currentImageIndex];
        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_imageAvailable[m_currentFrame];
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_renderFinished[m_currentFrame];

        vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_renderFinished[m_currentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_swapchain;
        presentInfo.pImageIndices = &m_currentImageIndex;

        VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            m_framebufferResized = true;
        else if (result == VK_ERROR_SURFACE_LOST_KHR)
            m_surfaceLost = true;

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // -------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------
    VkInstance       getInstance() const override       { return m_instance; }
    VkPhysicalDevice getPhysicalDevice() const override { return m_physicalDevice; }
    VkDevice         getDevice() const override         { return m_device; }
    VkRenderPass     getRenderPass() const override     { return m_renderPass; }
    VkExtent2D       getExtent() const override         { return m_swapchainExtent; }
    VkFormat         getFormat() const override         { return m_swapchainFormat; }

    // --- Called by Window on resize ---
    void handleResize() override
    {
        m_framebufferResized = true;
    }

private:
    // -------------------------------------------------------------------
    // Init
    // -------------------------------------------------------------------
    void initVulkan()
    {
        SDL_Window* sdlWindow = m_window->getSDLWindow();

        if (volkInitialize() != VK_SUCCESS)
        {
            std::cerr << "Failed to initialize Volk" << std::endl;
            return;
        }

        Uint32 extCount = 0;
        const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
        std::vector<const char*> extensions(sdlExts, sdlExts + extCount);
#ifdef _DEBUG
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan Sandbox";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ici.pApplicationInfo = &appInfo;
        ici.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        ici.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&ici, nullptr, &m_instance) != VK_SUCCESS)
        {
            std::cerr << "Failed to create Vulkan instance" << std::endl;
            return;
        }
        volkLoadInstance(m_instance);

        if (!SDL_Vulkan_CreateSurface(sdlWindow, m_instance, nullptr, &m_surface))
        {
            std::cerr << "Failed to create surface: " << SDL_GetError() << std::endl;
            return;
        }

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        QueueFamilyIndices qf;
        for (auto pd : devices)
        {
            qf = FindQueueFamilies(pd, m_surface);
            if (qf.IsComplete()) { m_physicalDevice = pd; break; }
        }
        if (!m_physicalDevice) { std::cerr << "No suitable GPU" << std::endl; return; }

        std::set<uint32_t> unique = { qf.graphics.value(), qf.present.value() };
        std::vector<VkDeviceQueueCreateInfo> qcis;
        float priority = 1.0f;
        for (auto fam : unique)
        {
            VkDeviceQueueCreateInfo qci{};
            qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qci.queueFamilyIndex = fam;
            qci.queueCount = 1;
            qci.pQueuePriorities = &priority;
            qcis.push_back(qci);
        }

        std::vector<const char*> deviceExts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkPhysicalDeviceFeatures features{};

        VkDeviceCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
        dci.pQueueCreateInfos = qcis.data();
        dci.enabledExtensionCount = static_cast<uint32_t>(deviceExts.size());
        dci.ppEnabledExtensionNames = deviceExts.data();
        dci.pEnabledFeatures = &features;

        if (vkCreateDevice(m_physicalDevice, &dci, nullptr, &m_device) != VK_SUCCESS)
        {
            std::cerr << "Failed to create logical device" << std::endl;
            return;
        }
        volkLoadDevice(m_device);

        vkGetDeviceQueue(m_device, qf.graphics.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, qf.present.value(), 0, &m_presentQueue);
    }

    // -------------------------------------------------------------------
    // Swapchain
    // -------------------------------------------------------------------
    void createSwapchain()
    {
        SDL_Window* sdlWindow = m_window->getSDLWindow();

        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);

        uint32_t fmtCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, formats.data());

        uint32_t presentCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentCount, presentModes.data());

        auto surfaceFormat = ChooseSurfaceFormat(formats);
        auto presentMode = ChoosePresentMode(presentModes);
        m_swapchainExtent = ChooseExtent(caps, sdlWindow);
        m_swapchainFormat = surfaceFormat.format;

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        VkSwapchainCreateInfoKHR sci{};
        sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sci.surface = m_surface;
        sci.minImageCount = imageCount;
        sci.imageFormat = surfaceFormat.format;
        sci.imageColorSpace = surfaceFormat.colorSpace;
        sci.imageExtent = m_swapchainExtent;
        sci.imageArrayLayers = 1;
        sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        sci.preTransform = caps.currentTransform;
        sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode = presentMode;
        sci.clipped = VK_TRUE;

        auto qf = FindQueueFamilies(m_physicalDevice, m_surface);
        uint32_t qfIdx[] = { qf.graphics.value(), qf.present.value() };
        if (qf.graphics != qf.present)
        {
            sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            sci.queueFamilyIndexCount = 2;
            sci.pQueueFamilyIndices = qfIdx;
        }
        else
        {
            sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        VkResult scResult = vkCreateSwapchainKHR(m_device, &sci, nullptr, &m_swapchain);
        if (scResult != VK_SUCCESS || m_swapchain == VK_NULL_HANDLE)
        {
            std::cerr << "Failed to create swapchain: " << scResult << std::endl;
            m_swapchain = VK_NULL_HANDLE;
            return;
        }

        uint32_t imgCount = 0;
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &imgCount, nullptr);
        m_swapchainImages.resize(imgCount);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &imgCount, m_swapchainImages.data());

        m_swapchainImageViews.resize(imgCount);
        for (uint32_t i = 0; i < imgCount; ++i)
        {
            VkImageViewCreateInfo ivci{};
            ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivci.image = m_swapchainImages[i];
            ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivci.format = m_swapchainFormat;
            ivci.components = { VK_COMPONENT_SWIZZLE_IDENTITY };
            ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ivci.subresourceRange.levelCount = 1;
            ivci.subresourceRange.layerCount = 1;
            vkCreateImageView(m_device, &ivci, nullptr, &m_swapchainImageViews[i]);
        }

        VkAttachmentDescription colorAtt{};
        colorAtt.format = m_swapchainFormat;
        colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpci{};
        rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpci.attachmentCount = 1;
        rpci.pAttachments = &colorAtt;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass;
        rpci.dependencyCount = 1;
        rpci.pDependencies = &dep;
        vkCreateRenderPass(m_device, &rpci, nullptr, &m_renderPass);

        m_swapchainFramebuffers.resize(imgCount);
        for (uint32_t i = 0; i < imgCount; ++i)
        {
            VkFramebufferCreateInfo fci{};
            fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fci.renderPass = m_renderPass;
            fci.attachmentCount = 1;
            fci.pAttachments = &m_swapchainImageViews[i];
            fci.width = m_swapchainExtent.width;
            fci.height = m_swapchainExtent.height;
            fci.layers = 1;
            vkCreateFramebuffer(m_device, &fci, nullptr, &m_swapchainFramebuffers[i]);
        }

        VkCommandPoolCreateInfo cpci{};
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = qf.graphics.value();
        vkCreateCommandPool(m_device, &cpci, nullptr, &m_commandPool);

        m_commandBuffers.resize(imgCount);
        VkCommandBufferAllocateInfo cbai{};
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = m_commandPool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = imgCount;
        vkAllocateCommandBuffers(m_device, &cbai, m_commandBuffers.data());
    }

    void destroySwapchain()
    {
        vkDeviceWaitIdle(m_device);
        if (m_commandPool) { vkDestroyCommandPool(m_device, m_commandPool, nullptr); m_commandPool = VK_NULL_HANDLE; }
        for (auto fb : m_swapchainFramebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
        m_swapchainFramebuffers.clear();
        if (m_renderPass) { vkDestroyRenderPass(m_device, m_renderPass, nullptr); m_renderPass = VK_NULL_HANDLE; }
        for (auto iv : m_swapchainImageViews) vkDestroyImageView(m_device, iv, nullptr);
        m_swapchainImageViews.clear();
        if (m_swapchain) { vkDestroySwapchainKHR(m_device, m_swapchain, nullptr); m_swapchain = VK_NULL_HANDLE; }
    }

    void recreateSwapchain()
    {
        destroySwapchain();
        createSwapchain();
        if (m_swapchain != VK_NULL_HANDLE)
            m_surfaceLost = false;
    }

    // -------------------------------------------------------------------
    // Sync
    // -------------------------------------------------------------------
    void createSyncObjects()
    {
        m_imageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
        m_renderFinished.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semCI{};
        semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkCreateSemaphore(m_device, &semCI, nullptr, &m_imageAvailable[i]);
            vkCreateSemaphore(m_device, &semCI, nullptr, &m_renderFinished[i]);
            vkCreateFence(m_device, &fenceCI, nullptr, &m_inFlightFences[i]);
        }
    }

    // -------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------
    void cleanupVulkan()
    {
        if (m_device)
        {
            vkDeviceWaitIdle(m_device);
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
            {
                if (m_imageAvailable[i]) vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
                if (m_renderFinished[i]) vkDestroySemaphore(m_device, m_renderFinished[i], nullptr);
                if (m_inFlightFences[i]) vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
            }
            destroySwapchain();
        }
        if (m_surface)  { vkDestroySurfaceKHR(m_instance, m_surface, nullptr); m_surface = VK_NULL_HANDLE; }
        if (m_device)   { vkDestroyDevice(m_device, nullptr); m_device = VK_NULL_HANDLE; }
        if (m_instance) { vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; }
    }

    // -------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    Window* m_window = nullptr;

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
    bool m_surfaceLost = false;
};

// ===================================================================
// Factory
// ===================================================================
RendererVulkan* RendererVulkan::create(Window* window)
{
    return new RendererVulkanImpl(window);
}

} // namespace library
