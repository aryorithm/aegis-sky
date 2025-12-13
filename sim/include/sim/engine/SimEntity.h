#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <queue>

namespace aegis::sim::engine {


    
    enum class EntityType {
        QUADCOPTER,
        FIXED_WING,
        BIRD,
        UNKNOWN
    };

    class SimEntity {
    public:
        SimEntity(const std::string& name, glm::dvec3 start_pos);
        virtual ~SimEntity() = default;

        // Updates position based on Waypoints and Speed
        void update(double dt);

        // Configuration Setters
        void set_type(EntityType t) { type_ = t; }
        void set_rcs(double rcs) { rcs_ = rcs; }
        void set_speed(double m_s) { speed_ = m_s; }
        void add_waypoint(glm::dvec3 wp);

        void set_temperature(double temp_c) { temperature_k_ = temp_c + 273.15; }
double get_temperature() const { return temperature_k_; }
        // Getters
        glm::dvec3 get_position() const { return position_; }
        glm::dvec3 get_velocity() const { return velocity_; }
        double get_rcs() const { return rcs_; }

    private:
        std::string name_;
        EntityType type_;
        
        // Physics State
        glm::dvec3 position_;
        glm::dvec3 velocity_;
        double rcs_;   // Radar Cross Section
        double speed_; // Max speed

        // AI Pathing
        std::queue<glm::dvec3> waypoints_;
        bool has_reached_destination_;
    };
}