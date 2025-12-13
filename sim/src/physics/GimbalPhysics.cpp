#include "sim/physics/GimbalPhysics.h"
#include <algorithm>
#include <cmath>

namespace aegis::sim::physics {

    GimbalPhysics::GimbalPhysics() : current_pan_(0.0f), current_tilt_(0.0f) {}

    void GimbalPhysics::update(double dt, float cmd_pan_vel, float cmd_tilt_vel) {
        // 1. Clamp Velocity (Motor Limits)
        float v_pan = std::clamp(cmd_pan_vel, -MAX_VEL, MAX_VEL);
        float v_tilt = std::clamp(cmd_tilt_vel, -MAX_VEL, MAX_VEL);

        // 2. Integrate Position (New Angle = Old Angle + Vel * Time)
        current_pan_ += v_pan * static_cast<float>(dt);
        current_tilt_ += v_tilt * static_cast<float>(dt);

        // 3. Hard Stops (Mechanical Limits)
        current_tilt_ = std::clamp(current_tilt_, MIN_TILT, MAX_TILT);
        
        // Pan is continuous (wrap around PI)
        if (current_pan_ > M_PI) current_pan_ -= 2 * M_PI;
        if (current_pan_ < -M_PI) current_pan_ += 2 * M_PI;
    }

    glm::dvec3 GimbalPhysics::get_forward_vector() const {
        // Convert Spherical (Pan/Tilt) to Cartesian (XYZ)
        // Z+ is North (0 Pan)
        double x = std::sin(current_pan_) * std::cos(current_tilt_);
        double y = std::sin(current_tilt_);
        double z = std::cos(current_pan_) * std::cos(current_tilt_);
        
        return glm::normalize(glm::dvec3(x, y, z));
    }
}