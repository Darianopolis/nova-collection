#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <array>
#include <iostream>

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    auto window = glfwCreateWindow(1920, 1200, "next", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

// -----------------------------------------------------------------------------

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const char* vertexSource = R"(
        #version 330 core

        const vec2[6] deltas = vec2[] (
            vec2(-1, -1),
            vec2(-1,  1),
            vec2( 1,  -1),

            vec2(-1,  1),
            vec2( 1,  1),
            vec2( 1, -1)
        );

        uniform vec2 uPos;
        uniform vec2 uSize;

        void main()
        {
            gl_Position = vec4(uPos + deltas[gl_VertexID] * uSize, 0, 1);
        }
    )";
    glShaderSource(vs, 1, &vertexSource, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fragmentSource = R"(
        #version 330 core

        uniform vec4 uColor;

        out vec4 outColor;

        void main()
        {
            outColor = uColor;
        }
    )";
    glShaderSource(fs, 1, &fragmentSource, nullptr);
    glCompileShader(fs);

    GLuint sp = glCreateProgram();
    glAttachShader(sp, vs);
    glAttachShader(sp, fs);
    glLinkProgram(sp);
    glUseProgram(sp);

    GLint posLoc = glGetUniformLocation(sp, "uPos");
    GLint sizeLoc = glGetUniformLocation(sp, "uSize");
    GLint colorLoc = glGetUniformLocation(sp, "uColor");

// -----------------------------------------------------------------------------

    int count;
    auto mode = glfwGetVideoMode(glfwGetMonitors(&count)[0]);
    int mWidth = mode->width;
    int mHeight = mode->height;

    std::cout << "Monitor size = " << mWidth << ", " << mHeight << '\n';

    struct Box {
        float x, y, w, h;
    };

    Box box1 {
        .x = mWidth * 0.25f,
        .y = mHeight * 0.25f,
        .w = 100.f,
        .h = 100.f,
    };

    Box box2 {
        .x = mWidth * 0.75f,
        .y = mHeight * 0.75f,
        .w = 100.f,
        .h = 100.f,
    };

    struct Rect {
        float left = INFINITY, right = -INFINITY, top = INFINITY, bottom = -INFINITY;
    };

    auto expandBounds = [](Rect& bounds, const Box& box)
    {
        // std::cout << "Box, pos = " << box.x << ", " << box.y << ", size = " << box.w << ", " << box.h << '\n';
        // std::cout << "Box, left = " << box.x - box.w << ", right = " << box.x + box.w
        //     << ", top = " << box.y - box.h << ", bottom = " << box.y + box.h << '\n';
        bounds.left = std::min(bounds.left, box.x - box.w);
        bounds.right = std::max(bounds.right, box.x + box.w);
        bounds.top = std::min(bounds.top, box.y - box.h);
        bounds.bottom = std::max(bounds.bottom, box.y + box.h);
    };

    auto drawBox = [&](const Rect& bounds, const Box& box) {
        float width = bounds.right - bounds.left;
        float height = bounds.bottom - bounds.top;

        Box out;
        out.x = (2.f * (box.x - bounds.left) / width) - 1.f;
        out.y = 1.f - (2.f * (box.y - bounds.top) / height);
        out.w = 2.f * box.w / width;
        out.h = 2.f * box.w / height;

        // static int count = 0;
        // if (count < 1 && ++count)
        //     std::cout << "Out, pos = " << out.x << ", " << out.y << ", size = " << out.w << ", " << out.h << '\n';

        glUniform2f(posLoc, out.x, out.y);
        glUniform2f(sizeLoc, out.w, out.h);
        glUniform4f(colorLoc, 1.f, 1.f, 1.f, 1.f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };


    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

// -----------------------------------------------------------------------------

        auto moveBox = [&](Box& box, int left, int right, int up, int down) {
            if (glfwGetKey(window, left))
                box.x -= 10;

            if (glfwGetKey(window, right))
                box.x += 10;

            if (glfwGetKey(window, up))
                box.y -= 10;

            if (glfwGetKey(window, down))
                box.y += 10;
        };

        moveBox(box1, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_W, GLFW_KEY_S);
        moveBox(box2, GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN);

// -----------------------------------------------------------------------------

        Rect bounds;
        expandBounds(bounds, box1);
        expandBounds(bounds, box2);

        // std::cout << "Bounds = left = " << bounds.left << ", right = " << bounds.right
        //     << ", top = " << bounds.top << ", bottom = " << bounds.bottom << '\n';

// -----------------------------------------------------------------------------

        // int w, h;
        // glfwGetFramebufferSize(window, &w, &h);
        // glViewport(0, 0, w, h);

        int w = bounds.right - bounds.left;
        int h = bounds.bottom - bounds.top;
        glfwSetWindowSize(window, w, h);
        glfwSetWindowPos(window, bounds.left, bounds.top);
        glViewport(0, 0, w, h);

        glClearColor(1.f, 0.f, 0.f, 0.5f);
        glClear(GL_COLOR_BUFFER_BIT);

        drawBox(bounds, box1);
        drawBox(bounds, box2);

        // glUniform2f(posLoc, 0.f, 0.f);
        // glUniform2f(sizeLoc, 0.25f, 0.25f);
        // glUniform4f(colorLoc, 0.f, 1.f, 0.f, 1.f);
        // glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwWindowShouldClose(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}