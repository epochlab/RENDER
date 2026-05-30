#include "texture.hpp"
#include <stb_image.h>
#include <stdexcept>
#include <utility>

Texture::Texture(const std::string& path) {
    stbi_set_flip_vertically_on_load(true);

    int w, h, channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
    if (!data)
        throw std::runtime_error("Texture load failed: " + path + " — " + stbi_failure_reason());

    GLenum internalFmt, fmt;
    if (channels == 1) {
        internalFmt = GL_R8;    fmt = GL_RED;
    } else if (channels == 3) {
        internalFmt = GL_RGB8;  fmt = GL_RGB;
    } else {
        internalFmt = GL_RGBA8; fmt = GL_RGBA;
    }

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
}

Texture::~Texture() {
    if (m_id) glDeleteTextures(1, &m_id);
}

Texture::Texture(Texture&& o) noexcept : m_id(o.m_id) { o.m_id = 0; }

Texture& Texture::operator=(Texture&& o) noexcept {
    if (this != &o) {
        if (m_id) glDeleteTextures(1, &m_id);
        m_id = o.m_id;
        o.m_id = 0;
    }
    return *this;
}

void Texture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

Texture Texture::white() {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    unsigned char pixel[3] = {255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    return Texture(id);
}
