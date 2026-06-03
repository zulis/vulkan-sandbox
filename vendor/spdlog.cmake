# spdlog
set(SPDLOG_BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(spdlog)
list(APPEND LIBS spdlog)
