#include "sim/engine/SimEngine.h"

#include "sim/engine/ScenarioLoader.h"
#include "sim/bridge_server/ShmWriter.h"
#include "sim/phenomenology/optics/MockRenderer.h"
#include "sim/physics/RadarPhysics.h"
#include "sim/physics/GimbalPhysics.h"
#include "sim/engine/Environment.h" // Occlusion Logic
#include "sim/math/Random.h" 

#include <spdlog/spdlog.h>
#include <thread>
#include <cmath>

namespace aegis::sim::engine {

    SimEngine::SimEngine() : is_running_(false) {
        // 1. Math Init
        math::Random::init();

        // 2. Subsystems Init
        bridge_ = std::make_unique<bridge::ShmWriter>();
        renderer_ = std::make_unique<phenomenology::MockRenderer>(1920, 1080);
        environment_ = std::make_unique<Environment>();

        // 3. Define World Environment (Obstacles)
        // Add a "Warehouse" at Z=200 that blocks the radar
        // Center: (0, 10, 200), Size: (50m wide, 20m tall, 10m deep)
        environment_->add_building(glm::dvec3(0, 10, 200), glm::dvec3(50, 20, 10));

        // 4. Physics Defaults
        drone_phys_config_.mass_kg = 1.2;
        drone_phys_config_.drag_coeff = 0.3;
        drone_phys_config_.max_thrust_n = 30.0;

        // 5. Radar Config
        radar_config_.fov_azimuth_deg = 120.0;
        radar_config_.fov_elevation_deg = 30.0;
        radar_config_.max_range = 2000.0;
        radar_config_.noise_range_m = 0.5;
        radar_config_.noise_angle_rad = 0.01;
        radar_config_.noise_vel_ms = 0.2;

        global_wind_ = glm::dvec3(2.0, 0.0, 1.0);
    }

    SimEngine::~SimEngine() {
        if (bridge_) bridge_->cleanup();
    }

    void SimEngine::initialize(const std::string& scenario_path) {
        spdlog::info("[Sim] Booting Aegis Matrix Kernel...");

        entities_ = ScenarioLoader::load_mission(scenario_path);
        if (entities_.empty()) throw std::runtime_error("No entities loaded.");

        if (!bridge_->initialize()) throw std::runtime_error("Bridge Init Failed");

        is_running_ = true;
        spdlog::info("[Sim] Ready. Loaded {} entities. Occlusion enabled.", entities_.size());
    }

    void SimEngine::run() {
        spdlog::info("[Sim] Loop Started.");
        
        glm::dvec3 sensor_pos(0, 0, 0); 
        
        while (is_running_) {
            time_manager_.tick();
            double dt = time_manager_.get_delta_time();
            double now = time_manager_.get_total_time();
            uint64_t frame = time_manager_.get_frame_count();

            // --- 1. INPUT (Bridge) ---
            ipc::ControlCommand cmd = bridge_->get_latest_command();
            
            // --- 2. GIMBAL PHYSICS ---
            gimbal_.update(dt, cmd.pan_velocity, cmd.tilt_velocity);
            glm::dvec3 sensor_facing = gimbal_.get_forward_vector();

            // --- 3. DRONE PHYSICS ---
            for (auto& entity : entities_) {
                physics::DroneDynamics::apply_physics(*entity, drone_phys_config_, dt);
                
                // Wind Drift
                glm::dvec3 gust(math::Random::gaussian(0.2), math::Random::gaussian(0.1), math::Random::gaussian(0.2));
                glm::dvec3 current_vel = entity->get_velocity();
                entity->set_velocity(current_vel + (global_wind_ * 0.05 + gust) * dt);
                
                entity->update(dt);
            }

            // --- 4. ELECTRONIC WARFARE (Calc Noise Floor) ---
            double current_noise = physics::RadarPhysics::calculate_environment_noise(entities_, sensor_pos);
            
            // --- 5. RADAR STEP ---
            std::vector<ipc::SimRadarPoint> radar_hits;

            for (auto& entity : entities_) {
                // A. OCCLUSION CHECK (Is it behind a building?)
                if (environment_->check_occlusion(sensor_pos, entity->get_position())) {
                    // Ray blocked by environment
                    continue; 
                }

                // B. RAYCAST (With Dynamic Noise)
                auto result = physics::RadarPhysics::cast_ray(sensor_pos, sensor_facing, *entity, radar_config_, current_noise);
                
                if (result.detected) {
                    ipc::SimRadarPoint hit;
                    // Convert to Noisy Cartesian for output
                    double r = result.range;
                    double az = result.azimuth; 
                    double el = result.elevation;

                    hit.x = static_cast<float>(r * std::sin(az));
                    hit.y = static_cast<float>(r * std::sin(el));
                    hit.z = static_cast<float>(r * std::cos(az));
                    
                    hit.velocity = static_cast<float>(result.velocity);
                    hit.snr_db = static_cast<float>(result.snr_db);
                    hit.object_id = 1; 

                    radar_hits.push_back(hit);
                }
            }

            // Clutter Generation
            for (int i = 0; i < 3; ++i) {
                ipc::SimRadarPoint ghost;
                double r = math::Random::uniform(10.0, 500.0);
                ghost.x = static_cast<float>(math::Random::uniform(-50, 50));
                ghost.y = static_cast<float>(math::Random::uniform(-10, 10)); 
                ghost.z = static_cast<float>(r);
                ghost.velocity = static_cast<float>(math::Random::gaussian(0.5));
                // Ghost targets also suffer from Jamming (Lower SNR)
                ghost.snr_db = static_cast<float>(math::Random::uniform(5.0, 12.0) - (current_noise / 1e-13)); 
                ghost.object_id = 0; 
                radar_hits.push_back(ghost);
            }

            // --- 6. VISION STEP ---
            renderer_->set_camera_orientation(sensor_facing);
            renderer_->clear();
            for (auto& entity : entities_) {
                // Occlusion check for vision too
                if (!environment_->check_occlusion(sensor_pos, entity->get_position())) {
                     renderer_->render_entity(*entity, sensor_pos);
                }
            }

            // --- 7. PUBLISH ---
            bridge_->publish_frame(frame, now, radar_hits);

            // Logging
            if (frame % 60 == 0) {
                 spdlog::info("[Sim] T:{:.1f}s | NoiseFloor:{:.2e}W | RadarHits:{}", 
                    now, current_noise, radar_hits.size());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void SimEngine::stop() { is_running_ = false; }
}