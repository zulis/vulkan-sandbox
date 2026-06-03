#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <cstdint>

#include <volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static VkSurfaceFormatKHR ChooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& available)
{
    for (const auto& fmt : available)
    {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return fmt;
    }
    return available[0];
}

static VkPresentModeKHR ChoosePresentMode(
    const std::vector<VkPresentModeKHR>& available)
{
    for (const auto& mode : available)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR; // always available
}

static VkExtent2D ChooseExtent(
    const VkSurfaceCapabilitiesKHR& caps,
    SDL_Window* window)
{
    if (caps.currentExtent.width != UINT32_MAX)
        return caps.currentExtent;

    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    VkExtent2D actual = {
        static_cast<uint32_t>(w),
        static_cast<uint32_t>(h)
    };
    actual.width  = std::clamp(actual.width,
                               caps.minImageExtent.width,
                               caps.maxImageExtent.width);
    actual.height = std::clamp(actual.height,
                               caps.minImageExtent.height,
                               caps.maxImageExtent.height);
    return actual;
}

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool IsComplete() const { return graphics.has_value() && present.has_value(); }
};

static QueueFamilyIndices FindQueueFamilies(
    VkPhysicalDevice device,
    VkSurfaceKHR surface)
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

        if (indices.IsComplete())
            break;
    }
    return indices;
}

// ---------------------------------------------------------------------------
// Swapchain wrapper — groups all swapchain-dependent objects so we can
// recreate them on resize.
// ---------------------------------------------------------------------------
struct SwapchainResources
{
    VkSwapchainKHR           swapchain = VK_NULL_HANDLE;
    VkFormat                 format = VK_FORMAT_UNDEFINED;
    VkExtent2D               extent = {0, 0};
    std::vector<VkImage>     images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    VkRenderPass             renderPass = VK_NULL_HANDLE;
    VkCommandPool            commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    void Destroy(VkDevice device)
    {
        if (commandPool)   vkDestroyCommandPool(device, commandPool, nullptr);
        for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        if (renderPass)    vkDestroyRenderPass(device, renderPass, nullptr);
        for (auto iv : imageViews)   vkDestroyImageView(device, iv, nullptr);
        if (swapchain)     vkDestroySwapchainKHR(device, swapchain, nullptr);
        *this = {};
    }
};

