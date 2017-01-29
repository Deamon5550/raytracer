
#include "Raytrace.h"

#include <cstdio>
#include <chrono>

#include "JobSystem.h"

#define NUM_PHOTONS 2048
#define LIGHT_POWER 1
#define MAX_PHOTON_RADIUS 100
#define PHOTONS_IN_ESTIMATE 63

#define CAUSTIC_PHOTONS 2048
#define CAUSTIC_PHOTONS_IN_ESTIMATE 63

//#define USE_SHADOW_RAYS
#define SHADOW_RAY_COUNT 5

namespace raytrace {

    uint32 traceRay(Vec3 &ray_source, Vec3 &ray, Scene *scene, SceneObject *exclude, int bounce, kdnode *global_tree, kdnode *caustic_tree) {
        if (bounce > 3) {
            return 0xFF000000;
        }
        Vec3 nearest_result(0, 0, 0);
        Vec3 nearest_normal(0, 0, 0);
        SceneObject *nearest_obj = nullptr;
        scene->intersect(ray_source, ray, exclude, &nearest_result, &nearest_normal, &nearest_obj);
        if (nearest_obj == nullptr) {
            return 0xFF000000;
        } else if (nearest_result.y > 4.95 && nearest_result.x > -1 && nearest_result.x < 1 && nearest_result.z > 3 && nearest_result.z < 5) {
            return 0xFFFFFFFF;
        } else {
            double chance = random::nextDouble();
            if (chance < nearest_obj->transmission_chance) {
                // refract
                // Equation from Fundamentals of Computer Graphics 4th edition p 325.
                double n = 1 / nearest_obj->refraction;
                double d = nearest_normal.x * ray.x + nearest_normal.y * ray.y + nearest_normal.z * ray.z;
                Vec3 n1(nearest_normal);
                n1.mul(d);
                n1.set(ray.x - n1.x, ray.y - n1.y, ray.z - n1.z);
                n1.mul(n);
                Vec3 n2(nearest_normal);
                double s = 1 - (n * n) * (1 - d * d);
                n2.mul(sqrt(s));
                n1.add(-n2.x, -n2.y, -n2.z);
                // n1 is refracted vector
                // we should be able to step a tiny part along our refracted ray to avoid
                // having to exclude the object we just hit allowing us to hit the otherside
                ray_source.set(nearest_result.x + n1.x * 0.01, nearest_result.y + n1.y * 0.01, nearest_result.z + n1.z * 0.01);
                ray.set(n1.x, n1.y, n1.z);
                ray.normalize();
                return traceRay(ray_source, ray, scene, nullptr, bounce + 1, global_tree, caustic_tree);
            } else if (chance < nearest_obj->transmission_chance + nearest_obj->specular_chance) {
                // calculate reflection angle
                Vec3 n1(nearest_normal);
                n1.mul(n1.x * ray.x + n1.y * ray.y + n1.z * ray.z);
                n1.mul(2);
                ray_source.set(nearest_result.x, nearest_result.y, nearest_result.z);
                ray.set(ray.x - n1.x, ray.y - n1.y, ray.z - n1.z);
                ray.normalize();
                // continue trace
                return traceRay(ray_source, ray, scene, nearest_obj, bounce + 1, global_tree, caustic_tree);
            }
            photon* nearest_photons[PHOTONS_IN_ESTIMATE];
            double photon_distances[PHOTONS_IN_ESTIMATE];
            for (int i = 0; i < PHOTONS_IN_ESTIMATE; i++) {
                nearest_photons[i] = nullptr;
                photon_distances[i] = 0;
            }
            float intensity = 0.0f;
            {
                int found = find_nearest_photons(nearest_photons, photon_distances, PHOTONS_IN_ESTIMATE, 0, &nearest_result, global_tree, MAX_PHOTON_RADIUS);
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
            }
            { // caustics
                float caustic_contribution = 0;
                int found = find_nearest_photons(nearest_photons, photon_distances, CAUSTIC_PHOTONS_IN_ESTIMATE, 0, &nearest_result, caustic_tree, 100);
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
                        filter = filter * filter * filter * filter;
                        caustic_contribution += d * filter;
                    }
                    caustic_contribution /= (3.141592653589 * 4 * r);
                    intensity += caustic_contribution;
                    if (intensity > 1) {
                        intensity = 1;
                    }
                }
            }

#if USE_SHADOW_RAYS
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
#endif

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
            return (alpha << 24) | (red << 16) | (green << 8) | blue;
        }
    }

    struct render_task_data {
        int32 y;
        int32 width;
        int32 height;
        uint32 *pane;
        Scene *scene;
        kdnode *global_tree;
        kdnode *caustic_tree;
    };

    void render_task(void *vdata) {
        render_task_data *data = (render_task_data*) vdata;
        Vec3 ray(0, 0, 0);
        Vec3 ray_source(0, 0, -12);
        Vec3 *light_dir = new Vec3(0, 0, 0);
        SceneObject *exclude = nullptr;
        ray_source.set(0, 0, -12);
        double last_progress = 0;
        for (int32 x = 0; x < data->width; x++) {
            ray_source.set(0, 0, -12);
            double x0 = (x - data->width / 2) / 64.0 - ray_source.x;
            double y0 = (data->y - data->height / 2) / 64.0 - ray_source.y;
            ray.set(x0, y0, -ray_source.z);
            ray.normalize();
            data->pane[x + data->y *data->width] = traceRay(ray_source, ray, data->scene, nullptr, 0, data->global_tree, data->caustic_tree);
        }
    }

    void renderScene(Scene *scene, uint32 *pane, int32 width, int32 height) {
        // photon mapping
        // Based on "A Practical Guide to Global Illumination using Photon Maps" from Siggraph 2000
        // https://graphics.stanford.edu/courses/cs348b-00/course8.pdf

        Vec3 ray(0, 0, 0);
        Vec3 light_source(0, 4.96, 4);
        SceneObject *nearest_obj = nullptr;
        Vec3 nearest_result(0, 0, 0);
        Vec3 nearest_normal(0, 0, 0);

        kdnode *global_tree = createPhotonMap(NUM_PHOTONS, light_source, scene);
        kdnode *caustic_tree = createCausticPhotonMap(CAUSTIC_PHOTONS, light_source, scene);

        // rendering
        printf("Rendering scene\n");
        auto start = std::chrono::high_resolution_clock::now();


        for (int32 y = 0; y < height; y++) {
            render_task_data *data = new render_task_data;
            data->y = y;
            data->width = width;
            data->height = height;
            data->pane = pane;
            data->scene = scene;
            data->global_tree = global_tree;
            data->caustic_tree = caustic_tree;
            scheduler::submit(render_task, data);
        }

        scheduler::waitForCompletion();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        printf("Scene rendered in %.3fs\n", duration.count());

        deleteTree(global_tree);
        deleteTree(caustic_tree);
    }

}