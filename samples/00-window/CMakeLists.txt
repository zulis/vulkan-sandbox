project(00-window LANGUAGES CXX)
# file(GLOB_RECURSE ALL_FILES ../common/*cpp ../common/*.h *.cpp *.h)
file(GLOB_RECURSE ALL_FILES *.cpp *.h)
add_executable(${PROJECT_NAME} ${ALL_FILES})
# target_link_libraries(${PROJECT_NAME} library)
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    volk
)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "samples")