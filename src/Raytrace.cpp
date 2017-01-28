
#include "Raytrace.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
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

    std::default_random_engine *rand_engine = new std::default_random_engine;

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

        void add(double x0, double y0, double z0) {
            x += x0;
            y += y0;
            z += z0;
        }

        void mul(double x0) {
            x *= x0;
            y *= x0;
            z *= x0;
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

        double distSquared(float ox, float oy, float oz) {
            return (x - ox) * (x - ox) + (y - oy) * (y - oy) + (z - oz) * (z - oz);
        }

        double x, y, z;

    };

    Vec3 cross(Vec3 *a, Vec3 *b) {
        Vec3 result(a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x);
        return result;
    }

    // An abstract class for an object in the scene
    class SceneObject {
    public:

        virtual bool intersect(Vec3 *camera, Vec3 *ray, Vec3 *result, Vec3 *normal) = 0;

        double x, y, z;
        uint32 color;

        double diffuse;
        double specular;
    };

    // A spherical object in the scene
    class SphereObject : public SceneObject {
    public:
        SphereObject(double x0, double y0, double z0, double r0, uint32 col, double d, double s) {
            x = x0;
            y = y0;
            z = z0;
            r = r0;
            color = col;
            diffuse = d;
            specular = s;
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
        PlaneObject(double x0, double y0, double z0, uint32 col, double d, double s) {
            x = x0;
            y = y0;
            z = z0;
            color = col;
            diffuse = d;
            specular = s;
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
        unsigned char power[4];
        float dx, dy, dz;
        char bounce;
    };

    struct kdnode {
        photon *value;
        kdnode *left;
        kdnode *right;
        Axis splitting_axis;
    };

    Axis largestAxis(double x, double y, double z) {
        if (x > y) {
            if (x > z) {
                return X_AXIS;
            } else {
                return Z_AXIS;
            }
        } else {
            if (y > z) {
                return Y_AXIS;
            } else {
                return Z_AXIS;
            }
        }
    }

    photon *findMedianPhoton(photon** photons, int size, Axis axis) {
        std::uniform_int_distribution<int> unif(0, size - 1);
        int k = unif(*rand_engine);
        photon *p = photons[k];
        photon** scratch = new photon*[size];
        int less_index = 0;
        int great_index = size - 1;
        double p_val;
        if (axis == X_AXIS) {
            p_val = p->x;
        } else if (axis == Y_AXIS) {
            p_val = p->y;
        } else {
            p_val = p->z;
        }
        for (int i = 0; i < size; i++) {
            photon *next = photons[i];
            if (next == p) {
                continue;
            }
            double val;
            if (axis == X_AXIS) {
                val = next->x;
            } else if (axis == Y_AXIS) {
                val = next->y;
            } else {
                val = next->z;
            }
            if (val <= p_val) {
                scratch[less_index++] = next;
            } else {
                scratch[great_index--] = next;
            }
        }
        photon *r;
        if (less_index == size / 2) {
            r = p;
        } else if (less_index > size / 2) {
            r = findMedianPhoton(scratch, less_index, axis);
        } else {
            r = findMedianPhoton(scratch + great_index + 1, size - great_index - 1, axis);
        }
        delete[] scratch;
        return r;
    }

    kdnode *createKDTree(photon **photons, int size, photon **scratch, int scratch_size) {
        Vec3 min(1000, 1000, 1000);
        Vec3 max(-1000, -1000, -1000);
        for (int i = 0; i < size; i++) {
            photon *next = photons[i];
            if (next->x < min.x) {
                min.x = next->x;
            }
            if (next->x > max.x) {
                max.x = next->x;
            }
            if (next->y < min.y) {
                min.y = next->y;
            }
            if (next->y > max.y) {
                max.y = next->y;
            }
            if (next->z < min.z) {
                min.z = next->z;
            }
            if (next->z > max.z) {
                max.z = next->z;
            }
        }

        double x_size = (max.x - min.x);
        double y_size = (max.y - min.y);
        double z_size = (max.z - min.z);
        Axis split = largestAxis(x_size, y_size, z_size);
        photon *median = findMedianPhoton(photons, size, split);
        double m_val;
        if (split == X_AXIS) {
            m_val = median->x;
        } else if (split == Y_AXIS) {
            m_val = median->y;
        } else {
            m_val = median->z;
        }
        int less_index = 0;
        int greater_index = size - 1;
        for (int i = 0; i < size; i++) {
            photon *next = photons[i];
            if (next == median) {
                continue;
            }
            double val;
            if (split == X_AXIS) {
                val = next->x;
            } else if (split == Y_AXIS) {
                val = next->y;
            } else {
                val = next->z;
            }
            if (val <= m_val) {
                scratch[less_index++] = next;
            } else {
                scratch[greater_index--] = next;
            }
        }
        kdnode *node = new kdnode;
        node->splitting_axis = split;
        node->value = median;
        if (less_index != 0) {
            node->left = createKDTree(scratch, less_index, photons, less_index);
        } else {
            node->left = nullptr;
        }
        if (greater_index < size - 1) {
            node->right = createKDTree(scratch + greater_index + 1, size - (greater_index + 1), photons + (greater_index + 1), size - (greater_index + 1));
        } else {
            node->right = nullptr;
        }
        return node;
    }

    void insert(photon **nearest, double *distances, int k, int size, photon *next, double dist) {
        if (size < k) {
            int i = size;
            nearest[i] = next;
            distances[i] = dist;
            while (i != 0) {
                // heapify up
                int parent = (int) floor((i - 1) / 2);
                if (distances[parent] >= distances[i]) {
                    break;
                }
                double t = distances[parent];
                photon *tp = nearest[parent];
                distances[parent] = distances[i];
                nearest[parent] = nearest[i];
                distances[i] = t;
                nearest[i] = tp;
                i = parent;
            }
        } else {
            if (dist > distances[0]) {
                return;
            }
            distances[0] = dist;
            nearest[0] = next;
            int i = 0;
            while (i < 31) {
                // heapify down the largest child node
                if (distances[2 * i + 1] < distances[2 * i + 2]) {
                    if (distances[2 * i + 2] > distances[i]) {
                        int parent = i;
                        i = 2 * i + 2;
                        double t = distances[parent];
                        photon *tp = nearest[parent];
                        distances[parent] = distances[i];
                        nearest[parent] = nearest[i];
                        distances[i] = t;
                        nearest[i] = tp;
                    } else {
                        break;
                    }
                } else {
                    if (distances[2 * i + 1] > distances[i]) {
                        int parent = i;
                        i = 2 * i + 1;
                        double t = distances[parent];
                        photon *tp = nearest[parent];
                        distances[parent] = distances[i];
                        nearest[parent] = nearest[i];
                        distances[i] = t;
                        nearest[i] = tp;
                    } else {
                        break;
                    }
                }
            }
        }

    }

    int find_nearest_photons(photon **nearest, double *distances, int k, int size, Vec3 *target, kdnode *root, double max_dist) {
        float dx = root->value->x - target->x;
        float dy = root->value->y - target->y;
        float dz = root->value->z - target->z;
        double dist = dx * dx + dy * dy + dz * dz;
        if (dist < max_dist) {
            if (size < k) {
                insert(nearest, distances, k, size, root->value, dist);
                size++;
            } else if (dist < distances[0]) {
                insert(nearest, distances, k, size, root->value, dist);
            }
        }

        // determine which side of the splitting plane we are on
        // true is positive side
        bool side = true;
        float axis_dist;
        if (root->splitting_axis == X_AXIS) {
            side = target->x <= root->value->x;
            axis_dist = dx * dx;
        } else if (root->splitting_axis == Y_AXIS) {
            side = target->y <= root->value->y;
            axis_dist = dy * dy;
        } else if (root->splitting_axis == Z_AXIS) {
            side = target->z <= root->value->z;
            axis_dist = dz * dz;
        }

        // recurse into that side first
        kdnode *side_node = side ? root->left : root->right;
        if (side_node != nullptr) {
            size = find_nearest_photons(nearest, distances, k, size, target, side_node, max_dist);
        }

        // check if the current max distance is larger than the distance from the target to the splitting plane
        if (size == 0 || distances[0] > axis_dist) {
            // if yes then recurse into that side as well
            side_node = !side ? root->left : root->right;
            if (side_node != nullptr) {
                size = find_nearest_photons(nearest, distances, k, size, target, side_node, max_dist);
            }
        }

        return size;
    }

    void showPhotons(uint32 *pane, kdnode *tree) {

        double dz = tree->value->z;
        Vec3 dir(-tree->value->x, -tree->value->y, -12 - tree->value->z);
        dir.normalize();
        dir.mul((dz / dir.z));

        int x0 = (int) floor(dir.x * 142) + 640;
        int y0 = (int) floor(dir.y * 142) + 360;
        pane[x0 + y0 * 1280] = 0xFFFF00FF;

        if (tree->right != nullptr) {
            showPhotons(pane, tree->right);
        }
        if (tree->left != nullptr) {
            showPhotons(pane, tree->left);
        }
    }

    void run() {
        // Seed the random engine with the current epoch tick
        long long time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        rand_engine->seed(time);
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
#define AMBIENT_REFLECTION_CONSTANT 0.03f
#define DIFFUSE_REFLECTION_CONSTANT 0.4f
#define SPECULAR_REFLECTION_CONSTANT 0.3f

#define NUM_PHOTONS 2048
#define LIGHT_POWER 1
#define MAX_PHOTON_RADIUS 1
#define PHOTONS_IN_ESTIMATE 63

#define PHOTON_FLAG_DIRECT 0x8

        // Setup our cornell box
#define NUM_OBJECTS 7
        SceneObject *objects[NUM_OBJECTS];
        objects[0] = new PlaneObject(0, -5, 0, 0xFFDDDDDD, 0.1, 0.0);
        objects[1] = new PlaneObject(0, 5, 0, 0xFFDDDDDD, 0.1, 0.0);
        objects[2] = new PlaneObject(5, 0, 0, 0xFFDD0000, 0.1, 0.0);
        objects[3] = new PlaneObject(-5, 0, 0, 0xFF0000DD, 0.1, 0.0);
        objects[4] = new PlaneObject(0, 0, 10, 0xFFDDDDDD, 0.1, 0.0);
        objects[5] = new SphereObject(2, -3.5, 3, 1.5, 0xFFFFFF00, 0.1, 0.0);
        objects[6] = new SphereObject(-2, -3.5, 5, 1.5, 0xFF00FFFF, 0.0, 1.0);


        // photon mapping
        // Based on "A Practical Guide to Global Illumination using Photon Maps" from Siggraph 2000
        // https://graphics.stanford.edu/courses/cs348b-00/course8.pdf

        std::uniform_real_distribution<double> unif(0, 1);

        Vec3 ray(0, 0, 0);
        Vec3 result(0, 0, 0);
        Vec3 normal(0, 0, 0);
        Vec3 light_source(0, 4.96, 4);
        float nearest = 4096 * 4096;
        SceneObject *nearest_obj = nullptr;
        Vec3 nearest_result(0, 0, 0);
        Vec3 nearest_normal(0, 0, 0);

        // We emit a number of photons from each area of the square light source
        // for each grid sqare of the light we emit 1 photon
        // @TODO: the positions should be quasi-randomized to be fairly uniform but not perfectly uniform

        kdnode *global_tree;
        // @TODO: make seperate tree for caustics
        {
            int photon_index = 0;
            int photon_size = NUM_PHOTONS;
            photon **photons = new photon*[photon_size];
            while (photon_index < photon_size) {
                double x0 = unif(*rand_engine) * 2 - 1;
                double z0 = unif(*rand_engine) * 2 + 3;
                double y0 = 4.95;
                light_source.set(x0, y0, z0);
                // direction based on cosine distribution
                // formula for distribution from https://www.particleincell.com/2015/cosine-distribution/
                double sin_theta = sqrt(unif(*rand_engine));
                double cos_theta = sqrt(1 - sin_theta*sin_theta);
                double psi = unif(*rand_engine) * 6.2831853;
                Vec3 light_dir(sin_theta * cos(psi), -cos_theta, sin_theta * sin(psi));
                light_dir.normalize();
                int bounces = 0;
                while (true) {
                    bounces++;
                    // trace photon

                    nearest = 1024 * 1024;
                    nearest_obj = nullptr;
                    for (int i = 0; i < NUM_OBJECTS; i++) {
                        if (objects[i]->intersect(&light_source, &light_dir, &result, &normal)) {
                            double dist = result.distSquared(&light_source);
                            if (dist < nearest) {
                                nearest = dist;
                                nearest_obj = objects[i];
                                nearest_result.set(result.x, result.y, result.z);
                                nearest_normal.set(normal.x, normal.y, normal.z);
                            }
                        }
                    }
                    if (nearest_obj == nullptr) {
                        break;
                    }
                    // we have a hit time to decide whether to reflect, absorb, or transmit
                    double chance = unif(*rand_engine);
                    if (bounces > 3) {
                        // force an absorption if we've already bounced too many times
                        chance = 1;
                    }
                    // @TODO use the diffuse and specular reflection values for each color band pg. 17
                    // @TODO also support transmission
                    if (chance < nearest_obj->diffuse) {
                        // diffuse reflection
                        light_source.set(nearest_result.x, nearest_result.y, nearest_result.z);
                        double sin_theta = sqrt(unif(*rand_engine));
                        double cos_theta = sqrt(1 - sin_theta*sin_theta);
                        double psi = unif(*rand_engine) * 6.2831853;
                        Vec3 n1(nearest_normal);
                        n1.mul(cos_theta);
                        Vec3 n2(n1.y, -n1.x, n1.z);
                        n2.mul(sin_theta * cos(psi));
                        Vec3 n3 = cross(&n1, &n2);
                        n3.mul(sin_theta * sin(psi));
                        light_dir.set(n1.x + n2.x + n3.x, n1.y + n2.y + n3.y, n1.z + n2.z + n3.z);
                        light_dir.normalize();
                        continue;
                    } else if (chance < nearest_obj->diffuse + nearest_obj->specular) {
                        // specular reflection
                        Vec3 n1(nearest_normal);
                        n1.mul(n1.x * light_dir.x + n1.y * light_dir.y + n1.z * light_dir.z);
                        n1.mul(2);
                        light_source.set(nearest_result.x, nearest_result.y, nearest_result.z);
                        light_dir.set(light_dir.x - n1.x, light_dir.y - n1.y, light_dir.z - n1.z);
                        light_dir.normalize();
                        continue;
                    } else {
                        // absorption
                        photon *next = new photon;
                        next->x = nearest_result.x;
                        next->y = nearest_result.y;
                        next->z = nearest_result.z;
                        next->power[0] = 80;
                        next->power[1] = 255;
                        next->power[2] = 255;
                        next->power[3] = 255;
                        next->dx = light_dir.x;
                        next->dy = light_dir.y;
                        next->dz = light_dir.z;
                        next->bounce = bounces;
                        photons[photon_index++] = next;
                        //printf("photon %.1f %.1f %.1f\n", next->x, next->y, next->z);
                    }
                    break;
                }
            }

            // process photons
            photon **scratch = new photon*[NUM_PHOTONS];
            global_tree = createKDTree(photons, NUM_PHOTONS, scratch, NUM_PHOTONS);
            delete[] scratch;
            delete[] photons;
        }

        // rendering

        Vec3 ray_source(0, 0, -12);
        uint32 *pane = new uint32[1280 * 720];
        Vec3 *light_dir = new Vec3(0, 0, 0);
        SceneObject *exclude = nullptr;
        ray_source.set(0, 0, -12);
        for (int x = 0; x < 1280; x++) {
            ray_source.set(0, 0, -12);
            double x0 = (x - 640) / 64.0 - ray_source.x;
            for (int y = 0; y < 720; y++) {
                ray_source.set(0, 0, -12);
                double y0 = (y - 360) / 64.0 - ray_source.y;
                ray.set(x0, y0, -ray_source.z);
                ray.normalize();
                int bounces = 0;
                exclude = nullptr;
                if (x == 559 && y == 246) {
                    printf("");
                }
                ray_trace: {
                    bounces++;
                    if (bounces > 3) {
                        pane[x + y * 1280] = 0xFF000000;
                        break;
                    }
                    nearest = 1024 * 1024;
                    nearest_obj = nullptr;
                    for (int i = 0; i < NUM_OBJECTS; i++) {
                        if (objects[i] == exclude) {
                            continue;
                        }
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
                    } else if (nearest_result.y > 4.95 && nearest_result.x > -1 && nearest_result.x < 1 && nearest_result.z > 3 && nearest_result.z < 5) {
                        pane[x + y * 1280] = 0xFFFFFFFF;
                    } else {
                        if (nearest_obj->specular > 0.99) {
                            pane[x + y * 1280] = 0xFFFF00FF;
                            //break;
                            Vec3 n1(nearest_normal);
                            n1.mul(n1.x * ray.x + n1.y * ray.y + n1.z * ray.z);
                            n1.mul(2);
                            ray_source.set(nearest_result.x, nearest_result.y, nearest_result.z);
                            ray.set(ray.x - n1.x, ray.y - n1.y, ray.z - n1.z);
                            ray.normalize();
                            exclude = nearest_obj;
                            goto ray_trace;
                        }
                        photon* nearest_photons[PHOTONS_IN_ESTIMATE];
                        double photon_distances[PHOTONS_IN_ESTIMATE];
                        for (int i = 0; i < PHOTONS_IN_ESTIMATE; i++) {
                            nearest_photons[i] = nullptr;
                            photon_distances[i] = 0;
                        }
                        // int find_nearest_photons(photon **nearest, double *distances, int k, int size, Vec3 *target, kdnode *root, double max_dist)
                        int found = find_nearest_photons(nearest_photons, photon_distances, PHOTONS_IN_ESTIMATE, 0, &nearest_result, global_tree, 100);
                        float intensity = 0.0f;
                        if (nearest_photons[0] != nullptr) {
                            double r = photon_distances[0];
                            for (int i = 0; i < found; i++) {
                                photon *ph = nearest_photons[i];
                                if (ph == nullptr) {
                                    break;
                                }
                                double d = -nearest_normal.dot(ph->dx, ph->dy, ph->dz);
                                if (d <= 0) {
                                    continue;
                                }
                                Vec3 dist(nearest_result.x - ph->x, nearest_result.y - ph->y, nearest_result.z - ph->z);
                                dist.mul((dist.x * nearest_normal.x + dist.y * nearest_normal.y + dist.z * nearest_normal.z) / (dist.z * dist.z + dist.z * dist.z + dist.z * dist.z));
                                if (dist.lengthSquared() > 0.1) {
                                     continue;
                                }
                                float filter = (1 - photon_distances[i] / r);
                                filter = filter * filter;
                                intensity += d * (ph->power[0] / 255.0) * filter;
                            }
                            intensity /= (3.141592653589 * 1 * r);
                            if (intensity > 1) {
                                intensity = 1;
                            }
                        }

#define SHADOW_RAY_COUNT 5
                        // calculate direct illumication with a shadow ray
                        int light_count = 0;
                        for (int i = 0; i < SHADOW_RAY_COUNT; i++) {
                            double x0 = unif(*rand_engine) * 2 - 1;
                            double z0 = unif(*rand_engine) * 2 + 3;
                            double y0 = 4.95;
                            light_source.set(x0, y0, z0);
                            ray.set(light_source.x - nearest_result.x, light_source.y - nearest_result.y, light_source.z - nearest_result.z);
                            float max_dist = ray.lengthSquared();
                            ray.normalize();
                            for (int i = 0; i < NUM_OBJECTS; i++) {
                                if (objects[i] == nearest_obj) {
                                    continue;
                                }
                                if (objects[i]->intersect(&nearest_result, &ray, &result, &normal)) {
                                    if (nearest_result.distSquared(&result) > max_dist) {
                                        continue;
                                    }
                                    light_count++;
                                    break;
                                }
                            }
                        }
                        if (light_count < SHADOW_RAY_COUNT) {
                            double d = nearest_normal.dot(&ray);
                            if (d > 0) {
                                intensity += d * 0.12 * (1 - (light_count / (double) SHADOW_RAY_COUNT));
                            }
                        }

                        if (intensity > 1) {
                            intensity = 1;
                        }
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
        }

        //showPhotons(pane, global_tree);

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

