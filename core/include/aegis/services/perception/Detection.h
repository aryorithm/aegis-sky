#pragma once
#include <cstdint>

namespace aegis::core::services {

    struct Detection {
        // Bounding Box in Pixel Coordinates
        float x_min, y_min, x_max, y_max;

        // Confidence Score (0.0 to 1.0)
        float confidence;

        // Class ID (e.g., 0=Drone, 1=Bird, 2=Plane)
        int class_id;

        // Track ID assigned by hardware or previous frame
        uint32_t track_id = 0;
    };
}