#pragma once

#include "Vector.h"

namespace raytrace {

    // An abstract class for an object in the scene
    class SceneObject {
    public:

        virtual bool intersect(Vec3 *ray_source, Vec3 *ray, Vec3 *result, Vec3 *result_normal) = 0;

        double x, y, z;
        float red, green, blue;

        double absorb_chance;
        double diffuse_chance;
        double specular_chance;
        double transmission_chance;
        double refraction;
        double specular_coeff;
    };

    class Scene {
    public:
        Scene(int32 num_objects);
        ~Scene();

        int32 size;
        SceneObject **objects;

        void intersect(Vec3 &ray_source, Vec3 &ray, SceneObject *exclude, Vec3 *result, Vec3 *result_normal, SceneObject **hit_object);

    };

    // A spherical object in the scene
    class SphereObject : public SceneObject {
    public:
        SphereObject(double x0, double y0, double z0, double r0, uint32 col, double d, double s, double t, double a);

        bool intersect(Vec3 *ray_source, Vec3 *ray, Vec3 *result, Vec3 *result_normal) override;

        double radius;
    };

    // A planar object, although a litle specialized to create the walls of a cornell box
    class PlaneObject : public SceneObject {
    public:
        // x0,y0,z0 should form a unit vector in the axis of the plane, arbitrary planes not supported
        PlaneObject(double x0, double y0, double z0, double min, double max, uint32 col, double d, double s, double t, double a);

        bool intersect(Vec3 *camera, Vec3 *ray, Vec3 *result, Vec3 *normal) override;

        double min_bound;
        double max_bound;

    };
}