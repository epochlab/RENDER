#pragma once
#include <GLFW/glfw3.h>
#include <string>

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void swapAndPoll();

    GLFWwindow* handle() const { return m_window; }
    int width()  const { return m_width; }
    int height() const { return m_height; }
    float aspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }

private:
    static void onResize(GLFWwindow* win, int w, int h);

    GLFWwindow* m_window;
    int m_width, m_height;
};
