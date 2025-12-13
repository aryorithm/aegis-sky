#include "sim/physics/RadarPhysics.h"
#include <glm/gtx/intersect.hpp> // GLM Ray Intersections

namespace aegis::sim::physics {

    RadarReturn RadarPhysics::cast_ray(
        const glm::dvec3& radar_pos, 
        const glm::dvec3& beam_dir, 
        const engine::SimEntity& target
    ) {
        RadarReturn ret = {false, 0, 0, 0, 0, -100.0};

        // 1. Simplified Geometry: Treat drone as a Sphere for Raycasting
        // GLM doesn't have double-precision intersectRaySphere in all versions, 
        // but let's implement the math using GLM vector ops.
        
        glm::dvec3 target_pos = target.get_position();
        double target_radius = 0.3; // 30cm drone

        glm::dvec3 m = radar_pos - target_pos;
        double b = glm::dot(m, beam_dir);
        double c = glm::dot(m, m) - target_radius * target_radius;

        // Exit if ray origin is outside sphere (c > 0) and ray points away from sphere (b > 0)
        if (c > 0.0 && b > 0.0) return ret;

        double discr = b*b - c;

        // A negative discriminant corresponds to no intersection
        if (discr < 0.0) return ret;

        // 2. HIT DETECTED
        ret.detected = true;
        ret.range = glm::distance(radar_pos, target_pos);

        // 3. Calculate Angles (Azimuth/Elevation)
        glm::dvec3 to_target = glm::normalize(target_pos - radar_pos);
        ret.azimuth = atan2(to_target.x, to_target.z);
        ret.elevation = asin(to_target.y);

        // 4. Calculate Doppler Velocity
        // Project target velocity onto the "Line of Sight" vector
        // Formula: v_radial = dot(v_target, normalize(pos_target - pos_radar))
        glm::dvec3 los_vector = glm::normalize(target_pos - radar_pos);
        ret.velocity = glm::dot(target.get_velocity(), los_vector);

        // 5. Calculate SNR (Radar Equation simplified)
        // Power drops with 1/R^4
        double r4 = ret.range * ret.range * ret.range * ret.range;
        double rcs = 0.01; // Drone Radar Cross Section (m^2)
        double tx_power = 1000.0; 
        
        double power_received = (tx_power * rcs) / (r4 + 1e-6);
        ret.snr_db = 10.0 * log10(power_received);

        return ret;
    }

}