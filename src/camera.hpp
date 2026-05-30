#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Camera {
public:
    Camera(glm::vec3 position, float aspect, float fovDeg = 60.0f,
           float near = 0.1f, float far = 200.0f);

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix() const;

    void setAspect(float aspect);
    void processInput(GLFWwindow* window, float dt);
    void processMouseMove(double xpos, double ypos);

    float moveSpeed        = 5.0f;
    float mouseSensitivity = 0.1f;

private:
    glm::vec3 m_pos;
    float m_yaw;
    float m_pitch;
    float m_aspect, m_fov, m_near, m_far;
    bool   m_firstMouse = true;
    double m_lastX = 0.0, m_lastY = 0.0;

    glm::vec3 front() const;
};
