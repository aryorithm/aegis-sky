#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <queue>

namespace aegis::sim::engine {

    enum class EntityType { QUADCOPTER, FIXED_WING, BIRD, UNKNOWN };

    struct MicroDopplerProfile {
        double blade_speed_mps = 0.0; // Tip speed
        double blade_rate_hz = 0.0;   // Rotation rate
        bool is_flapping = false;     // Bird vs Rotor
    };

    class SimEntity {
    public:
        SimEntity(const std::string& name, glm::dvec3 start_pos);
        virtual ~SimEntity() = default;

        void update(double dt);

        // Configuration
        void set_type(EntityType t) { type_ = t; }
        void set_rcs(double rcs) { rcs_ = rcs; }
        void set_speed(double s) { speed_ = s; }
        void set_temperature(double c) { temperature_k_ = c + 273.15; }
        void set_velocity(glm::dvec3 v) { velocity_ = v; }
        void set_position(glm::dvec3 p) { position_ = p; }
        
        // Micro-Doppler Config
        void set_micro_doppler(double speed, double hz, bool flap);

        // Pathing
        void add_waypoint(glm::dvec3 wp);

        // Getters
        glm::dvec3 get_position() const { return position_; }
        glm::dvec3 get_velocity() const { return velocity_; }
        double get_rcs() const { return rcs_; }
        double get_temperature() const { return temperature_k_; }
        std::string get_name() const { return name_; }

        // Physics Calculation
        double get_instant_doppler_mod(double time) const;

    private:
        std::string name_;
        EntityType type_;
        
        glm::dvec3 position_;
        glm::dvec3 velocity_;
        
        double rcs_ = 0.01;
        double speed_ = 10.0;
        double temperature_k_ = 300.0;
        
        MicroDopplerProfile micro_doppler_;

        std::queue<glm::dvec3> waypoints_;
    };
}