project(library LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Collect library sources (src/ only — public API is in include/)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ALL_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

# ---------------------------------------------------------------------------
# Static library target
# ---------------------------------------------------------------------------
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

# Public: only expose include/ to consumers
# Private: src/ for internal implementation headers
target_include_directories(${PROJECT_NAME}
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
# Vendor dependencies (all PUBLIC — transitive includes for examples)
# ---------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PUBLIC assimp)

# flecs
if(TARGET flecs_static)
    target_link_libraries(${PROJECT_NAME} PUBLIC flecs_static)
elseif(TARGET flecs::flecs_static)
    target_link_libraries(${PROJECT_NAME} PUBLIC flecs::flecs_static)
elseif(TARGET flecs)
    target_link_libraries(${PROJECT_NAME} PUBLIC flecs)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC glm)
target_link_libraries(${PROJECT_NAME} PUBLIC imgui)

# SDL3 (includes come transitively from the SDL target)
if(TARGET SDL3-static)
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL3-static)
elseif(TARGET SDL3::SDL3-static)
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3-static)
elseif(TARGET SDL3::SDL3)
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC stb)
target_link_libraries(${PROJECT_NAME} PUBLIC volk)