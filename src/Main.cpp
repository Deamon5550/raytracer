#include <cstdio>

#include "Render.h"
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
    raytrace::run();
    return 0;
}
