#pragma once
#include <GLFW/glfw3.h>
#include <string>

class Texture {
public:
    explicit Texture(const std::string& path,
                     GLenum wrapMode = GL_CLAMP_TO_EDGE);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void bind(int unit = 0) const;
    GLuint id() const { return m_id; }

    static Texture white();
    static Texture flatNormal();
    static Texture createEmpty(int w, int h, GLenum internalFmt, bool generateMipmaps);

private:
    GLuint m_id;
    explicit Texture(GLuint id) : m_id(id) {}
};
