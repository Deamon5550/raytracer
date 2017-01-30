
#include "Random.h"

#include <random>

namespace randutil {

    std::default_random_engine *rand_engine = new std::default_random_engine;
    std::uniform_real_distribution<double> *real_dist;

    // sets up the random source with the given seed
    void init(int64 seed) {
        int32 s = (int32) ((((uint64) seed) >> 32) ^ seed);
        rand_engine->seed(s);
        real_dist = new std::uniform_real_distribution<double>(0, 1);
    }

    // gets an integer in the range of 0 (inclusive) and max (exclusive)
    int32 nextInt(int32 min, int32 max) {
        std::uniform_int_distribution<int> unif(min, max - 1);
        return unif(*rand_engine);
    }

    // gets a double in the range of 0 and 1
    double nextDouble() {
        return (*real_dist)(*rand_engine);
    }

}