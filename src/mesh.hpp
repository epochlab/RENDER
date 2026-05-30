#pragma once
#include <GLFW/glfw3.h>
#include <vector>

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

    static Mesh cube();
    static Mesh plane(float size = 10.0f);
    static Mesh sphere(int stacks = 32, int slices = 32);

private:
    GLuint  m_vao, m_vbo, m_ebo;
    GLsizei m_indexCount;
};
