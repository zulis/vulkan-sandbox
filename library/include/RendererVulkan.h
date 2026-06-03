#pragma once

#include <volk.h>

namespace library
{

class Window;

class RendererVulkan
{
public:
    static RendererVulkan* create(Window* window);
    virtual ~RendererVulkan() = default;

    // --- Frame management ---
    virtual VkCommandBuffer beginFrame() = 0;
    virtual void           endFrame() = 0;

    // --- Resize ---
    virtual void handleResize() = 0;

    // --- Vulkan accessors ---
    virtual VkInstance       getInstance() const = 0;
    virtual VkPhysicalDevice getPhysicalDevice() const = 0;
    virtual VkDevice         getDevice() const = 0;
    virtual VkRenderPass     getRenderPass() const = 0;
    virtual VkExtent2D       getExtent() const = 0;
    virtual VkFormat         getFormat() const = 0;
};

} // namespace library
