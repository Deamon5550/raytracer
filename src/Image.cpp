
#include "Image.h"

#include <cstdio>
#include <chrono>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "Raytrace.h"

namespace raytrace {

    void render(const char *image_file, int32 width, int32 height, int32 sample_ratio) {
        // Seed the random engine with the current epoch tick
        int64 time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        randutil::init(time);
        printf("Setting up scene\n");
        // Setup our cornell box
        Scene *scene = new Scene(7);
        scene->objects[0] = new PlaneObject(0, -5, 0, -5, 5, 0xFFEEEEEE, 0.4, 0.0, 0.0, 1.0);
        scene->objects[1] = new PlaneObject(0, 5, 0, -5, 5, 0xFFDEEEEEE, 0.4, 0.0, 0.0, 1.0);
        scene->objects[2] = new PlaneObject(5, 0, 0, -5, 5, 0xFFFF3333, 0.4, 0.0, 0.0, 1.0);
        scene->objects[3] = new PlaneObject(-5, 0, 0, -5, 5, 0xFF3333FF, 0.4, 0.0, 0.0, 1.0);
        scene->objects[4] = new PlaneObject(0, 0, 10, -5, 5, 0xFFEEEEEE, 0.4, 0.0, 0.0, 1.0);
        scene->objects[5] = new SphereObject(2, -3.5, 3, 1.5, 0xFFFFFFFF, 0.0, 0.1, 0.9, 0.0);
        scene->objects[5]->refraction = 2.5;
        scene->objects[6] = new SphereObject(-2, -3.5, 5, 1.5, 0xFFFFFFFF, 0.0, 1.0, 0.0, 0.0);
        //scene->objects[7] = new SphereObject(0, 1, 5, 0.8, 0xFF33FF33, 0.4, 0.0, 0.0, 1.0, 0.2, 0, 0);

        uint32 *pane = new uint32[width * height * sample_ratio * sample_ratio];
        Vec3 camera(0, 0, -12);
        raytrace::renderScene(scene, camera, pane, width * sample_ratio, height * sample_ratio);

        uint32 *sample = new uint32[width * height];
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                int32 red = 0;
                int32 green = 0;
                int32 blue = 0;
                for (int x0 = 0; x0 < sample_ratio; x0++) {
                    for (int y0 = 0; y0 < sample_ratio; y0++) {
                        int32 s = pane[x * sample_ratio + x0 + (y * sample_ratio + y0) * width * sample_ratio];
                        red += (s >> 16) & 0xFF;
                        green += (s >> 8) & 0xFF;
                        blue += s & 0xFF;
                    }
                }
                red /= sample_ratio * sample_ratio;
                green /= sample_ratio * sample_ratio;
                blue /= sample_ratio * sample_ratio;
                sample[x + (height - y - 1) * width] = (0xFF << 24) | (red << 16) | (green << 8) | blue;
            }
        }

        // Cleanup
        delete scene;
        delete[] pane;

        stbi_write_png(image_file, width, height, 4, sample, width * 4);
#ifdef _WIN32
        system(image_file);
#else
        char cmd[200];
        strcpy(cmd, "open ");
        strcat(cmd, image_file);
        system(cmd);
#endif

        delete[] sample;

    }

}