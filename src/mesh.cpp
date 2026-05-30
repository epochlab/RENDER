#include "mesh.hpp"
#include <utility>
#include <cstddef>

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : m_indexCount(static_cast<GLsizei>(indices.size()))
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, nx)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
}

Mesh::Mesh(Mesh&& o) noexcept
    : m_vao(o.m_vao), m_vbo(o.m_vbo), m_ebo(o.m_ebo), m_indexCount(o.m_indexCount)
{
    o.m_vao = o.m_vbo = o.m_ebo = 0;
}

Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
        m_vao = o.m_vao; m_vbo = o.m_vbo; m_ebo = o.m_ebo;
        m_indexCount = o.m_indexCount;
        o.m_vao = o.m_vbo = o.m_ebo = 0;
    }
    return *this;
}

void Mesh::draw() const {
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

Mesh Mesh::cube() {
    std::vector<Vertex> v = {
        // front  +Z
        {-0.5f,-0.5f, 0.5f,  0, 0, 1}, { 0.5f,-0.5f, 0.5f,  0, 0, 1},
        { 0.5f, 0.5f, 0.5f,  0, 0, 1}, {-0.5f, 0.5f, 0.5f,  0, 0, 1},
        // back   -Z
        { 0.5f,-0.5f,-0.5f,  0, 0,-1}, {-0.5f,-0.5f,-0.5f,  0, 0,-1},
        {-0.5f, 0.5f,-0.5f,  0, 0,-1}, { 0.5f, 0.5f,-0.5f,  0, 0,-1},
        // left   -X
        {-0.5f,-0.5f,-0.5f, -1, 0, 0}, {-0.5f,-0.5f, 0.5f, -1, 0, 0},
        {-0.5f, 0.5f, 0.5f, -1, 0, 0}, {-0.5f, 0.5f,-0.5f, -1, 0, 0},
        // right  +X
        { 0.5f,-0.5f, 0.5f,  1, 0, 0}, { 0.5f,-0.5f,-0.5f,  1, 0, 0},
        { 0.5f, 0.5f,-0.5f,  1, 0, 0}, { 0.5f, 0.5f, 0.5f,  1, 0, 0},
        // top    +Y
        {-0.5f, 0.5f, 0.5f,  0, 1, 0}, { 0.5f, 0.5f, 0.5f,  0, 1, 0},
        { 0.5f, 0.5f,-0.5f,  0, 1, 0}, {-0.5f, 0.5f,-0.5f,  0, 1, 0},
        // bottom -Y
        {-0.5f,-0.5f,-0.5f,  0,-1, 0}, { 0.5f,-0.5f,-0.5f,  0,-1, 0},
        { 0.5f,-0.5f, 0.5f,  0,-1, 0}, {-0.5f,-0.5f, 0.5f,  0,-1, 0},
    };
    std::vector<unsigned int> idx;
    idx.reserve(36);
    for (unsigned int f = 0; f < 6; ++f) {
        unsigned int b = f * 4;
        idx.insert(idx.end(), {b, b+1, b+2,  b, b+2, b+3});
    }
    return Mesh(v, idx);
}

Mesh Mesh::plane(float size) {
    float h = size * 0.5f;
    std::vector<Vertex> v = {
        {-h, 0,-h,  0, 1, 0}, { h, 0,-h,  0, 1, 0},
        { h, 0, h,  0, 1, 0}, {-h, 0, h,  0, 1, 0},
    };
    std::vector<unsigned int> idx = {0, 1, 2,  0, 2, 3};
    return Mesh(v, idx);
}
