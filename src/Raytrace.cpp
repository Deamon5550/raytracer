
#include "Raytrace.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef WINDOWS
#include <windows.h>
#include <direct.h>
#include <io.h>
#endif
#ifdef LINUX
#include <unistd.h>
#endif

namespace raytrace {

    const char *vertex_shader = "\
#version 330 core \n\
\n\
in vec4 position;\n\
in vec2 texture;\n\
\n\
out vec2 texture_pos;\n\
\n\
uniform mat4 view_transform;\n\
\n\
void main() {\n\
    gl_Position =  view_transform * position;\n\
    texture_pos = texture;\n\
}\n\
";
    const char *fragment_shader = "\n\
#version 330 core\n\
\n\
in vec2 texture_pos;\n\
\n\
out vec4 color;\n\
\n\
uniform sampler2D texture_sampler;\n\
\n\
void main() {\n\
    color = texture( texture_sampler, texture_pos );\n\
}\n\
";

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

    void onMouseClick(GLFWwindow *window, int button, int action, int mods) {
        if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_1) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            printf("Click %.1f %.1f\n", x, 720 - y);
        }
    }

    class Vec3 {
    public:
        Vec3(float x0, float y0, float z0) {
            x = x0;
            y = y0;
            z = z0;
        }

        void set(float x0, float y0, float z0) {
            x = x0;
            y = y0;
            z = z0;
        }

        float lengthSquared() {
            return x * x + y * y + z * z;
        }

        float length() {
            return sqrt(x * x + y * y + z * z);
        }

        void normalize() {
            float len = length();
            x /= len;
            y /= len;
            z /= len;
        }

        float dot(Vec3 *o) {
            return x * o->x + y * o->y + z * o->z;
        }

        float dot(float ox, float oy, float oz) {
            return x * ox + y * oy + z * oz;
        }

        float x, y, z;

    };

    class SceneObject {
    public:

        virtual bool intersect(Vec3 *camera, Vec3 *ray, Vec3 *result, Vec3 *normal) = 0;

        float x, y, z;
        uint32 color;
        float shiny;
    };

    class SphereObject : public SceneObject {
    public:
        SphereObject(float x0, float y0, float z0, float r0, uint32 col, float s) {
            x = x0;
            y = y0;
            z = z0;
            r = r0;
            color = col;
            shiny = s;
        }

        bool intersect(Vec3 *camera, Vec3 *ray, Vec3 *result, Vec3 *normal) override {
            Vec3 l(x - camera->x, y - camera->y, z - camera->z);
            float b = ray->dot(&l);
            if (b < 0) {
                return false;
            }
            float d2 = l.dot(&l) - b  * b;
            if (d2 > r * r) {
                return false;
            }
            float t = sqrt(r*r - d2);
            if (t < 0) {
                t = b + t;
            } else {
                t = b - t;
            }
            if (t < 0) {
                return false;
            }
            result->set(ray->x * t + camera->x, ray->y * t + camera->y, ray->z * t + camera->z);
            normal->set(result->x - x, result->y - y, result->z - z);
            normal->normalize();
            return true;
        }

        float r;
    };

    class PlaneObject : public SceneObject {
    public:
        PlaneObject(float x0, float y0, float z0, uint32 col, float s) {
            x = x0;
            y = y0;
            z = z0;
            color = col;
            shiny = s;
        }

        bool intersect(Vec3 *camera, Vec3 *ray, Vec3 *result, Vec3 *normal) override {
            if (ray->y >= 0) {
                return false;
            }
            float dy = y - camera->y;
            float mul = dy / ray->y;
            result->set(camera->x + mul * ray->x, y, camera->z + mul * ray->z);
            normal->set(0, 1, 0);
            return true;
        }

    };

    class Light {
    public:
        Light(float x0, float y0, float z0, float i) {
            x = x0;
            y = y0;
            z = z0;
            intensity = i;
        }

        float x, y, z;
        float intensity;
    };

    void run() {
        printf("Starting\n");
        if (!glfwInit()) {
            fprintf(stderr, "Failed to initialize GLFW\n");
            return;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        printf("Making window\n");
        GLFWwindow *window = glfwCreateWindow(1280, 720, "Raytrace", NULL, NULL);
        glfwMakeContextCurrent(window);
        glfwSetWindowAspectRatio(window, 16, 9);
        glewExperimental = true;
        if (glewInit() != GLEW_OK) {
            fprintf(stderr, "Failed to initialize GLEW\n");
            glfwTerminate();
            return;
        }
        glfwSwapInterval(1);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0, 0, 1, 1);
        glViewport(0, 0, 1280, 720);

        printf("Loading shaders\n");
        uint32 pid;
        loadShaders(&pid);
        if (pid == 0) {
            glfwTerminate();
            return;
        }
        uint32 matrix_location = glGetUniformLocation(pid, "view_transform");
        uint32 texture_location = glGetUniformLocation(pid, "texture_sampler");
        glm::mat4 projection_matrix = glm::ortho(0.0f, 1280.0f, 0.0f, 720.0f, 0.0f, 100.0f);
        glm::mat4 view_matrix = glm::lookAt(glm::vec3(0, 0, 4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 combined_matrix = projection_matrix * view_matrix;

        glUseProgram(pid);
        glUniformMatrix4fv(matrix_location, 1, GL_FALSE, &combined_matrix[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(texture_location, 0);

        printf("Creating VBOs\n");
        uint32 vao;
        uint32 vbo;
        uint32 vboi;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 24, (void*) 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 24, (void*) 16);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glGenBuffers(1, &vboi);

        printf("Creating texture\n");
        uint32 tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        printf("Creating vertex + index data\n");
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

#define AMBIENT_REFLECTION_CONSTANT 0.03f
#define DIFFUSE_REFLECTION_CONSTANT 0.4f
#define SPECULAR_REFLECTION_CONSTANT 0.3f

#define NUM_OBJECTS 4
        SceneObject *objects[NUM_OBJECTS];
        objects[0] = new PlaneObject(0, -2, 0, 0xFFAAAAAA, 10);
        objects[1] = new SphereObject(0, -1, 0, 1, 0xFF00FF00, 25);
        objects[2] = new SphereObject(-4, 0, 0, 2, 0xFFFF00FF, 40);
        objects[3] = new SphereObject(4, -1, 0, 1, 0xFF0000FF, 14);

#define NUM_LIGHTS 1
        Light *lights[NUM_LIGHTS];
        lights[0] = new Light(20, 20, 0, 0.95f);

        bool lights_visible[NUM_LIGHTS];
        uint32 *pane = new uint32[1280 * 720];
        Vec3 *ray = new Vec3(0, 0, 0);
        Vec3 *result = new Vec3(0, 0, 0);
        Vec3 *normal = new Vec3(0, 0, 0);
        Vec3 *camera = new Vec3(0, 0, -12);
        float nearest = 4096 * 4096;
        SceneObject *nearest_obj = nullptr;
        Vec3 *nearest_result = new Vec3(0, 0, 0);
        Vec3 *nearest_normal = new Vec3(0, 0, 0);
        Vec3 *light_dir = new Vec3(0, 0, 0);
        for (int x = 0; x < 1280; x++) {
            float x0 = (x - 640) / 64.0f - camera->x;
            for (int y = 0; y < 720; y++) {
                float y0 = (y - 360) / 64.0f - camera->y;
                nearest = 1024 * 1024;
                nearest_obj = nullptr;
                for (int i = 0; i < NUM_OBJECTS; i++) {
                    ray->set(x0, y0, -camera->z);
                    ray->normalize();
                    if (objects[i]->intersect(camera, ray, result, normal)) {
                        // Expand: bump mapping
                        float dist = (result->x - camera->x) * (result->x - camera->x);
                        dist += (result->y - camera->y) * (result->y - camera->y);
                        dist += (result->z - camera->z) * (result->z - camera->z);
                        if (dist < nearest) {
                            nearest = dist;
                            nearest_obj = objects[i];
                            nearest_result->set(result->x, result->y, result->z);
                            nearest_normal->set(normal->x, normal->y, normal->z);
                        }
                    }
                }
                if (nearest_obj == nullptr) {
                    pane[x + y * 1280] = 0xFF000000;
                } else {
                    if (x == 842 && y == 236) {
                        printf("");
                    }
                    for (int i = 0; i < NUM_LIGHTS; i++) {
                        Light *light = lights[i];
                        ray->set(light->x - nearest_result->x, light->y - nearest_result->y, light->z - nearest_result->z);
                        ray->normalize();
                        bool found = false;
                        for (int i = 0; i < NUM_OBJECTS; i++) {
                            if (objects[i] == nearest_obj) {
                                continue;
                            }
                            if (objects[i]->intersect(nearest_result, ray, result, normal)) {-
                                // Expand: light radius for soft shadows
                                // Expand: transparent or semi-transparent materials
                                found = true;
                                break;
                            }
                        }
                        lights_visible[i] = !found;
                    }
                    float intensity = AMBIENT_REFLECTION_CONSTANT;
                    for (int i = 0; i < NUM_LIGHTS; i++) {
                        if (!lights_visible[i]) {
                            continue;
                        }
                        Light *light = lights[i];
                        light_dir->set(light->x - nearest_result->x, light->y - nearest_result->y, light->z - nearest_result->z);
                        light_dir->normalize();
                        // Expand: attenuation radius for light
                        float d = nearest_normal->dot(light_dir);
                        if (d <= 0) {
                            continue;
                        }
                        intensity += light->intensity * d * DIFFUSE_REFLECTION_CONSTANT;
                        float ref = 2 * (-light_dir->x * nearest_normal->x + -light_dir->y * nearest_normal->y + -light_dir->z * nearest_normal->z);
                        Vec3 r(-light_dir->x - ref * nearest_normal->x, -light_dir->y - ref * nearest_normal->y, -light_dir->z - ref * nearest_normal->z);
                        r.normalize();
                        Vec3 v(camera->x - result->x, camera->y - result->y, camera->z - result->z);
                        v.normalize();
                        float sp = r.dot(&v);
                        if (sp <= 0) {
                            continue;
                        }
                        float spec = SPECULAR_REFLECTION_CONSTANT * std::pow(sp, nearest_obj->shiny);
                        intensity += light->intensity * spec;
                        // Expand: colored lighting
                    }
                    // Expand: check reflectivity of material and compute bounce if needed
                    int32 alpha = (nearest_obj->color >> 24) & 0xFF;
                    int32 red = (nearest_obj->color >> 16) & 0xFF;
                    red = (int) floor(red * intensity);
                    int32 green = (nearest_obj->color >> 8) & 0xFF;
                    green = (int) floor(green * intensity);
                    int32 blue = (nearest_obj->color) & 0xFF;
                    blue = (int) floor(blue * intensity);
                    pane[x + y * 1280] = (alpha << 24) | (red << 16) | (green << 8) | blue;
                }
            }
        }

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 720, 0, GL_RGBA, GL_UNSIGNED_BYTE, pane);
        printf("Starting loop\n");
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

        printf("Terminating\n");
        glDeleteBuffers(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &vboi);
        glDeleteProgram(pid);

        glfwTerminate();
    }

}

