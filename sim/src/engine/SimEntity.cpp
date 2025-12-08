#include "sim/engine/SimEntity.h"
#include <cmath>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace aegis::sim::engine {

    SimEntity::SimEntity(const std::string& name, glm::dvec3 start_pos)
        : name_(name), type_(EntityType::UNKNOWN), position_(start_pos), velocity_(0) {}

    void SimEntity::set_micro_doppler(double speed, double hz, bool flap) {
        micro_doppler_ = {speed, hz, flap};
    }

    void SimEntity::add_waypoint(glm::dvec3 wp) {
        waypoints_.push(wp);
    }

    void SimEntity::destroy() {
        if (!is_destroyed_) {
            is_destroyed_ = true;
            // Visual/Physics effect of destruction
            velocity_ = glm::dvec3(0, -9.81, 0); // Freefall
            temperature_k_ += 500.0; // Explosion heat spike
        }
    }

    void SimEntity::apply_thermal_damage(double joules) {
        if (is_destroyed_) return;
        
        thermal_health_ -= joules;
        // Heat up the drone as it takes damage
        temperature_k_ += (joules * 0.05); 

        if (thermal_health_ <= 0) {
            destroy();
        }
    }

    void SimEntity::update(double dt) {
        if (is_destroyed_) {
            // Dead entities just fall
            velocity_.y += -9.81 * dt;
            position_ += velocity_ * dt;
            if (position_.y < 0) position_.y = 0; // Hit ground
            return;
        }

        // Standard Waypoint Following (if not controlled by Swarm physics externally)
        if (swarm_id_ == -1 && !waypoints_.empty()) {
            glm::dvec3 target = waypoints_.front();
            glm::dvec3 dir = target - position_;
            double dist = glm::length(dir);

            if (dist < 2.0) {
                waypoints_.pop();
            } else {
                // Smooth turn logic could go here
                velocity_ = glm::normalize(dir) * max_speed_;
            }
        }
        
        // Integration
        position_ += velocity_ * dt;
    }

    double SimEntity::get_instant_doppler_mod(double time) const {
        if (micro_doppler_.blade_speed_mps <= 0.0) return 0.0;

        // Phase = Time * Frequency
        double phase = time * micro_doppler_.blade_rate_hz * 2.0 * M_PI;

        if (micro_doppler_.is_flapping) {
            // Biological: High amplitude, low frequency
            return std::sin(phase) * 2.0; 
        } else {
            // Mechanical: "Blade Flash" effect
            // Returns radial velocity component of blade tip relative to radar
            return std::sin(phase) * micro_doppler_.blade_speed_mps * 0.15;
        }
    }
}