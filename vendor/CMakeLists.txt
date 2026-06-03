# assimp
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build package with shared libraries." FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "If the supplementary tools for Assimp are built in addition to the library." FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "If the test suite for Assimp is built in addition to the library." FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "Do not treat Assimp warnings as errors." FORCE)
add_subdirectory(assimp)

# flecs
include(flecs.cmake)

# glm
include(glm.cmake)

# imgui
include(imgui.cmake)

# SDL
include(SDL.cmake)

# spdlog
include(spdlog.cmake)

# stb
include(stb.cmake)

# volk
add_subdirectory(volk)

# vulkan
include(vulkan.cmake)
