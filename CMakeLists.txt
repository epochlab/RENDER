cmake_minimum_required(VERSION 3.20)
project(renderer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

add_executable(renderer
    src/main.cpp
    src/window.cpp
    src/shader.cpp
    src/camera.cpp
    src/mesh.cpp
)

target_include_directories(renderer PRIVATE src)

target_compile_definitions(renderer PRIVATE
    GL_SILENCE_DEPRECATION
    GLFW_INCLUDE_GLCOREARB
)

target_link_libraries(renderer PRIVATE
    glfw
    OpenGL::GL
    glm::glm
)
