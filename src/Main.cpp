
#include <cstdio>
#include <cstdlib>

#define OUTPUT_IMAGE

#ifdef OUTPUT_IMAGE
#include "Image.h"
#else
#include "Render.h"
#endif
#include "JobSystem.h"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: ./raytracer [# cores] [width] [height] [samples]\n");
        return 0;
    }
    int cores = atoi(argv[1]);
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    int samples = atoi(argv[4]);
    if (cores < 1) {
        cores = 1;
    }
    if (width / 16 != height / 9) {
        printf("Image dimensions must be a 16:9 ratio.\n");
        return 0;
    }
    if (samples < 0 || width < 0 || height < 0) {
        printf("Dimensions and samples must be positive\n");
        return 0;
    }
    scheduler::startWorkers(cores);
#ifdef OUTPUT_IMAGE
    raytrace::render("raytraced.png", width, height, samples);
#else
    raytrace::run();
#endif
    return 0;
}
