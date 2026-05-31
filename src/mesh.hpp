#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstddef>
#include <cfloat>

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    float tx, ty, tz, tw;  // tangent + handedness (glTF vec4)
};

class Mesh {
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw() const;
    int       triangleCount()  const { return m_indexCount / 3; }
    int       indexCount()     const { return m_indexCount; }
    float     boundingRadius() const { return m_boundingRadius; }   // model space, origin-centred
    glm::vec3 boundsMin()      const { return m_min; }
    glm::vec3 boundsMax()      const { return m_max; }

private:
    GLuint    m_vao, m_vbo, m_ebo;
    GLsizei   m_indexCount;
    float     m_boundingRadius = 0.0f;
    glm::vec3 m_min{ FLT_MAX};
    glm::vec3 m_max{-FLT_MAX};
};
