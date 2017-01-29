
#include "Render.h"

#include <cstdio>
#include <cstring>
#include <chrono>

#include "Scene.h"
#include "Raytrace.h"
#include "Random.h"

namespace raytrace {

    // Trivial vertex and fragment shaders to render the scene pane onto the window
    const char *vertex_shader = "\
#version 330 core \n\
in vec4 position;\n\
in vec2 texture;\n\
out vec2 texture_pos;\n\
uniform mat4 view_transform;\n\
void main() {\n\
    gl_Position = view_transform * position;\n\
    texture_pos = texture;\n\
}\n\
";
    const char *fragment_shader = "\n\
#version 330 core\n\
in vec2 texture_pos;\n\
out vec4 color;\n\
uniform sampler2D texture_sampler;\n\
void main() {\n\
    color = texture(texture_sampler, texture_pos);\n\
}\n\
";

    // Loads our trivial vertex and fragment shaders and returns the program id to the given ptr.
    void loadShaders(uint32 *result_id) {
        GLuint vs_id = glCreateShader(GL_VERTEX_SHADER);
        GLuint fs_id = glCreateShader(GL_FRAGMENT_SHADER);

        GLint result = GL_FALSE;
        int info_log_length;

        glShaderSource(vs_id, 1, &vertex_shader, NULL);
        glCompileShader(vs_id);

        glGetShaderiv(vs_id, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE) {
            glGetShaderiv(vs_id, GL_INFO_LOG_LENGTH, &info_log_length);
            char *error_msg = new char[info_log_length + 1];
            glGetShaderInfoLog(vs_id, info_log_length, NULL, error_msg);
            fprintf(stderr, "Error compiling vertex shader: %s\n", error_msg);
            delete[] error_msg;
            *result_id = 0;
            return;
        }

        glShaderSource(fs_id, 1, &fragment_shader, NULL);
        glCompileShader(fs_id);

        glGetShaderiv(fs_id, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE) {
            glGetShaderiv(fs_id, GL_INFO_LOG_LENGTH, &info_log_length);
            char *error_msg = new char[info_log_length + 1];
            glGetShaderInfoLog(fs_id, info_log_length, NULL, error_msg);
            fprintf(stderr, "Error compiling fragment shader: %s\n", error_msg);
            delete[] error_msg;
            *result_id = 0;
            return;
        }

        GLuint p_id = glCreateProgram();
        glAttachShader(p_id, vs_id);
        glAttachShader(p_id, fs_id);
        glLinkProgram(p_id);

        glGetProgramiv(p_id, GL_LINK_STATUS, &result);
        if (result == GL_FALSE) {
            glGetShaderiv(vs_id, GL_INFO_LOG_LENGTH, &info_log_length);
            char *error_msg = new char[info_log_length + 1];
            glGetShaderInfoLog(vs_id, info_log_length, NULL, error_msg);
            fprintf(stderr, "Error linking program %s\n", error_msg);
            delete[] error_msg;
            *result_id = 0;
            return;
        }

        glDetachShader(p_id, vs_id);
        glDetachShader(p_id, fs_id);

        glDeleteShader(vs_id);
        glDeleteShader(fs_id);
        *result_id = p_id;
    }

