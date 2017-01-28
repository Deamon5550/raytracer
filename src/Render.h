#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/freeglut.h>
#include <GLFW/glfw3.h>

#include "Vector.h"

namespace raytrace {

    struct render_data {
        uint32 vbo;
        uint32 vboi;
        uint32 vao;
        uint32 tex;
        uint32 pid;

        GLFWwindow *window;
    };

    void run();

    void draw(render_data *data);

}
