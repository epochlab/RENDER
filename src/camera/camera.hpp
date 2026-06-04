#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Camera {
public:
    // filmbackMm: sensor width (36mm = full-frame). focalLengthMm: e.g. 50mm.
    Camera(glm::vec3 position, float aspect,
           float filmbackMm    = 35.0f,
           float focalLengthMm = 70.0f,
           float near          = 0.1f,
           float far           = 100.0f);

    glm::mat4 viewMatrix()       const;
    glm::mat4 projectionMatrix() const;

    void setAspect(float aspect);
    void setFocalLength(float mm);
    void setPosition(glm::vec3 p)      { m_pos = p; }
    void setYaw(float deg)             { m_yaw = deg; }
    void setPitch(float deg);
    void setNear(float n)              { m_near = n; }
    void setFar(float f)               { m_far  = f; }
    void setFilmback(float mm)         { m_filmbackMm = mm; }
    void setISO(float iso)             { m_iso          = glm::max(iso, 1.f); }
    void setFStop(float f)             { m_fStop        = glm::max(f, 0.1f); }
    void setShutterSpeed(float s)      { m_shutterSpeed = glm::clamp(s, 1.f/8000.f, 30.f); }
    void setFocusDist(float d)         { m_focusDist    = glm::max(d, 0.01f); }
    void processInput(GLFWwindow* window, float dt);
    void processMouseMove(double xpos, double ypos);
    void resetMouse() { m_firstMouse = true; }

    // Orbit: LMB press calls setPivot + beginOrbit; release calls endOrbit.
    void setPivot(glm::vec3 pivot);   // store pivot; does not start orbiting
    void beginOrbit();                // LMB press: start tumbling
    void endOrbit();                  // LMB release: stop tumbling

    glm::vec3 position()      const { return m_pos; }
    glm::vec3 front()         const;
    float     yaw()           const { return m_yaw; }
    float     pitch()         const { return m_pitch; }
    float     nearPlane()     const { return m_near; }
    float     farPlane()      const { return m_far; }
    float     filmback()      const { return m_filmbackMm; }
    float     focalLength()   const { return m_focalLengthMm; }
    float     iso()           const { return m_iso; }
    float     fStop()         const { return m_fStop; }
    float     shutterSpeed()  const { return m_shutterSpeed; }
    float     focusDist()     const { return m_focusDist; }

    float exposureValue()                const;
    float cocScale(float imageWidthPx)   const;

    float moveSpeed        = 1.25f;
    float mouseSensitivity = 0.05f;

private:
    glm::vec3 m_pos;
    float m_yaw;
    float m_pitch;
    float m_aspect;
    float m_filmbackMm;
    float m_focalLengthMm;
    float m_near, m_far;
    float m_iso          = 100.f;
    float m_fStop        = 8.f;
    float m_shutterSpeed = 0.01f;
    float m_focusDist    = 10.f;
    bool   m_firstMouse = true;
    double m_lastX = 0.0, m_lastY = 0.0;

    glm::vec3 m_pivot    {0.f, 0.f, 0.f};
    bool      m_orbiting = false;
};
