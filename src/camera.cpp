#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

Camera::Camera(glm::vec3 position, float aspect,
               float filmbackMm, float focalLengthMm,
               float near, float far)
    : m_pos(position), m_yaw(-90.0f), m_pitch(0.0f),
      m_aspect(aspect),
      m_filmbackMm(filmbackMm), m_focalLengthMm(focalLengthMm),
      m_near(near), m_far(far)
{}

glm::vec3 Camera::front() const {
    float yr = glm::radians(m_yaw);
    float pr = glm::radians(m_pitch);
    return glm::normalize(glm::vec3{
        std::cos(pr) * std::cos(yr),
        std::sin(pr),
        std::cos(pr) * std::sin(yr)
    });
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(m_pos, m_pos + front(), glm::vec3{0, 1, 0});
}

glm::mat4 Camera::projectionMatrix() const {
    // Horizontal FOV from filmback + focal length; convert to vertical for glm
    float hFovRad = 2.0f * std::atan(m_filmbackMm / (2.0f * m_focalLengthMm));
    float vFovRad = 2.0f * std::atan(std::tan(hFovRad * 0.5f) / m_aspect);
    return glm::perspective(vFovRad, m_aspect, m_near, m_far);
}

void Camera::setAspect(float aspect) {
    m_aspect = aspect;
}

void Camera::setFocalLength(float mm) {
    m_focalLengthMm = mm;
}

void Camera::setPitch(float deg) {
    m_pitch = std::clamp(deg, -89.0f, 89.0f);
}

void Camera::processInput(GLFWwindow* window, float dt) {
    float spd   = moveSpeed * dt;
    glm::vec3 f = front();
    glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3{0, 1, 0}));

    if (glfwGetKey(window, GLFW_KEY_W)           == GLFW_PRESS) m_pos += f * spd;
    if (glfwGetKey(window, GLFW_KEY_S)           == GLFW_PRESS) m_pos -= f * spd;
    if (glfwGetKey(window, GLFW_KEY_A)           == GLFW_PRESS) m_pos -= r * spd;
    if (glfwGetKey(window, GLFW_KEY_D)           == GLFW_PRESS) m_pos += r * spd;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) m_pos.y += spd;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) m_pos.y -= spd;
}

void Camera::processMouseMove(double xpos, double ypos) {
    if (m_firstMouse) {
        m_lastX = xpos;
        m_lastY = ypos;
        m_firstMouse = false;
        return;
    }
    float dx =  static_cast<float>(xpos - m_lastX) * mouseSensitivity;
    float dy = -static_cast<float>(ypos - m_lastY) * mouseSensitivity;
    m_lastX = xpos;
    m_lastY = ypos;
    m_yaw   += dx;
    m_pitch  = std::clamp(m_pitch + dy, -89.0f, 89.0f);
}
