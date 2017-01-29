
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
    if (argc < 2) {
        printf("Usage: ./raytracer [# cores]");
        return 0;
    }
    int cores = atoi(argv[1]);
    if (cores < 1) {
        cores = 1;
    }
    scheduler::startWorkers(cores);
#ifdef OUTPUT_IMAGE
    raytrace::render("raytraced.png");
#else
    raytrace::run();
#endif
    return 0;
}
