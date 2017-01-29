#pragma once

#include "Scene.h"
#include "Vector.h"

namespace raytrace {

    void render(const char *image_file, int32 width, int32 height, int32 samples);

}