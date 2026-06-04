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

void Camera::setPivot(glm::vec3 pivot) {
    m_pivot = pivot;
}

void Camera::beginOrbit() {
    m_orbiting   = true;
    m_firstMouse = true;
}

void Camera::endOrbit() {
    m_orbiting = false;
}

void Camera::processInput(GLFWwindow* window, float dt) {
    float spd = moveSpeed * dt;
    glm::vec3 f = front();
    glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3{0, 1, 0}));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) m_pos += f * spd;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) m_pos -= f * spd;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) m_pos -= r * spd;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) m_pos += r * spd;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) m_pos.y += spd;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) m_pos.y -= spd;
}

float Camera::exposureValue() const {
    // Normalised: ISO 100 / f8 / 1/100s → 1.0
    // kRef = tRef * ISORef / NRef² = 0.01 * 100 / 64 = 0.015625
    static constexpr float kRef = 0.015625f;
    return (m_shutterSpeed * m_iso) / (m_fStop * m_fStop * kRef);
}

float Camera::cocScale(float imageWidthPx) const {
    // CoC_px = cocScale * abs(linDepth - focusDist) / linDepth
    return (imageWidthPx * m_focalLengthMm) / (m_fStop * m_filmbackMm);
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
    if (!m_orbiting) return;

    glm::vec3 offset = m_pos - m_pivot;

    // Yaw: rotate offset around world Y-axis.
    offset = glm::vec3(
        glm::rotate(glm::mat4(1.f), glm::radians(-dx), glm::vec3(0, 1, 0))
        * glm::vec4(offset, 0.f));

    // Pitch: rotate around local right axis.
    glm::vec3 right  = glm::normalize(glm::cross(glm::vec3(0, 1, 0), offset));
    glm::vec3 tilted = glm::vec3(
        glm::rotate(glm::mat4(1.f), glm::radians(dy), right)
        * glm::vec4(offset, 0.f));
    if (std::abs(glm::normalize(tilted).y) < 0.99f)
        offset = tilted;

    m_pos = m_pivot + offset;

    // Sync yaw/pitch so WASD re-entry reflects the new look direction.
    glm::vec3 dir = glm::normalize(m_pivot - m_pos);
    m_pitch = glm::degrees(std::asin(glm::clamp(dir.y, -1.f, 1.f)));
    m_yaw   = glm::degrees(std::atan2(dir.z, dir.x));
}
