# Vulkan — only verify SDK availability; volk handles loader + headers
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIR}")
