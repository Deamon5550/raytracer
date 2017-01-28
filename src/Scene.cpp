
#include "Scene.h"

#include <cmath>

namespace raytrace {

    Scene::Scene(int32 num_objects) {
        size = num_objects;
        objects = new SceneObject*[size];
        for (int i = 0; i < size; i++) objects[i] = nullptr;
    }

    Scene::~Scene() {
        for (int i = 0; i < size; i++) {
            if (objects[i] != nullptr) {
                delete objects[i];
            }
        }
        delete[] objects;
    }

    void Scene::intersect(Vec3 &ray_source, Vec3 &ray, SceneObject *exclude, Vec3 *final_result, Vec3 *result_normal, SceneObject **hit_object) {
        double nearest = 1024 * 1024;
        SceneObject *nearest_obj = nullptr;
        Vec3 result(0, 0, 0);
        Vec3 normal(0, 0, 0);
        for (int i = 0; i < size; i++) {
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
                    final_result->set(result.x, result.y, result.z);
                    result_normal->set(normal.x, normal.y, normal.z);
                }
            }
        }
        *hit_object = nearest_obj;
    }

    SphereObject::SphereObject(double x0, double y0, double z0, double r0, uint32 col, double d, double s, double t) {
        x = x0;
        y = y0;
        z = z0;
        radius = r0;
        color = col;
        diffuse_chance = d;
        specular_chance = s;
        transmission_chance = t;
        refraction = 0;
    }

    bool SphereObject::intersect(Vec3 *ray_source, Vec3 *ray, Vec3 *result, Vec3 *result_normal) {
        Vec3 l(x - ray_source->x, y - ray_source->y, z - ray_source->z);
        double b = ray->dot(&l);
        if (b < 0) {
            return false;
        }
        double d2 = l.dot(&l) - b  * b;
        if (d2 > radius * radius) {
            return false;
        }
        double t = std::sqrt(radius * radius - d2);
        if (t < 0) {
            t = b + t;
        } else {
            t = b - t;
        }
        if (t < 0) {
            return false;
        }
        result->set(ray->x * t + ray_source->x, ray->y * t + ray_source->y, ray->z * t + ray_source->z);
        result_normal->set(result->x - x, result->y - y, result->z - z);
        result_normal->normalize();
        return true;
    }

    PlaneObject::PlaneObject(double x0, double y0, double z0, double min, double max, uint32 col, double d, double s, double t) {
        x = x0;
        y = y0;
        z = z0;
        color = col;
        diffuse_chance = d;
        specular_chance = s;
        transmission_chance = t;
        refraction = 0;
        max_bound = max;
        min_bound = min;
    }

    bool PlaneObject::intersect(Vec3 *camera, Vec3 *ray, Vec3 *result, Vec3 *normal) {
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
            if (hy > max_bound || hy < min_bound) {
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
            if (hx < min_bound || hx > max_bound) {
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
            if (hy < min_bound || hy > max_bound) {
                return false;
            }
            result->set(hx, hy, z);
            normal->set(0, 0, z < 0 ? 1 : -1);
            return true;
        }
        return false;
    }
}