#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "mesh.hpp"
#include "texture.hpp"
#include "shader.hpp"

struct SubMesh {
    Mesh    mesh;
    Texture albedo;
    Texture normalMap;  // tangent-space normal map (unit 2)
};

class Model {
public:
    static Model loadGLTF(const std::string& path);

    void draw(Shader& shader, const glm::mat4& modelMatrix) const;

    const std::string& name()         const { return m_name; }
    const glm::mat4& transform()      const { return m_transform; }
    float            boundingRadius() const { return m_boundingRadius; }
    glm::vec3        centre()         const { return m_centre; }
    int              triangleCount()  const;
    int              vertexCount()    const;

    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) noexcept        = default;
    Model& operator=(Model&&) noexcept = default;

private:
    std::vector<SubMesh> m_submeshes;
    std::string m_name;
    glm::mat4  m_transform{1.0f};
    float      m_boundingRadius = 0.0f;
    glm::vec3  m_centre{};

    Model() = default;
};
