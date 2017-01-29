#include <cstdio>

#include "Render.h"
#include "JobSystem.h"

int main(int argc, char *argv[]) {
    scheduler::startWorkers(8);
    raytrace::run();
    return 0;
}
