#pragma once
#include <GLFW/glfw3.h>
#include <vector>
#include <cstddef>

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
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
    int    triangleCount()  const { return m_indexCount / 3; }
    int    indexCount()     const { return m_indexCount; }
    float  boundingRadius() const { return m_boundingRadius; }   // model space, origin-centred
    size_t gpuBytes()       const { return m_gpuBytes; }          // VBO + EBO bytes

    static Mesh cube();
    static Mesh plane(float size = 10.0f);
    static Mesh sphere(int stacks = 32, int slices = 32);

private:
    GLuint  m_vao, m_vbo, m_ebo;
    GLsizei m_indexCount;
    float   m_boundingRadius = 0.0f;
    size_t  m_gpuBytes       = 0;
};
