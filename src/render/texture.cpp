#include "texture.hpp"
#include <stb_image.h>
#include <stdexcept>

Texture::Texture(const std::string& path, GLenum wrapMode) {
    bool isHdr = path.size() >= 4 && path.substr(path.size() - 4) == ".hdr";

    if (isHdr) {
        stbi_set_flip_vertically_on_load(false);
        int w, h, ch;
        float* data = stbi_loadf(path.c_str(), &w, &h, &ch, 3);
        if (!data)
            throw std::runtime_error("HDR load failed: " + path + " — " + stbi_failure_reason());
        glGenTextures(1, &m_id);
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);
        return;
    }

    stbi_set_flip_vertically_on_load(true);
    int w, h, channels;
    // Always load as RGBA so the alpha channel is available for debug views.
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data)
        throw std::runtime_error("Texture load failed: " + path + " — " + stbi_failure_reason());

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     wrapMode);
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

Texture Texture::flatNormal() {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    unsigned char pixel[4] = {128, 128, 255, 255};  // tangent-space (0,0,1) → identity perturbation
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    return Texture(id);
}

Texture Texture::createEmpty(int w, int h, GLenum internalFmt, bool generateMipmaps) {
    auto baseFmt = [](GLenum ifmt) -> GLenum {
        if (ifmt == GL_RG16F)               return GL_RG;
        if (ifmt == GL_DEPTH_COMPONENT32F)  return GL_DEPTH_COMPONENT;
        return GL_RGB;
    };
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, baseFmt(internalFmt), GL_FLOAT, nullptr);
    if (generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return Texture(id);
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