    // For debugging the scene this returns the pixel position of a mouse click so that
    // it is easier to set a break point for that pixel
    void onMouseClick(GLFWwindow *window, int button, int action, int mods) {
        if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_1) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            printf("Click %.1f %.1f\n", x, 720 - y);
        }
    }

    void run() {
        // Seed the random engine with the current epoch tick
        int64 time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        random::init(time);
        // Initialize opengl
        if (!glfwInit()) {
            fprintf(stderr, "Failed to initialize GLFW\n");
            return;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        // Create the window
        GLFWwindow *window = glfwCreateWindow(1280, 720, "Raytrace", NULL, NULL);
        glfwMakeContextCurrent(window);
        glewExperimental = true;
        if (glewInit() != GLEW_OK) {
            fprintf(stderr, "Failed to initialize GLEW\n");
            glfwTerminate();
            return;
        }
        glfwSwapInterval(1);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        // a nice hot pink so we know if we're not drawing properly
        glClearColor(1, 0, 1, 1);
        glViewport(0, 0, 1280, 720);

        uint32 pid;
        loadShaders(&pid);
        if (pid == 0) {
            glfwTerminate();
            return;
        }
        uint32 matrix_location = glGetUniformLocation(pid, "view_transform");
        uint32 texture_location = glGetUniformLocation(pid, "texture_sampler");
        // Setup a simple orthographic projection matrix
        glm::mat4 projection_matrix = glm::ortho(0.0f, 1280.0f, 0.0f, 720.0f, 0.0f, 100.0f);
        glm::mat4 view_matrix = glm::lookAt(glm::vec3(0, 0, 4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 combined_matrix = projection_matrix * view_matrix;

        glUseProgram(pid);
        glUniformMatrix4fv(matrix_location, 1, GL_FALSE, &combined_matrix[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(texture_location, 0);

        uint32 vao;
        uint32 vbo;
        uint32 vboi;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 24, (void*) 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 24, (void*) 16);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glGenBuffers(1, &vboi);

        uint32 tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        float vertex_buffer[24] = {0, 720, 0, 1, 0, 1,
            0, 0, 0, 1, 0, 0,
            1280, 0, 0, 1, 1, 0,
            1280, 720, 0, 1, 1, 1};
        uint16 index_buffer[6] = {0, 1, 2, 2, 3, 0};

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, 96, vertex_buffer, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboi);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 12, index_buffer, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glfwSetMouseButtonCallback(window, onMouseClick);

        // Rendering constants

        printf("Setting up scene\n");
        // Setup our cornell box
        Scene *scene = new Scene(7);
        scene->objects[0] = new PlaneObject(0, -5, 0, -5, 5, 0xFFEEEEEE, 0.4, 0.0, 0.0, 1.0);
        scene->objects[1] = new PlaneObject(0, 5, 0, -5, 5, 0xFFDEEEEEE, 0.4, 0.0, 0.0, 1.0);
        scene->objects[2] = new PlaneObject(5, 0, 0, -5, 5, 0xFFFF3333, 0.4, 0.0, 0.0, 1.0);
        scene->objects[3] = new PlaneObject(-5, 0, 0, -5, 5, 0xFF3333FF, 0.4, 0.0, 0.0, 1.0);
        scene->objects[4] = new PlaneObject(0, 0, 10, -5, 5, 0xFFEEEEEE, 0.4, 0.0, 0.0, 1.0);
        scene->objects[5] = new SphereObject(2, -3.5, 3, 1.5, 0xFFFFFFFF, 0.0, 0.1, 0.9, 0.0);
        scene->objects[5]->refraction = 2.5;
        scene->objects[6] = new SphereObject(-2, -3.5, 5, 1.5, 0xFFFFFFFF, 0.0, 1.0, 0.0, 0.0);
        //scene->objects[7] = new SphereObject(0, 1, 5, 0.8, 0xFF33FF33, 0.4, 0.0, 0.0, 1.0, 0.2, 0, 0);

        int32 sample_ratio = 1;
        uint32 *pane = new uint32[1280 * 720 * sample_ratio * sample_ratio];
        Vec3 camera(0, 0, -12);
        raytrace::renderScene(scene, camera, pane, 1280 * sample_ratio, 720 * sample_ratio);

        uint32 *sample = new uint32[1280 * 720];
        for (int x = 0; x < 1280; x++) {
            for (int y = 0; y < 720; y++) {
                int32 red = 0;
                int32 green = 0;
                int32 blue = 0;
                for (int x0 = 0; x0 < sample_ratio; x0++) {
                    for (int y0 = 0; y0 < sample_ratio; y0++) {
                        int32 s = pane[x * sample_ratio + x0 + (y * sample_ratio + y0) * 1280 * sample_ratio];
                        red += (s >> 16) & 0xFF;
                        green += (s >> 8) & 0xFF;
                        blue += s & 0xFF;
                    }
                }
                red /= sample_ratio * sample_ratio;
                green /= sample_ratio * sample_ratio;
                blue /= sample_ratio * sample_ratio;
                sample[x + y * 1280] = (0xFF << 24) | (red << 16) | (green << 8) | blue;
            }
        }

        // Cleanup
        delete scene;

        // Upload our pane to our texture
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 720, 0, GL_BGRA, GL_UNSIGNED_BYTE, sample);

        while (!glfwWindowShouldClose(window)) {

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboi);

            glUseProgram(pid);

            glBindTexture(GL_TEXTURE_2D, tex);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void*) 0);

            glUseProgram(0);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            glfwSwapBuffers(window);
            glfwPollEvents();

        }

        glDeleteBuffers(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &vboi);
        glDeleteProgram(pid);

        glfwTerminate();
    }

}

