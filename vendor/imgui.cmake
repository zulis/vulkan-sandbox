add_library(
        imgui
        STATIC
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_demo.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_tables.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp
        ${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_sdl3.cpp
)
target_include_directories(
        imgui
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/imgui
        ${CMAKE_CURRENT_LIST_DIR}/imgui/backends
        ${CMAKE_CURRENT_LIST_DIR}/SDL/include
)
if(TARGET SDL3-static)
        target_link_libraries(imgui PUBLIC SDL3-static)
elseif(TARGET SDL3::SDL3-static)
        target_link_libraries(imgui PUBLIC SDL3::SDL3-static)
elseif(TARGET SDL3::SDL3)
        target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif()
set_target_properties(imgui PROPERTIES FOLDER "imgui")
