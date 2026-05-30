#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <mach/mach.h>
#include <iostream>
#include <stdexcept>
#include "window.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include "mesh.hpp"
#include "texture.hpp"
#include "hud.hpp"

static Camera* g_camera = nullptr;

static void onMouseMove(GLFWwindow*, double xpos, double ypos) {
    if (g_camera) g_camera->processMouseMove(xpos, ypos);
}

static const char* viewModeName(int m) {
    switch (m) {
        case 1: return "Diffuse";
        case 2: return "Wireframe";
        case 3: return "Normals";
        case 4: return "Depth";
        case 5: return "UV";
        default: return "Unknown";
    }
}

static float queryMemoryMB() {
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
        return 0.0f;
    return static_cast<float>(info.phys_footprint) / (1024.0f * 1024.0f);
}

int main() {
    try {
        Window win(1280, 720, "Renderer");
        glfwSetInputMode(win.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPosCallback(win.handle(), onMouseMove);

        Shader shader("shaders/basic.vert", "shaders/basic.frag");
        shader.use();
        shader.set("uAlbedo", 0);

        Camera camera({0.0f, 1.5f, 6.0f}, win.aspectRatio());
        g_camera = &camera;

        HUD hud(win.handle());

        Texture checker("assets/textures/checker.png");
        Texture white = Texture::white();

        Mesh cube   = Mesh::cube();
        Mesh sphere = Mesh::sphere(16, 16);   // 512 tris, radius 0.5m
        Mesh ground = Mesh::plane(10.0f);     // 10m × 10m

        // GPU timer query
        GLuint gpuQuery       = 0;
        bool   gpuQueryActive = false;
        glGenQueries(1, &gpuQuery);

        FrameStats stats{};
        int  viewMode    = 1;
        bool prevKeys[5] = {};
        double lastTime  = glfwGetTime();

        while (!win.shouldClose()) {
            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime   = now;

            if (glfwGetKey(win.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(win.handle(), GLFW_TRUE);

            // ── View mode keys ─────────────────────────────────
            static const int viewKeys[5] = {
                GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5
            };
            for (int i = 0; i < 5; ++i) {
                bool down = glfwGetKey(win.handle(), viewKeys[i]) == GLFW_PRESS;
                if (down && !prevKeys[i]) viewMode = i + 1;
                prevKeys[i] = down;
            }

            camera.setAspect(win.aspectRatio());
            camera.processInput(win.handle(), dt);

            // ── Read last frame's GPU timer ────────────────────
            if (gpuQueryActive) {
                GLuint ns = 0;
                glGetQueryObjectuiv(gpuQuery, GL_QUERY_RESULT, &ns);
                stats.gpuTimeMs  = static_cast<float>(ns) / 1e6f;
                gpuQueryActive   = false;
            }

            // ── Draw scene ─────────────────────────────────────
            glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glPolygonMode(GL_FRONT_AND_BACK, viewMode == 2 ? GL_LINE : GL_FILL);

            glBeginQuery(GL_TIME_ELAPSED, gpuQuery);

            shader.use();
            shader.set("uView",       camera.viewMatrix());
            shader.set("uProjection", camera.projectionMatrix());
            shader.set("uViewMode",   viewMode);
            shader.set("uNear",       camera.nearPlane());
            shader.set("uFar",        camera.farPlane());

            float angle = static_cast<float>(glfwGetTime()) * 40.0f;
            glm::mat4 mCube = glm::rotate(glm::mat4(1.0f), glm::radians(angle), {0.0f, 1.0f, 0.0f});
            checker.bind(0);
            shader.set("uModel", mCube);
            cube.draw();

            glm::mat4 mSphere = glm::translate(glm::mat4(1.0f), {2.5f, 0.0f, -1.0f});
            checker.bind(0);
            shader.set("uModel", mSphere);
            sphere.draw();

            glm::mat4 mGround = glm::translate(glm::mat4(1.0f), {0.0f, -0.5f, 0.0f});
            white.bind(0);
            shader.set("uModel", mGround);
            ground.draw();

            glEndQuery(GL_TIME_ELAPSED);
            gpuQueryActive = true;

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // ── Collect stats ──────────────────────────────────
            stats.fps            = (dt > 0.0f) ? 1.0f / dt : 0.0f;
            stats.frameTimeMs    = dt * 1000.0f;
            stats.memMB          = queryMemoryMB();
            stats.drawCalls      = 3;
            stats.totalTriangles = cube.triangleCount() + sphere.triangleCount() + ground.triangleCount();
            stats.totalVertices  = cube.indexCount()    + sphere.indexCount()    + ground.indexCount();
            stats.width          = win.width();
            stats.height         = win.height();
            stats.camPos            = camera.position();
            stats.camRotX           = camera.pitch();      // X = pitch
            stats.camRotY           = camera.yaw();        // Y = yaw
            stats.camRotZ           = 0.0f;               // no roll
            stats.camFilmbackMm     = camera.filmback();
            stats.camFocalLengthMm  = camera.focalLength();
            stats.camNear           = camera.nearPlane();
            stats.camFar            = camera.farPlane();
            stats.viewMode       = viewMode;
            stats.viewModeName   = ::viewModeName(viewMode);

            // Per-object breakdown
            stats.numObjects     = 3;
            stats.objects[0]     = {"cube",   cube.triangleCount(),   cube.indexCount()};
            stats.objects[1]     = {"sphere", sphere.triangleCount(), sphere.indexCount()};
            stats.objects[2]     = {"ground", ground.triangleCount(), ground.indexCount()};

            // ── HUD overlay ────────────────────────────────────
            hud.beginFrame();
            hud.draw(stats);
            hud.endFrame();

            win.swapAndPoll();
        }

        glDeleteQueries(1, &gpuQuery);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
