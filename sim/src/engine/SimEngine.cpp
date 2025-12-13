#include "sim/engine/SimEngine.h"

// Include the full headers here to access functions
#include "sim/engine/ScenarioLoader.h"
#include "sim/bridge_server/ShmWriter.h"
#include "sim/phenomenology/optics/MockRenderer.h"
#include "sim/physics/RadarPhysics.h"

#include <spdlog/spdlog.h>
#include <thread>
#include <iostream>

namespace aegis::sim::engine {

    SimEngine::SimEngine() : is_running_(false) {
        // Instantiate Subsystems
        bridge_ = std::make_unique<bridge::ShmWriter>();
        
        // Initialize Renderer at 1080p
        renderer_ = std::make_unique<phenomenology::MockRenderer>(1920, 1080);
        
        // Define Default Physics (DJI Mavic style)
        drone_phys_config_.mass_kg = 1.2;
        drone_phys_config_.drag_coeff = 0.3;
        drone_phys_config_.max_thrust_n = 30.0;
    }

    SimEngine::~SimEngine() {
        if (bridge_) {
            bridge_->cleanup();
        }
    }

    void SimEngine::initialize(const std::string& scenario_path) {
        spdlog::info("[Sim] Booting Aegis Matrix Kernel...");

        // 1. Load the Scenario
        entities_ = ScenarioLoader::load_mission(scenario_path);
        
        if (entities_.empty()) {
            throw std::runtime_error("ScenarioLoader returned 0 entities. Check your JSON path.");
        }

        // 2. Initialize Shared Memory Bridge
        if (!bridge_->initialize()) {
            throw std::runtime_error("Failed to initialize /dev/shm bridge.");
        }

        is_running_ = true;
        spdlog::info("[Sim] Initialization Complete. Ready to run.");
    }

    void SimEngine::run() {
        spdlog::info("[Sim] Simulation Loop Started.");
        
        // Aegis Pod Location (Origin)
        glm::dvec3 sensor_pos(0, 0, 0);
        glm::dvec3 radar_facing(0, 0, 1); // Facing Z+

        while (is_running_) {
            // A. Update Time
            time_manager_.tick();
            double dt = time_manager_.get_delta_time();
            double total_time = time_manager_.get_total_time();
            uint64_t frame = time_manager_.get_frame_count();

            // B. Update Physics (Movement)
            for (auto& entity : entities_) {
                physics::DroneDynamics::apply_physics(*entity, drone_phys_config_, dt);
                entity->update(dt);
            }

            // C. Simulate Radar (Raycasting)
            std::vector<ipc::SimRadarPoint> radar_hits;
            for (auto& entity : entities_) {
                auto result = physics::RadarPhysics::cast_ray(sensor_pos, radar_facing, *entity);
                
                if (result.detected) {
                    ipc::SimRadarPoint hit;
                    // Convert double (Physics) to float (IPC)
                    hit.x = static_cast<float>(entity->get_position().x);
                    hit.y = static_cast<float>(entity->get_position().y);
                    hit.z = static_cast<float>(entity->get_position().z);
                    hit.velocity = static_cast<float>(result.velocity);
                    hit.snr_db = static_cast<float>(result.snr_db);
                    
                    // TODO: Hash string name to integer ID if needed
                    hit.object_id = 1; 

                    radar_hits.push_back(hit);
                }
            }

            // D. Simulate Vision (Rendering)
            renderer_->clear();
            for (auto& entity : entities_) {
                renderer_->render_entity(*entity, sensor_pos);
            }

            // E. Publish to Bridge
            // Note: Currently we send Radar data. Video data requires extra memory copy logic.
            bridge_->publish_frame(frame, total_time, radar_hits);

            // F. Logging & Frame Pacing
            if (frame % 60 == 0) {
                 spdlog::info("[Sim] Time: {:.2f}s | Drones: {} | Radar Detections: {}", 
                    total_time, entities_.size(), radar_hits.size());
            }

            // Cap at ~100Hz to save CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void SimEngine::stop() {
        is_running_ = false;
        spdlog::info("[Sim] Stopping Engine.");
    }

}