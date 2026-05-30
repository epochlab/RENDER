#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>
#include "window.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include "mesh.hpp"

static Camera* g_camera = nullptr;

static void onMouseMove(GLFWwindow*, double xpos, double ypos) {
    if (g_camera) g_camera->processMouseMove(xpos, ypos);
}

int main() {
    try {
        Window win(1280, 720, "Renderer");
        glfwSetInputMode(win.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPosCallback(win.handle(), onMouseMove);

        Shader shader("shaders/basic.vert", "shaders/basic.frag");

        Camera camera({0.0f, 1.5f, 6.0f}, win.aspectRatio());
        g_camera = &camera;

        Mesh cube1  = Mesh::cube();
        Mesh cube2  = Mesh::cube();
        Mesh ground = Mesh::plane(14.0f);

        double lastTime = glfwGetTime();

        while (!win.shouldClose()) {
            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime   = now;

            if (glfwGetKey(win.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(win.handle(), GLFW_TRUE);

            camera.setAspect(win.aspectRatio());
            camera.processInput(win.handle(), dt);

            glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader.use();
            shader.set("uView",       camera.viewMatrix());
            shader.set("uProjection", camera.projectionMatrix());

            // Cube 1: spinning at origin
            float angle = static_cast<float>(glfwGetTime()) * 45.0f;
            glm::mat4 m1 = glm::rotate(glm::mat4(1.0f), glm::radians(angle), {0.0f, 1.0f, 0.0f});
            shader.set("uModel", m1);
            cube1.draw();

            // Cube 2: offset, tilted
            glm::mat4 m2 = glm::translate(glm::mat4(1.0f), {2.8f, 0.0f, -1.5f});
            m2 = glm::rotate(m2, glm::radians(25.0f), {1.0f, 0.6f, 0.2f});
            shader.set("uModel", m2);
            cube2.draw();

            // Ground plane, lowered half a unit
            glm::mat4 mg = glm::translate(glm::mat4(1.0f), {0.0f, -0.5f, 0.0f});
            shader.set("uModel", mg);
            ground.draw();

            win.swapAndPoll();
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
