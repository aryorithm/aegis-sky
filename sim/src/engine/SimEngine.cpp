#include "sim/engine/SimEngine.h"

#include "sim/engine/ScenarioLoader.h"
#include "sim/bridge_server/ShmWriter.h"
#include "sim/phenomenology/optics/MockRenderer.h"
#include "sim/physics/RadarPhysics.h"
#include "sim/physics/GimbalPhysics.h"
#include "sim/engine/Environment.h"
#include "sim/engine/WeatherSystem.h"
#include "sim/math/Random.h" 

#include <spdlog/spdlog.h>
#include <thread>
#include <cmath>

namespace aegis::sim::engine {

    // Simple Projectile Struct (Kinetic Interceptor)
    struct Projectile {
        glm::dvec3 pos;
        glm::dvec3 vel;
        bool active = true;
        double age = 0.0;
    };

    SimEngine::SimEngine() : is_running_(false), is_headless_(false) {
        math::Random::init();

        bridge_ = std::make_unique<bridge::ShmWriter>();
        renderer_ = std::make_unique<phenomenology::MockRenderer>(1920, 1080);
        environment_ = std::make_unique<Environment>();
        
        // Add "Warehouse" Obstacle
        environment_->add_building(glm::dvec3(0, 15, 200), glm::dvec3(60, 30, 20));

        // Physics Configs
        drone_phys_config_ = {1.2, 0.3, 30.0};
        radar_config_ = {120.0, 30.0, 2000.0, 0.5, 0.01, 0.2};
        global_wind_ = glm::dvec3(3.0, 0.0, 1.5);
    }

    SimEngine::~SimEngine() { if (bridge_) bridge_->cleanup(); }

    void SimEngine::initialize(const std::string& scenario_path) {
        entities_ = ScenarioLoader::load_mission(scenario_path);
        if (entities_.empty()) throw std::runtime_error("Empty Mission.");
        if (!bridge_->initialize()) throw std::runtime_error("Bridge Failed.");
        
        is_running_ = true;
        spdlog::info("[Sim] Engine Online. Occlusion: ON. EW: ON.");
    }

    void SimEngine::run() {
        std::vector<Projectile> projectiles;
        glm::dvec3 sensor_pos(0,0,0);

        while (is_running_) {
            // --- 0. TIME & WEATHER ---
            time_manager_.tick();
            double dt = time_manager_.get_delta_time();
            double now = time_manager_.get_total_time();
            uint64_t frame = time_manager_.get_frame_count();

            // Dynamic Storm (Example)
            if (now > 10.0) weather_.set_condition(20.0, 0.2, 5.0); // Rain 20mm/hr

            // --- 1. BRIDGE INPUT ---
            ipc::ControlCommand cmd = bridge_->get_latest_command();

            // --- 2. FIRE CONTROL (Projectiles) ---
            if (cmd.fire_trigger) {
                // Rate limiter 10Hz
                static double last_shot = 0;
                if (now - last_shot > 0.1) {
                    Projectile p;
                    p.pos = sensor_pos;
                    p.vel = gimbal_.get_forward_vector() * 800.0; // 800m/s
                    projectiles.push_back(p);
                    last_shot = now;
                    spdlog::info("💥 SHOT FIRED");
                }
            }
            
            // Projectile Physics & Collision
            for (auto& p : projectiles) {
                if (!p.active) continue;
                p.vel.y += -9.81 * dt; // Gravity
                p.pos += p.vel * dt;
                p.age += dt;
                
                // Ground Hit / Timeout
                if (p.pos.y < 0 || p.age > 4.0) p.active = false;

                // Drone Hit
                for (auto& e : entities_) {
                    if (glm::distance(p.pos, e->get_position()) < 1.0) {
                        spdlog::critical("🎯 KILL CONFIRMED: {}", e->get_name());
                        e->set_position(glm::dvec3(0, -9000, 0)); // Teleport away
                        p.active = false;
                    }
                }
            }

            // --- 3. HARDWARE UPDATE ---
            gimbal_.update(dt, cmd.pan_velocity, cmd.tilt_velocity);
            glm::dvec3 sensor_facing = gimbal_.get_forward_vector();

            // --- 4. DRONE PHYSICS ---
            for (auto& e : entities_) {
                physics::DroneDynamics::apply_physics(*e, drone_phys_config_, dt);
                // Wind + Gusts
                glm::dvec3 gust(math::Random::gaussian(0.5), math::Random::gaussian(0.2), math::Random::gaussian(0.5));
                e->set_velocity(e->get_velocity() + (global_wind_ * 0.1 + gust) * dt);
                e->update(dt);
            }

            // --- 5. RADAR SENSOR ---
            std::vector<ipc::SimRadarPoint> radar_hits;
            double noise = physics::RadarPhysics::calculate_environment_noise(entities_, sensor_pos);
            
            for (auto& e : entities_) {
                if (environment_->check_occlusion(sensor_pos, e->get_position())) continue;

                // Scan (Returns Direct + Multipath ghosts)
                auto returns = physics::RadarPhysics::scan_target(
                    sensor_pos, sensor_facing, *e, radar_config_, noise, weather_.get_state()
                );

                for (const auto& ret : returns) {
                    ipc::SimRadarPoint hit;
                    // Polar to Cartesian (Simulating raw detection)
                    hit.x = ret.range * sin(ret.azimuth) * cos(ret.elevation);
                    hit.y = ret.range * sin(ret.elevation);
                    hit.z = ret.range * cos(ret.azimuth) * cos(ret.elevation);
                    hit.velocity = ret.velocity;
                    hit.snr_db = ret.snr_db;
                    hit.object_id = 1; 
                    radar_hits.push_back(hit);
                }
            }

            // --- 6. VISION SENSOR ---
            if (!is_headless_) {
                // Auto-Switch Day/Night based on Sun
                // For MVP: Toggle manually or based on time. 
                // Let's assume VISIBLE unless commanded otherwise.
                renderer_->set_render_mode(phenomenology::RenderMode::VISIBLE);
                
                renderer_->set_camera_orientation(sensor_facing);
                renderer_->set_sun_position(glm::normalize(glm::dvec3(0.5, 1.0, -0.5)));
                renderer_->clear();
                
                for (auto& e : entities_) {
                    if (!environment_->check_occlusion(sensor_pos, e->get_position())) {
                        renderer_->render_entity(*e, sensor_pos);
                    }
                }
                
                renderer_->apply_environmental_effects(weather_.get_state().fog_density);
            }

            // --- 7. BRIDGE ---
            bridge_->publish_frame(frame, now, radar_hits);

            // --- 8. PACING ---
            if (!is_headless_) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void SimEngine::stop() { is_running_ = false; }
    void SimEngine::set_headless(bool h) { is_headless_ = h; }
}