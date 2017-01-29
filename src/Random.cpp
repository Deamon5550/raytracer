
#include "Random.h"

#include <random>

namespace random {

    std::default_random_engine *rand_engine = new std::default_random_engine;
    std::uniform_real_distribution<double> *real_dist;

    void init(int64 seed) {
        int32 s = (int32) ((((uint64) seed) >> 32) ^ seed);
        rand_engine->seed(s);
        real_dist = new std::uniform_real_distribution<double>(0, 1);
    }

    int32 nextInt(int32 min, int32 max) {
        std::uniform_int_distribution<int> unif(min, max - 1);
        return unif(*rand_engine);
    }

    double nextDouble() {
        return (*real_dist)(*rand_engine);
    }

}