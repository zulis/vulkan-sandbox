add_library(
        imgui
        STATIC
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_demo.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_tables.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_sdl3.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(
        imgui
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/imgui
        ${CMAKE_CURRENT_LIST_DIR}/imgui/backends
        ${CMAKE_CURRENT_LIST_DIR}/SDL/include
        ${CMAKE_CURRENT_LIST_DIR}/volk
)
if(TARGET SDL3-static)
        target_link_libraries(imgui PUBLIC SDL3-static)
elseif(TARGET SDL3::SDL3-static)
        target_link_libraries(imgui PUBLIC SDL3::SDL3-static)
elseif(TARGET SDL3::SDL3)
        target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif()
target_link_libraries(imgui PUBLIC volk)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)
set_target_properties(imgui PROPERTIES FOLDER "imgui")
