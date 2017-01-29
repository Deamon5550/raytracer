#pragma once

#include "Vector.h"
#include "Scene.h"

namespace raytrace {

    struct photon {
        float x, y, z;
        uint8 power[4];
        float dx, dy, dz;
        int8 bounce;
    };

    struct kdnode {
        photon *value;
        kdnode *left;
        kdnode *right;
        Axis splitting_axis;
    };

    photon *findMedianPhoton(photon** photons, int size, Axis axis);
    kdnode *createKDTree(photon **photons, int size, photon **scratch, int scratch_size);
    void insert(photon **nearest, double *distances, int k, int size, photon *next, double dist);
    int find_nearest_photons(photon **nearest, double *distances, int k, int size, Vec3 *target, kdnode *root, double max_dist);
    void showPhotons(uint32 *pane, kdnode *tree);

    kdnode *createPhotonMap(int32 photon_count, Vec3 &light_source, Vec3 &light_color, Scene *scene);
    kdnode *createCausticPhotonMap(int32 photon_count, Vec3 &light_source, Vec3 &light_color, Scene *scene);

    void deleteTree(kdnode *tree);
}