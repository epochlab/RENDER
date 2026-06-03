#pragma once

// Singleton RAII: creates a hidden 1×1 GLFW window (OpenGL 3.3 Core) on first
// construction. Subsequent constructions are no-ops. Destruction is a no-op —
// cleanup happens at process exit, which is fine for a test binary.
struct GlContext {
    GlContext();
    ~GlContext() = default;
    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;
};
