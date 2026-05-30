#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
        Mesh sphere = Mesh::sphere();
        Mesh ground = Mesh::plane(14.0f);

        FrameStats stats{};
        int  viewMode   = 1;
        bool prevKeys[5] = {};          // previous-frame state for keys 1–5
        double lastTime = glfwGetTime();

        while (!win.shouldClose()) {
            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime   = now;

            if (glfwGetKey(win.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(win.handle(), GLFW_TRUE);

            // ── View mode keys (one-shot on press) ─────────────
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

            // ── Draw scene ─────────────────────────────────────
            glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glPolygonMode(GL_FRONT_AND_BACK, viewMode == 2 ? GL_LINE : GL_FILL);

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

            // Restore fill for ImGui
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // ── Collect stats ──────────────────────────────────
            stats.fps            = (dt > 0.0f) ? 1.0f / dt : 0.0f;
            stats.frameTimeMs    = dt * 1000.0f;
            stats.drawCalls      = 3;
            stats.totalTriangles = cube.triangleCount() + sphere.triangleCount() + ground.triangleCount();
            stats.totalVertices  = cube.indexCount()    + sphere.indexCount()    + ground.indexCount();
            stats.width          = win.width();
            stats.height         = win.height();
            stats.camPos         = camera.position();
            stats.camYaw         = camera.yaw();
            stats.camPitch       = camera.pitch();
            stats.viewMode       = viewMode;
            stats.viewModeName   = ::viewModeName(viewMode);

            // ── HUD overlay ────────────────────────────────────
            hud.beginFrame();
            hud.draw(stats);
            hud.endFrame();

            win.swapAndPoll();
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
