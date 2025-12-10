#include "sim/engine/SimEngine.h"
#include "sim/engine/ScenarioLoader.h"
#include "sim/bridge_server/ShmWriter.h"
#include "sim/phenomenology/optics/MockRenderer.h"
#include "sim/physics/RadarPhysics.h"
#include "sim/physics/GimbalPhysics.h"
#include "sim/physics/DroneDynamics.h"
#include "sim/engine/TerrainSystem.h"
#include "sim/engine/Environment.h"
#include "sim/engine/WeatherSystem.h"
#include "sim/math/Random.h" 

#include <spdlog/spdlog.h>
#include <thread>

namespace aegis::sim::engine {

    struct Projectile {
        glm::dvec3 pos, vel;
        bool active = true;
        double age = 0;
    };

    SimEngine::SimEngine() : is_running_(false), is_headless_(false) {
        math::Random::init();
        bridge_ = std::make_unique<bridge::ShmWriter>();
        renderer_ = std::make_unique<phenomenology::MockRenderer>(1920, 1080);
        environment_ = std::make_unique<Environment>();
        
        environment_->add_building(glm::dvec3(0, 15, 200), glm::dvec3(60, 30, 20)); // Obstacle
        
        drone_phys_config_ = {1.2, 0.3, 30.0};
        radar_config_ = {120.0, 30.0, 2500.0, 0.5, 0.01, 0.2};
        global_wind_ = glm::dvec3(2.0, 0.0, 0.0);
    }

    SimEngine::~SimEngine() { if (bridge_) bridge_->cleanup(); }

    void SimEngine::initialize(const std::string& path) {
        entities_ = ScenarioLoader::load_mission(path);
        if (!bridge_->initialize()) throw std::runtime_error("Bridge Init Failed");
        is_running_ = true;
    }

    void SimEngine::run() {
        spdlog::info("[Sim] Matrix Online. Physics: HIGH.");
        std::vector<Projectile> projectiles;
        glm::dvec3 sensor_pos(0,0,0);

        while(is_running_) {
            time_manager_.tick();
            double dt = time_manager_.get_delta_time();
            double now = time_manager_.get_total_time();
            uint64_t frame = time_manager_.get_frame_count();

            // 1. INPUT
            ipc::ControlCommand cmd = bridge_->get_latest_command();

            // 2. FIRE CONTROL
            if (cmd.fire_trigger) {
                static double last_shot = 0;
                if (now - last_shot > 0.1) {
                    projectiles.push_back({sensor_pos, gimbal_.get_forward_vector()*800.0, true, 0});
                    last_shot = now;
                    spdlog::info("💥 SHOT");
                }
            }
            
            // 3. PROJECTILE UPDATE
            for (auto& p : projectiles) {
                if (!p.active) continue;
                p.vel.y += -9.81 * dt; p.pos += p.vel * dt; p.age += dt;
                
                if (p.pos.y < TerrainSystem::get_height(p.pos.x, p.pos.z) || p.age > 5.0) p.active = false;

                for (auto& e : entities_) {
                    if (glm::distance(p.pos, e->get_position()) < 1.0) {
                        spdlog::critical("🎯 HIT: {}", e->get_name());
                        e->set_position(glm::dvec3(0,-9999,0));
                        p.active = false;
                    }
                }
            }

            // 4. HARDWARE
            gimbal_.update(dt, cmd.pan_velocity, cmd.tilt_velocity);
            glm::dvec3 facing = gimbal_.get_forward_vector();

            // 5. DRONE PHYSICS (Terrain Collision)
            for(auto& e : entities_) {
                physics::DroneDynamics::apply_physics(*e, drone_phys_config_, dt);
                
                // Wind + Gust
                glm::dvec3 gust(math::Random::gaussian(0.5), math::Random::gaussian(0.2), math::Random::gaussian(0.5));
                e->set_velocity(e->get_velocity() + (global_wind_ * 0.1 + gust) * dt);
                e->update(dt);

                // Crash Check
                double h = TerrainSystem::get_height(e->get_position().x, e->get_position().z);
                if (e->get_position().y < h) {
                    e->set_position(glm::dvec3(e->get_position().x, h, e->get_position().z));
                    e->set_velocity(glm::dvec3(0));
                }
            }

            // 6. RADAR (Multipath + Micro-Doppler)
            std::vector<ipc::SimRadarPoint> radar_hits;
            double noise = physics::RadarPhysics::calculate_environment_noise(entities_, sensor_pos);
            
            for(auto& e : entities_) {
                // Occulsion: Building OR Terrain
                if (environment_->check_occlusion(sensor_pos, e->get_position())) continue;
                if (TerrainSystem::check_occlusion(sensor_pos, e->get_position())) continue;

                // Scan with Time (for Micro-Doppler)
                auto returns = physics::RadarPhysics::scan_target(
                    sensor_pos, facing, *e, radar_config_, noise, weather_.get_state(), now
                );

                for(const auto& ret : returns) {
                    ipc::SimRadarPoint hit;
                    hit.x = ret.range * sin(ret.azimuth) * cos(ret.elevation);
                    hit.y = ret.range * sin(ret.elevation);
                    hit.z = ret.range * cos(ret.azimuth) * cos(ret.elevation);
                    hit.velocity = ret.velocity;
                    hit.snr_db = ret.snr_db;
                    hit.object_id = 1;
                    radar_hits.push_back(hit);
                }
            }

            // 7. VISION (Motion Blur)
            if (!is_headless_) {
                renderer_->set_camera_orientation(facing);
                renderer_->clear();
                for(auto& e : entities_) {
                    if (!TerrainSystem::check_occlusion(sensor_pos, e->get_position()))
                        renderer_->render_entity(*e, sensor_pos, dt);
                }
                renderer_->apply_effects(weather_.get_state().fog_density);
            }

            // 8. BRIDGE
            bridge_->publish_frame(frame, now, radar_hits);
            if (!is_headless_) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    void SimEngine::stop() { is_running_ = false; }
    void SimEngine::set_headless(bool h) { is_headless_ = h; }
}