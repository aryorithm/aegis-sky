#include "sim/physics/RadarPhysics.h"
#include <glm/gtx/norm.hpp> // For length2
#include <cmath>
#include <algorithm>

namespace aegis::sim::physics {

    // -------------------------------------------------------------------------
    // RADAR CONSTANTS (Approximation of a 4D AESA Radar)
    // -------------------------------------------------------------------------
    static constexpr double TX_POWER_WATTS = 200.0;     // Transmit Power
    static constexpr double SYSTEM_LOSS = 0.5;          // System efficiency
    static constexpr double NOISE_FLOOR_WATTS = 1e-13;  // Thermal noise floor
    
    // For Hitbox detection (Simplified Sphere)
    static constexpr double HITBOX_RADIUS = 0.5;        // Meters

    RadarReturn RadarPhysics::cast_ray(
        const glm::dvec3& radar_pos, 
        const glm::dvec3& beam_dir, 
        const engine::SimEntity& target
    ) {
        RadarReturn ret = {false, 0.0, 0.0, 0.0, 0.0, -100.0};

        // ---------------------------------------------------------------------
        // 1. GEOMETRY: Ray-Sphere Intersection
        // ---------------------------------------------------------------------
        // Vector from Radar (Origin) to Target Center
        glm::dvec3 L = target.get_position() - radar_pos;
        
        // Project L onto the beam direction (How far along the ray is the closest point?)
        double t_closest = glm::dot(L, beam_dir);

        // If t_closest < 0, the target is behind the radar
        if (t_closest < 0.0) return ret;

        // Distance squared from Target Center to the Ray Line
        double d2 = glm::length2(L) - (t_closest * t_closest);
        double r2 = HITBOX_RADIUS * HITBOX_RADIUS;

        // If distance squared > radius squared, the ray missed the target
        if (d2 > r2) return ret;

        // ---------------------------------------------------------------------
        // 2. HIT CONFIRMED - Calculate Polar Coordinates
        // ---------------------------------------------------------------------
        ret.detected = true;
        
        // Exact distance to the surface of the target
        double dist_to_surface = sqrt(r2 - d2);
        ret.range = t_closest - dist_to_surface;

        // Sanity check (don't detect things inside the radar)
        if (ret.range < 1.0) { ret.detected = false; return ret; }

        // Calculate Relative Vector for Angles
        glm::dvec3 to_target = glm::normalize(target.get_position() - radar_pos);
        
        // Azimuth (Horizontal Angle): atan2(x, z) assumes Forward is Z+
        ret.azimuth = std::atan2(to_target.x, to_target.z);
        
        // Elevation (Vertical Angle): arcsin(y)
        ret.elevation = std::asin(to_target.y);

        // ---------------------------------------------------------------------
        // 3. DOPPLER PHYSICS
        // ---------------------------------------------------------------------
        // Radial Velocity is the projection of Target Velocity onto the Line-of-Sight
        // v_radial = dot(v_target, normalize(pos_target - pos_radar))
        // Positive = Moving Away, Negative = Closing In
        ret.velocity = glm::dot(target.get_velocity(), to_target);

        // ---------------------------------------------------------------------
        // 4. SIGNAL STRENGTH (The Radar Equation)
        // ---------------------------------------------------------------------
        // Pr = (Pt * G^2 * lambda^2 * RCS) / ((4*pi)^3 * R^4)
        // Simplified proportionality: Pr ~ (RCS / R^4)
        
        double range_4 = ret.range * ret.range * ret.range * ret.range;
        
        // CRITICAL: Use the entity's specific RCS (Bird vs Drone)
        double rcs = target.get_rcs(); 

        // Calculate received power (Simplified Model)
        double power_received = (TX_POWER_WATTS * rcs) / (range_4 + 1e-9);

        // Convert to Decibels (dB)
        // SNR = 10 * log10(Signal / Noise)
        ret.snr_db = 10.0 * std::log10(power_received / NOISE_FLOOR_WATTS);

        return ret;
    }

}