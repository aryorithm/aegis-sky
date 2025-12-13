#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "sim/engine/SimEntity.h"

namespace aegis::sim::physics {

    struct RadarReturn {
        bool detected;
        double range;       // Distance (meters)
        double azimuth;     // Horizontal Angle (radians)
        double elevation;   // Vertical Angle (radians)
        double velocity;    // Radial Velocity (m/s) - Doppler
        double snr_db;      // Signal Strength
    };

    class RadarPhysics {
    public:
        // Simulates a single radar beam interacting with a target
        static RadarReturn cast_ray(
            const glm::dvec3& radar_pos, 
            const glm::dvec3& beam_dir, 
            const engine::SimEntity& target
        );
    };
}