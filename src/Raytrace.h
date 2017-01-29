#pragma once

#include "Vector.h"
#include "Scene.h"
#include "Random.h"
#include "PhotonMap.h"

namespace raytrace {

    void renderScene(Scene *scene, Vec3 &camera, uint32 *pane, int32 width, int32 height);

}