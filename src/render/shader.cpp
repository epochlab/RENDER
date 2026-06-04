#include "shader.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint Shader::compile(const std::string& path, GLenum type) {
    std::string src = readFile(path);
    const char* csrc = src.c_str();
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &csrc, nullptr);
    glCompileShader(sh);
    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        glDeleteShader(sh);
        throw std::runtime_error("Shader compile error (" + path + "):\n" + log);
    }
    return sh;
}

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    GLuint vert = compile(vertPath, GL_VERTEX_SHADER);
    GLuint frag = compile(fragPath, GL_FRAGMENT_SHADER);
    m_program = glCreateProgram();
    glAttachShader(m_program, vert);
    glAttachShader(m_program, frag);
    glLinkProgram(m_program);
    glDeleteShader(vert);
    glDeleteShader(frag);
    GLint ok;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        glDeleteProgram(m_program);
        throw std::runtime_error("Shader link error:\n" + std::string(log));
    }
}

Shader::~Shader() {
    glDeleteProgram(m_program);
}

void Shader::use() const {
    glUseProgram(m_program);
}

GLint Shader::loc(const std::string& name) const {
    auto [it, inserted] = m_locCache.emplace(name, -1);
    if (inserted) it->second = glGetUniformLocation(m_program, name.c_str());
    return it->second;
}

void Shader::set(const std::string& name, const glm::mat4& m) const {
    glUniformMatrix4fv(loc(name), 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::set(const std::string& name, const glm::mat3& m) const {
    glUniformMatrix3fv(loc(name), 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::set(const std::string& name, const glm::vec3& v) const {
    glUniform3fv(loc(name), 1, glm::value_ptr(v));
}

void Shader::set(const std::string& name, const glm::vec2& v) const {
    glUniform2fv(loc(name), 1, glm::value_ptr(v));
}

void Shader::set(const std::string& name, float f) const {
    glUniform1f(loc(name), f);
}

void Shader::set(const std::string& name, int i) const {
    glUniform1i(loc(name), i);
}

void Shader::set(const std::string& name, bool b) const {
    glUniform1i(loc(name), b ? 1 : 0);
}

GLint Shader::uniformLoc(const std::string& name) const {
    return glGetUniformLocation(m_program, name.c_str());
}

void Shader::setAt(GLint loc, const glm::mat4& m) const {
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::setAt(GLint loc, const glm::mat3& m) const {
    glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::setAt(GLint loc, const glm::vec3& v) const {
    glUniform3fv(loc, 1, glm::value_ptr(v));
}

void Shader::setAt(GLint loc, const glm::vec2& v) const {
    glUniform2fv(loc, 1, glm::value_ptr(v));
}

void Shader::setAt(GLint loc, float f) const {
    glUniform1f(loc, f);
}

void Shader::setAt(GLint loc, int i) const {
    glUniform1i(loc, i);
}

void Shader::setAt(GLint loc, bool b) const {
    glUniform1i(loc, b ? 1 : 0);
}

void Shader::bindUniformBlock(const std::string& name, GLuint bindingPoint) const {
    GLuint idx = glGetUniformBlockIndex(m_program, name.c_str());
    if (idx != GL_INVALID_INDEX)
        glUniformBlockBinding(m_program, idx, bindingPoint);
}
