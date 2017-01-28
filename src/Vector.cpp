
#include "Vector.h"

#include <cmath>

namespace raytrace {

    int32 fastfloor(float x) {
        int32 xi = (int32) x;
        return x < xi ? xi - 1 : xi;
    }

    Vec3 cross(Vec3 *a, Vec3 *b) {
        Vec3 result(a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x);
        return result;
    }

    Vec3::Vec3(double x0, double y0, double z0) {
        x = x0;
        y = y0;
        z = z0;
    }

    void Vec3::set(double x0, double y0, double z0) {
        x = x0;
        y = y0;
        z = z0;
    }

    void Vec3::set(Vec3 *o) {
        x = o->x;
        y = o->y;
        z = o->z;
    }

    void Vec3::add(double x0, double y0, double z0) {
        x += x0;
        y += y0;
        z += z0;
    }

    void Vec3::add(Vec3 *o) {
        x += o->x;
        y += o->y;
        z += o->z;
    }

    void Vec3::sub(double x0, double y0, double z0) {
        x -= x0;
        y -= y0;
        z -= z0;
    }

    void Vec3::sub(Vec3 *o) {
        x -= o->x;
        y -= o->y;
        z -= o->z;
    }

    void Vec3::mul(double x0) {
        x *= x0;
        y *= x0;
        z *= x0;
    }

    double Vec3::lengthSquared() {
        return x * x + y * y + z * z;
    }

    double Vec3::length() {
        return std::sqrt(x * x + y * y + z * z);
    }

    void Vec3::normalize() {
        double len = length();
        x /= len;
        y /= len;
        z /= len;
    }

    double Vec3::dot(Vec3 *o) {
        return x * o->x + y * o->y + z * o->z;
    }

    double Vec3::dot(double ox, double oy, double oz) {
        return x * ox + y * oy + z * oz;
    }

    double Vec3::distSquared(Vec3 *o) {
        return (x - o->x) * (x - o->x) + (y - o->y) * (y - o->y) + (z - o->z) * (z - o->z);
    }

    double Vec3::distSquared(float ox, float oy, float oz) {
        return (x - ox) * (x - ox) + (y - oy) * (y - oy) + (z - oz) * (z - oz);
    }

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
}