static bool CreateSwapchainResources(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkSurfaceKHR surface,
    SDL_Window* window,
    SwapchainResources& res)
{
    // --- surface capabilities ---
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
    VkPresentModeKHR   presentMode   = ChoosePresentMode(presentModes);
    VkExtent2D         extent        = ChooseExtent(caps, window);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    res.format = surfaceFormat.format;
    res.extent = extent;

    // --- swapchain ---
    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = surface;
    sci.minImageCount    = imageCount;
    sci.imageFormat      = surfaceFormat.format;
    sci.imageColorSpace  = surfaceFormat.colorSpace;
    sci.imageExtent      = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.preTransform     = caps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = presentMode;
    sci.clipped          = VK_TRUE;
    sci.oldSwapchain     = VK_NULL_HANDLE;

    QueueFamilyIndices qf = FindQueueFamilies(physicalDevice, surface);
    uint32_t qfIndices[] = { qf.graphics.value(), qf.present.value() };

    if (qf.graphics != qf.present)
    {
        sci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices   = qfIndices;
    }
    else
    {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(device, &sci, nullptr, &res.swapchain) != VK_SUCCESS)
    {
        std::cerr << "Failed to create swapchain" << std::endl;
        return false;
    }

    // --- images & views ---
    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(device, res.swapchain, &imgCount, nullptr);
    res.images.resize(imgCount);
    vkGetSwapchainImagesKHR(device, res.swapchain, &imgCount, res.images.data());

    res.imageViews.resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i)
    {
        VkImageViewCreateInfo ivci{};
        ivci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image    = res.images[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format   = res.format;
        ivci.components = { VK_COMPONENT_SWIZZLE_IDENTITY };
        ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel   = 0;
        ivci.subresourceRange.levelCount     = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device, &ivci, nullptr, &res.imageViews[i]) != VK_SUCCESS)
        {
            std::cerr << "Failed to create image view " << i << std::endl;
            return false;
        }
    }

    // --- render pass ---
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = res.format;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &colorAttachment;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    if (vkCreateRenderPass(device, &rpci, nullptr, &res.renderPass) != VK_SUCCESS)
    {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }

    // --- framebuffers ---
    res.framebuffers.resize(imgCount);
    for (uint32_t i = 0; i < imgCount; ++i)
    {
        VkFramebufferCreateInfo fci{};
        fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass      = res.renderPass;
        fci.attachmentCount = 1;
        fci.pAttachments    = &res.imageViews[i];
        fci.width           = extent.width;
        fci.height          = extent.height;
        fci.layers          = 1;

        if (vkCreateFramebuffer(device, &fci, nullptr, &res.framebuffers[i]) != VK_SUCCESS)
        {
            std::cerr << "Failed to create framebuffer " << i << std::endl;
            return false;
        }
    }

    // --- command pool & buffers ---
    VkCommandPoolCreateInfo cpci{};
    cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = qf.graphics.value();

    if (vkCreateCommandPool(device, &cpci, nullptr, &res.commandPool) != VK_SUCCESS)
    {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }

    res.commandBuffers.resize(imgCount);
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool        = res.commandPool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = imgCount;

    if (vkAllocateCommandBuffers(device, &cbai, res.commandBuffers.data()) != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate command buffers" << std::endl;
        return false;
    }

    // --- record command buffers ---
    VkClearValue clearValue{};
    clearValue.color = { { 0.39f, 0.58f, 0.93f, 1.0f } };

    for (uint32_t i = 0; i < imgCount; ++i)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(res.commandBuffers[i], &beginInfo);

        VkRenderPassBeginInfo rpbi{};
        rpbi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass        = res.renderPass;
        rpbi.framebuffer       = res.framebuffers[i];
        rpbi.renderArea.extent = extent;
        rpbi.clearValueCount   = 1;
        rpbi.pClearValues      = &clearValue;

        vkCmdBeginRenderPass(res.commandBuffers[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdEndRenderPass(res.commandBuffers[i]);

        vkEndCommandBuffer(res.commandBuffers[i]);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main()
{
    // --- SDL ---
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "SDL3 + Vulkan + Volk",
        1280, 720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // --- Volk ---
    if (volkInitialize() != VK_SUCCESS)
    {
        std::cerr << "Failed to initialize Volk" << std::endl;
        return -1;
    }

    // --- Instance ---
    Uint32 extCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!sdlExts)
    {
        std::cerr << "Failed to get extensions: " << SDL_GetError() << std::endl;
        return -1;
    }
    std::vector<const char*> extensions(sdlExts, sdlExts + extCount);
#ifdef _DEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "SDL3 Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "Custom Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo ici{};
    ici.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo        = &appInfo;
    ici.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ici.ppEnabledExtensionNames = extensions.data();

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS)
    {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return -1;
    }
    volkLoadInstance(instance);

    // --- Surface ---
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
    {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
        return -1;
    }

    // --- Physical device ---
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        std::cerr << "No Vulkan-capable GPU found" << std::endl;
        return -1;
    }
    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices qfIndices;
    for (auto pd : physicalDevices)
    {
        qfIndices = FindQueueFamilies(pd, surface);
        if (qfIndices.IsComplete())
        {
            physicalDevice = pd;
            break;
        }
    }
    if (!physicalDevice)
    {
        std::cerr << "No suitable physical device" << std::endl;
        return -1;
    }

    // --- Logical device ---
    std::set<uint32_t> uniqueFamilies = {
        qfIndices.graphics.value(),
        qfIndices.present.value()
    };
    std::vector<VkDeviceQueueCreateInfo> qcis;

    float priority = 1.0f;
    for (auto fam : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &priority;
        qcis.push_back(qci);
    }

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = static_cast<uint32_t>(qcis.size());
    dci.pQueueCreateInfos       = qcis.data();
    dci.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    dci.ppEnabledExtensionNames = deviceExtensions.data();
    dci.pEnabledFeatures        = &deviceFeatures;

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS)
    {
        std::cerr << "Failed to create logical device" << std::endl;
        return -1;
    }
    volkLoadDevice(device);

    VkQueue graphicsQueue, presentQueue;
    vkGetDeviceQueue(device, qfIndices.graphics.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, qfIndices.present.value(),  0, &presentQueue);

    // --- Swapchain ---
    SwapchainResources sc{};
    if (!CreateSwapchainResources(physicalDevice, device, surface, window, sc))
        return -1;

    std::cout << "Vulkan initialized successfully." << std::endl;

    // --- Synchronization ---
    const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> imageAvailable(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkSemaphore> renderFinished(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkFence>     inFlightFences(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semCI{};
    semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkCreateSemaphore(device, &semCI, nullptr, &imageAvailable[i]);
        vkCreateSemaphore(device, &semCI, nullptr, &renderFinished[i]);
        vkCreateFence(device, &fenceCI, nullptr, &inFlightFences[i]);
    }

    // --- Main loop ---
    int  currentFrame = 0;
    bool running      = true;
    bool framebufferResized = false;

    while (running)
    {
        // --- Events ---
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_ESCAPE)
                    running = false;
            }
            else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                framebufferResized = true;
            }
        }

        if (framebufferResized)
        {
            // Recreate swapchain for the new size
            vkDeviceWaitIdle(device);
            sc.Destroy(device);
            if (!CreateSwapchainResources(physicalDevice, device, surface, window, sc))
                break;
            framebufferResized = false;
        }

        // --- Draw frame ---
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        uint32_t imageIndex = 0;
        VkResult acquireResult = vkAcquireNextImageKHR(
            device, sc.swapchain, UINT64_MAX,
            imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR ||
            acquireResult == VK_SUBOPTIMAL_KHR)
        {
            vkDeviceWaitIdle(device);
            sc.Destroy(device);
            if (!CreateSwapchainResources(physicalDevice, device, surface, window, sc))
                break;
            continue;
        }
        else if (acquireResult != VK_SUCCESS)
        {
            std::cerr << "Failed to acquire swapchain image" << std::endl;
            break;
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &imageAvailable[currentFrame];
        submitInfo.pWaitDstStageMask    = waitStages;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &sc.commandBuffers[imageIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &renderFinished[currentFrame];

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                          inFlightFences[currentFrame]) != VK_SUCCESS)
        {
            std::cerr << "Failed to submit draw command" << std::endl;
            break;
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &renderFinished[currentFrame];
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &sc.swapchain;
        presentInfo.pImageIndices      = &imageIndex;

        VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);

        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
            presentResult == VK_SUBOPTIMAL_KHR)
        {
            framebufferResized = true;
        }
        else if (presentResult != VK_SUCCESS)
        {
            std::cerr << "Failed to present" << std::endl;
            break;
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // --- Cleanup ---
    vkDeviceWaitIdle(device);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(device, imageAvailable[i], nullptr);
        vkDestroySemaphore(device, renderFinished[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    sc.Destroy(device);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
