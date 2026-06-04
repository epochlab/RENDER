#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

class Shader {
public:
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void set(const std::string& name, const glm::mat4& m) const;
    void set(const std::string& name, const glm::mat3& m) const;
    void set(const std::string& name, const glm::vec3& v) const;
    void set(const std::string& name, const glm::vec2& v) const;
    void set(const std::string& name, float f) const;
    void set(const std::string& name, int i) const;
    void set(const std::string& name, bool b) const;
    void bindUniformBlock(const std::string& name, GLuint bindingPoint) const;

    // Direct GL query — use once at startup to pre-cache locations in caller locals.
    GLint uniformLoc(const std::string& name) const;

    void setAt(GLint loc, const glm::mat4& m) const;
    void setAt(GLint loc, const glm::mat3& m) const;
    void setAt(GLint loc, const glm::vec3& v) const;
    void setAt(GLint loc, const glm::vec2& v) const;
    void setAt(GLint loc, float f) const;
    void setAt(GLint loc, int i) const;
    void setAt(GLint loc, bool b) const;

private:
    GLuint m_program;
    mutable std::unordered_map<std::string, GLint> m_locCache;
    // Lazy-caches via m_locCache; called by every set(name, ...) overload.
    GLint loc(const std::string& name) const;
    static GLuint compile(const std::string& path, GLenum type);
};
