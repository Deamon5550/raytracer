#pragma once

typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;

typedef float real32;
typedef double real64;

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/freeglut.h>
#include <GLFW/glfw3.h>

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
