
#include "Raytrace.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <random>

#ifdef WINDOWS
#include <windows.h>
#include <direct.h>
#include <io.h>
#endif
#ifdef LINUX
#include <unistd.h>
#endif

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

    // A simple 3d vector of 3 doubles
    class Vec3 {
    public:
        Vec3(double x0, double y0, double z0) {
            x = x0;
            y = y0;
            z = z0;
        }

        void set(double x0, double y0, double z0) {
            x = x0;
            y = y0;
            z = z0;
        }

        double lengthSquared() {
            return x * x + y * y + z * z;
        }

        double length() {
            return sqrt(x * x + y * y + z * z);
        }

        void normalize() {
            double len = length();
            x /= len;
            y /= len;
            z /= len;
        }

        double dot(Vec3 *o) {
            return x * o->x + y * o->y + z * o->z;
        }

        double dot(double ox, double oy, double oz) {
            return x * ox + y * oy + z * oz;
        }

        double distSquared(Vec3 *o) {
            return (x - o->x) * (x - o->x) + (y - o->y) * (y - o->y) + (z - o->z) * (z - o->z);
        }

        double x, y, z;

    };

    // An abstract class for an object in the scene
    class SceneObject {
    public:

        virtual bool intersect(Vec3 *camera, Vec3 *ray, Vec3 *result, Vec3 *normal) = 0;

        double x, y, z;
        uint32 color;
        double shiny;

        float d_red, d_green, d_blue;
        float s_red, s_green, s_blue;
    };

    // A spherical object in the scene
    class SphereObject : public SceneObject {
    public:
        SphereObject(double x0, double y0, double z0, double r0, uint32 col, double s) {
            x = x0;
            y = y0;
            z = z0;
            r = r0;
            color = col;
            shiny = s;
        }

        bool intersect(Vec3 *camera, Vec3 *ray, Vec3 *result, Vec3 *normal) override {
            Vec3 l(x - camera->x, y - camera->y, z - camera->z);
            double b = ray->dot(&l);
            if (b < 0) {
                return false;
            }
            double d2 = l.dot(&l) - b  * b;
            if (d2 > r * r) {
                return false;
            }
            double t = sqrt(r*r - d2);
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

    // A planar object, although a litle specialized to create the walls of a cornell box
    class PlaneObject : public SceneObject {
    public:
        PlaneObject(double x0, double y0, double z0, uint32 col, float s) {
            x = x0;
            y = y0;
            z = z0;
            color = col;
            shiny = s;
        }

        bool intersect(Vec3 *camera, Vec3 *ray, Vec3 *result, Vec3 *normal) override {
            if (x != 0) {
                double dx = x - camera->x;
                double mul = dx / ray->x;
                if (mul < 0) {
                    return false;
                }
                double hz = camera->z + mul * ray->z;
                if (hz < -0.01) {
                    return false;
                }
                double hy = camera->y + mul * ray->y;
                if (hy > 5 || hy < -5) {
                    return false;
                }
                result->set(x, hy, hz);
                normal->set(x < 0 ? 1 : -1, 0, 0);
                return true;
            } else if (y != 0) {
                double dy = y - camera->y;
                double mul = dy / ray->y;
                if (mul < 0) {
                    return false;
                }
                double hz = camera->z + mul * ray->z;
                if (hz < -0.01) {
                    return false;
                }
                double hx = camera->x + mul * ray->x;
                if (hx < -5 || hx > 5) {
                    return false;
                }
                result->set(hx, y, hz);
                normal->set(0, y < 0 ? 1 : -1, 0);
                return true;
            } else if (z > 0) {
                double dz = z - camera->z;
                double mul = dz / ray->z;
                if (mul < 0) {
                    return false;
                }
                double hx = camera->x + mul * ray->x;
                if (hx < -5 || hx > 5) {
                    return false;
                }
                double hy = camera->y + mul * ray->y;
                if (hy < -5 || hy > 5) {
                    return false;
                }
                result->set(hx, hy, z);
                normal->set(0, 0, z < 0 ? 1 : -1);
                return true;
            }
            return false;
        }

    };

    enum Axis {
        X_AXIS,
        Y_AXIS,
        Z_AXIS,
    };

    struct photon {
        float x, y, z;
        char power[4];
        char phi, theta;
        short flags;
    };

    void makeTree(photon** points, int points_size, photon** tree, int node, Axis axis, photon **x_sorted, photon** y_sorted, photon **z_sorted) {
        photon *median = nullptr;
        if(axis == X_AXIS) {

        } else if (axis == Y_AXIS) {

        } else {

        }
    }

    void run() {
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
#define AMBIENT_REFLECTION_CONSTANT 0.03f
#define DIFFUSE_REFLECTION_CONSTANT 0.4f
#define SPECULAR_REFLECTION_CONSTANT 0.3f

#define NUM_PHOTONS 16
#define LIGHT_POWER 1

        // Setup our cornell box
#define NUM_OBJECTS 7
        SceneObject *objects[NUM_OBJECTS];
        objects[0] = new PlaneObject(0, -5, 0, 0xFFDDDDDD, 10);
        objects[1] = new PlaneObject(0, 5, 0, 0xFFDD0000, 10);
        objects[2] = new PlaneObject(5, 0, 0, 0xFF00DD00, 10);
        objects[3] = new PlaneObject(-5, 0, 0, 0xFF0000DD, 10);
        objects[4] = new PlaneObject(0, 0, 10, 0xFFDD00DD, 10);
        objects[5] = new SphereObject(2, -3.5, 3, 1.5, 0xFFFFFF00, 25);
        objects[6] = new SphereObject(-2, -3.5, 5, 1.5, 0xFF00FFFF, 40);


        // photon mapping
        // Based on "A Practical Guide to Global Illumination using Photon Maps" from Siggraph 2000
        // https://graphics.stanford.edu/courses/cs348b-00/course8.pdf

        std::uniform_real_distribution<double> unif(0, 1);
        std::default_random_engine re;

        Vec3 ray(0, 0, 0);
        Vec3 result(0, 0, 0);
        Vec3 normal(0, 0, 0);
        Vec3 ray_source(0, 0, 0);
        float nearest = 4096 * 4096;
        SceneObject *nearest_obj = nullptr;
        Vec3 nearest_result(0, 0, 0);
        Vec3 nearest_normal(0, 0, 0);

        // We emit a number of photons from each area of the square light source
        // for each grid sqare of the light we emit 1 photon
        // @TODO: the positions should be quasi-randomized to be fairly uniform but not perfectly uniform

        // this array is expanded as needed as we do not know how many photon interactions there will be
        int photon_index = 0;
        int photon_size = NUM_PHOTONS;
        photon **photons = new photon*[NUM_PHOTONS];
        while (photon_index < photon_size) {
            double x0 = unif(re) * 2 - 1;
            double z0 = unif(re) * 2 + 3;
            double y0 = 5;
            ray_source.set(x0, y0, z0);
            // direction based on cosine distribution
            // formula for distribution from https://www.particleincell.com/2015/cosine-distribution/
            double sin_theta = sqrt(unif(re));
            double cos_theta = sqrt(1 - sin_theta*sin_theta);
            double psi = unif(re) * 6.2831853;
            Vec3 light_dir(sin_theta * cos(psi), -cos_theta, sin_theta * sin(psi));
            light_dir.normalize();


            // trace photon

            nearest = 1024 * 1024;
            nearest_obj = nullptr;
            for (int i = 0; i < NUM_OBJECTS; i++) {
                if (objects[i]->intersect(&ray_source, &light_dir, &result, &normal)) {
                    double dist = result.distSquared(&ray_source);
                    if (dist < nearest) {
                        nearest = dist;
                        nearest_obj = objects[i];
                        nearest_result.set(result.x, result.y, result.z);
                        nearest_normal.set(normal.x, normal.y, normal.z);
                    }
                }
            }
            if (nearest_obj == nullptr) {
                continue;
            }
            // we have a hit time to decide whether to reflect, absorb, or transmit
            double chance = unif(re);
            // @TODO use the diffuse and specular reflection values for each color band pg. 17
            // @TODO also support transmission
            if (chance < 0) {
                // diffuse reflection
            } else if (chance < 0.01) {
                // specular reflection
            } else {
                // absorption
                photon *next = new photon;
                next->x = nearest_result.x;
                next->y = nearest_result.y;
                next->z = nearest_result.z;
                printf("photon %.1f %.1f %.1f\n", next->x, next->y, next->z);

                photons[photon_index++] = next;
            }
        }

        int index = 0;
        photon **x_sorted = new photon*[NUM_PHOTONS];
        for (int i = 0; i < NUM_PHOTONS; i++) {
            photon *next = photons[i];
            int j = 0;
            for (; j < index; j++) {
                if (x_sorted[j]->x >= next->x) {
                    break;
                }
            }
            photon *last = x_sorted[j];
            x_sorted[j] = next;
            if (j < index) {
                for (; j < index; j++) {
                    photon *n = x_sorted[j + 1];
                    x_sorted[j + 1] = last;
                    last = n;
                }
            }
            index++;
        }
        index = 0;
        photon **y_sorted = new photon*[NUM_PHOTONS];
        photon **z_sorted = new photon*[NUM_PHOTONS];
        photon **photon_tree = new photon*[NUM_PHOTONS];
        makeTree(photons, NUM_PHOTONS, photon_tree, 0);

        // rendering

        ray_source.set(0, 0, -12);
        uint32 *pane = new uint32[1280 * 720];
        Vec3 *light_dir = new Vec3(0, 0, 0);
        for (int x = 0; x < 1280; x++) {
            double x0 = (x - 640) / 64.0 - ray_source.x;
            for (int y = 0; y < 720; y++) {
                double y0 = (y - 360) / 64.0 - ray_source.y;
                nearest = 1024 * 1024;
                nearest_obj = nullptr;
                for (int i = 0; i < NUM_OBJECTS; i++) {
                    ray.set(x0, y0, -ray_source.z);
                    ray.normalize();
                    if (objects[i]->intersect(&ray_source, &ray, &result, &normal)) {
                        // Expand: bump mapping
                        double dist = (result.x - ray_source.x) * (result.x - ray_source.x);
                        dist += (result.y - ray_source.y) * (result.y - ray_source.y);
                        dist += (result.z - ray_source.z) * (result.z - ray_source.z);
                        if (dist < nearest) {
                            nearest = dist;
                            nearest_obj = objects[i];
                            nearest_result.set(result.x, result.y, result.z);
                            nearest_normal.set(normal.x, normal.y, normal.z);
                        }
                    }
                }
                if (nearest_obj == nullptr) {
                    pane[x + y * 1280] = 0xFF000000;
                } else {
                    pane[x + y * 1280] = nearest_obj->color;
                }
            }
        }

        // Cleanup
        for (int i = 0; i < NUM_OBJECTS; i++) {
            delete objects[i];
        }

        // Upload our pane to our texture
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 720, 0, GL_BGRA, GL_UNSIGNED_BYTE, pane);

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

