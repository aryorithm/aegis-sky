#pragma once
#include <glm/glm.hpp>

namespace aegis::sim::physics {

    class GimbalPhysics {
    public:
        GimbalPhysics();

        // Updates orientation based on velocity commands
        void update(double dt, float cmd_pan_vel, float cmd_tilt_vel);

        // Getters for the Renderer/Radar
        glm::dvec3 get_forward_vector() const;
        float get_current_pan() const { return current_pan_; }

    private:
        // State
        float current_pan_;  // Radians (0 = North)
        float current_tilt_; // Radians (0 = Horizon)

        // Physical Constraints (FLIR PTU-D48 specs)
        const float MAX_VEL = 2.0f; // rad/s (~120 deg/s)
        const float MAX_ACCEL = 5.0f;
        const float MIN_TILT = -0.5f; // -30 deg
        const float MAX_TILT = 1.5f;  // +90 deg
    };
}