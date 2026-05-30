#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void set(const std::string& name, const glm::mat4& m) const;
    void set(const std::string& name, const glm::vec3& v) const;
    void set(const std::string& name, const glm::vec2& v) const;
    void set(const std::string& name, float f) const;
    void set(const std::string& name, int i) const;

private:
    GLuint m_program;
    GLint loc(const std::string& name) const;
    static GLuint compile(const std::string& path, GLenum type);
};
