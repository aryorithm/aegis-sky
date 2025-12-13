#include "sim/engine/SimEntity.h"
#include <glm/gtc/epsilon.hpp>

namespace aegis::sim::engine {

    SimEntity::SimEntity(const std::string& name, glm::dvec3 start_pos)
        : name_(name), type_(EntityType::UNKNOWN), position_(start_pos), 
          velocity_(0), rcs_(0.01), speed_(10.0), has_reached_destination_(false) {}

    void SimEntity::add_waypoint(glm::dvec3 wp) {
        waypoints_.push(wp);
    }

    void SimEntity::update(double dt) {
        if (waypoints_.empty()) {
            // Hover logic or Inertia logic could go here
            velocity_ = glm::dvec3(0); 
            return;
        }

        // 1. Get Target
        glm::dvec3 target = waypoints_.front();
        glm::dvec3 to_target = target - position_;
        double dist = glm::length(to_target);

        // 2. Check if reached (within 1 meter)
        if (dist < 1.0) {
            waypoints_.pop(); // Remove waypoint
            if (waypoints_.empty()) {
                has_reached_destination_ = true;
                return;
            }
            // Recalculate for next waypoint
            target = waypoints_.front();
            to_target = target - position_;
            dist = glm::length(to_target);
        }

        // 3. Move towards target
        glm::dvec3 direction = glm::normalize(to_target);
        
        // Simple kinematics: Velocity = Dir * Speed
        velocity_ = direction * speed_;
        
        // Update Position
        position_ += velocity_ * dt;
    }
}