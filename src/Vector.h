#pragma once

typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;

namespace raytrace {

    int32 fastfloor(float value);
    int32 fastfloor(double value);
    int32 min(int32 a, int32 b);

    // A simple 3d vector of 3 doubles
    class Vec3 {
    public:
        Vec3(double x0, double y0, double z0);

        void set(double x0, double y0, double z0);
        void set(Vec3 *o);
        void add(double x0, double y0, double z0);
        void add(Vec3 *o);
        void sub(double x0, double y0, double z0);
        void sub(Vec3 *o);
        void mul(double x0);
        void mul(double x0, double y0, double z0);

        double lengthSquared();
        double length();
        void normalize();

        double dot(Vec3 *o);
        double dot(double ox, double oy, double oz);

        double distSquared(Vec3 *o);
        double distSquared(float ox, float oy, float oz);

        double x, y, z;

    };

    Vec3 cross(Vec3 *a, Vec3 *b);

    enum Axis {
        X_AXIS,
        Y_AXIS,
        Z_AXIS,
    };

    Axis largestAxis(double x, double y, double z);

}