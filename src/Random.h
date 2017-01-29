#pragma once

#include "Vector.h"

namespace randutil {

    void init(int64 seed);

    int nextInt(int32 min, int32 max);

    double nextDouble();

}