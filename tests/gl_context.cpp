#include "gl_context.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>

static bool s_ready = false;

GlContext::GlContext() {
    if (s_ready) return;
    if (!glfwInit())
        throw std::runtime_error("glfwInit failed in test GL context");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,        GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_VISIBLE,               GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(1, 1, "test", nullptr, nullptr);
    if (!win) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed in test GL context");
    }
    glfwMakeContextCurrent(win);
    s_ready = true;
}
