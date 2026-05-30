#include "mesh.hpp"
#include <utility>
#include <cstddef>
#include <cmath>

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : m_indexCount(static_cast<GLsizei>(indices.size()))
{
    // Bounding sphere radius (origin-centred, model space) for frustum culling,
    // and tracked GPU buffer footprint.
    float r2 = 0.0f;
    for (const Vertex& v : vertices) {
        float d2 = v.x * v.x + v.y * v.y + v.z * v.z;
        if (d2 > r2) r2 = d2;
    }
    m_boundingRadius = std::sqrt(r2);
    m_gpuBytes = vertices.size() * sizeof(Vertex) + indices.size() * sizeof(unsigned int);

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

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
}

Mesh::Mesh(Mesh&& o) noexcept
    : m_vao(o.m_vao), m_vbo(o.m_vbo), m_ebo(o.m_ebo), m_indexCount(o.m_indexCount),
      m_boundingRadius(o.m_boundingRadius), m_gpuBytes(o.m_gpuBytes)
{
    o.m_vao = o.m_vbo = o.m_ebo = 0;
}

Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
        m_vao = o.m_vao; m_vbo = o.m_vbo; m_ebo = o.m_ebo;
        m_indexCount     = o.m_indexCount;
        m_boundingRadius = o.m_boundingRadius;
        m_gpuBytes       = o.m_gpuBytes;
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
    // Each face: positions, normal, UVs (bl, br, tr, tl)
    std::vector<Vertex> v = {
        // front  +Z
        {-0.5f,-0.5f, 0.5f,  0, 0, 1,  0,0}, { 0.5f,-0.5f, 0.5f,  0, 0, 1,  1,0},
        { 0.5f, 0.5f, 0.5f,  0, 0, 1,  1,1}, {-0.5f, 0.5f, 0.5f,  0, 0, 1,  0,1},
        // back   -Z
        { 0.5f,-0.5f,-0.5f,  0, 0,-1,  0,0}, {-0.5f,-0.5f,-0.5f,  0, 0,-1,  1,0},
        {-0.5f, 0.5f,-0.5f,  0, 0,-1,  1,1}, { 0.5f, 0.5f,-0.5f,  0, 0,-1,  0,1},
        // left   -X
        {-0.5f,-0.5f,-0.5f, -1, 0, 0,  0,0}, {-0.5f,-0.5f, 0.5f, -1, 0, 0,  1,0},
        {-0.5f, 0.5f, 0.5f, -1, 0, 0,  1,1}, {-0.5f, 0.5f,-0.5f, -1, 0, 0,  0,1},
        // right  +X
        { 0.5f,-0.5f, 0.5f,  1, 0, 0,  0,0}, { 0.5f,-0.5f,-0.5f,  1, 0, 0,  1,0},
        { 0.5f, 0.5f,-0.5f,  1, 0, 0,  1,1}, { 0.5f, 0.5f, 0.5f,  1, 0, 0,  0,1},
        // top    +Y
        {-0.5f, 0.5f, 0.5f,  0, 1, 0,  0,0}, { 0.5f, 0.5f, 0.5f,  0, 1, 0,  1,0},
        { 0.5f, 0.5f,-0.5f,  0, 1, 0,  1,1}, {-0.5f, 0.5f,-0.5f,  0, 1, 0,  0,1},
        // bottom -Y
        {-0.5f,-0.5f,-0.5f,  0,-1, 0,  0,0}, { 0.5f,-0.5f,-0.5f,  0,-1, 0,  1,0},
        { 0.5f,-0.5f, 0.5f,  0,-1, 0,  1,1}, {-0.5f,-0.5f, 0.5f,  0,-1, 0,  0,1},
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
    float t = 1.0f;  // UVs span 0→1 across the full plane
    std::vector<Vertex> v = {
        {-h, 0,-h,  0,1,0,  0,0}, { h, 0,-h,  0,1,0,  t,0},
        { h, 0, h,  0,1,0,  t,t}, {-h, 0, h,  0,1,0,  0,t},
    };
    std::vector<unsigned int> idx = {0, 1, 2,  0, 2, 3};
    return Mesh(v, idx);
}

Mesh Mesh::sphere(int stacks, int slices) {
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;

    for (int i = 0; i <= stacks; ++i) {
        float phi = static_cast<float>(M_PI) * i / stacks;  // 0 .. PI
        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * static_cast<float>(M_PI) * j / slices;
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            float u = static_cast<float>(j) / slices;
            float v = static_cast<float>(i) / stacks;
            verts.push_back({x * 0.5f, y * 0.5f, z * 0.5f,  x, y, z,  u, v});
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned int a = i * (slices + 1) + j;
            unsigned int b = a + slices + 1;
            idx.insert(idx.end(), {a, b, a+1,  b, b+1, a+1});
        }
    }
    return Mesh(verts, idx);
}
