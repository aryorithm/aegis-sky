#pragma once
#include <glm/glm.hpp>

namespace aegis::sim::engine {
    struct Projectile {
        glm::dvec3 position;
        glm::dvec3 velocity;
        bool active = true;
        double time_alive = 0.0;
        
        // Physics
        static constexpr double MUZZLE_VELOCITY = 800.0; // m/s (30mm Cannon)
        static constexpr double GRAVITY = -9.81;
    };
}