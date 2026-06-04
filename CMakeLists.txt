cmake_minimum_required(VERSION 3.20)
project(KODAK CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "" FORCE)

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

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cgltf)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(openexr
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openexr.git
    GIT_TAG        v3.3.3
    GIT_SHALLOW    TRUE
)
set(OPENEXR_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
set(OPENEXR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(openexr)

FetchContent_Declare(OpenColorIO
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/OpenColorIO.git
    GIT_TAG        v2.4.0
    GIT_SHALLOW    TRUE
)
set(OCIO_BUILD_APPS             OFF CACHE BOOL   "" FORCE)
set(OCIO_BUILD_TESTS            OFF CACHE BOOL   "" FORCE)
set(OCIO_BUILD_PYTHON           OFF CACHE BOOL   "" FORCE)
set(OCIO_INSTALL_EXT_PACKAGES MISSING CACHE STRING "" FORCE)
FetchContent_MakeAvailable(OpenColorIO)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)
target_compile_definitions(imgui_lib PUBLIC GL_SILENCE_DEPRECATION GLFW_INCLUDE_GLCOREARB)

# ── Shared core (no main, no ObjC++ menu, no stb-write) ──────────────────────
add_library(kodak_core STATIC
    src/render/window.cpp
    src/render/shader.cpp
    src/camera/camera.cpp
    src/render/mesh.cpp
    src/render/texture.cpp
    src/render/model.cpp
    src/core/config.cpp
    src/render/cgltf_impl.cpp
    src/render/exr_io.cpp
    src/render/color_pipeline.cpp
    external/stb/stb_image.cpp
    external/stb/stb_image_write.cpp
)
target_include_directories(kodak_core PUBLIC
    src
    src/core
    src/render
    src/camera
    external/stb
    ${cgltf_SOURCE_DIR}
)
target_compile_definitions(kodak_core PUBLIC GL_SILENCE_DEPRECATION GLFW_INCLUDE_GLCOREARB)
target_link_libraries(kodak_core PUBLIC OpenGL::GL glm::glm glfw nlohmann_json::nlohmann_json OpenEXR::OpenEXR OpenColorIO)

# ── Main application ──────────────────────────────────────────────────────────
add_executable(KODAK
    src/main.cpp
    src/ui/hud.cpp
    src/ui/menu_osx.mm
)
target_include_directories(KODAK PRIVATE src src/ui)
target_link_libraries(KODAK PRIVATE kodak_core imgui_lib "-framework Cocoa")

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(tests_kodak
    tests/gl_context.cpp
    tests/reporter.cpp
    tests/test_config.cpp
    tests/test_camera.cpp
    tests/test_frustum.cpp
    tests/test_mesh.cpp
    tests/test_shader.cpp
    tests/test_texture.cpp
    tests/render_harness.cpp
    tests/test_pbr_math.cpp
    tests/test_aov.cpp
    tests/test_ssao_math.cpp
    tests/test_cpu_math.cpp
    tests/test_ssao_blur.cpp
    tests/test_exr_io.cpp
)
target_include_directories(tests_kodak PRIVATE tests)
target_link_libraries(tests_kodak PRIVATE kodak_core Catch2::Catch2WithMain)
catch_discover_tests(tests_kodak WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
