# SDL's CMakeLists may include an old cmake_minimum_required version.
# Ensure compatibility with CMake versions that no longer support < 3.5 compatibility mode.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE INTERNAL "Minimum CMake policy compatibility version for SDL")

add_subdirectory(SDL)