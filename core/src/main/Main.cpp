#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <cmath>
#include <csignal>

// --- INFRASTRUCTURE ---
#include "platform/threading/Scheduler.h"
#include "aegis_ipc/shm_layout.h"

// --- DRIVERS (HAL) ---
#include "drivers/bridge_client/ShmReader.h"
// Including .cpp files for MVP single-unit compilation.
// In production, link against compiled object libraries.
#include "drivers/bridge_client/SimRadar.cpp"
#include "drivers/bridge_client/SimCamera.cpp"

// --- SERVICES (The Brain) ---
#include "services/comms/StationLink.h"
#include "services/fusion/FusionEngine.h"
#include "services/perception/InferenceManager.h"
#include "services/tracking/TrackManager.h"

// --- LOGGING ---
#include <spdlog/spdlog.h>

using namespace aegis::core;

// Global flag for graceful shutdown on Ctrl+C
volatile std::sig_atomic_t g_running = 1;

void signal_handler(int) {
    g_running = 0;
}

int main(int argc, char** argv) {
    // 1. SETUP LOGGING & SIGNALS
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [Core] %v");
    std::signal(SIGINT, signal_handler);

    spdlog::info("========================================");
    spdlog::info("   AEGIS CORE: FLIGHT SOFTWARE v1.0     ");
    spdlog::info("========================================");

    // 2. REAL-TIME PRIORITY (Requires sudo)
    if (platform::Scheduler::set_realtime_priority(50)) {
        spdlog::info("Running in Real-Time Mode (SCHED_FIFO)");
    } else {
        spdlog::warn("Running in Standard Scheduling Mode (Latency not guaranteed)");
    }

    try {
        // 3. INITIALIZE HARDWARE BRIDGE (Link to Simulator)
        auto bridge = std::make_shared<drivers::ShmReader>();
        spdlog::info("Connecting to Matrix Bridge...");
        while (!bridge->connect() && g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (!g_running) return 0;
        spdlog::info("Bridge Connected. Sensors Online.");

        // 4. INITIALIZE DRIVERS
        auto radar_driver = std::make_unique<drivers::SimRadar>(bridge);
        auto camera_driver = std::make_unique<drivers::SimCamera>(bridge);

        // 5. INITIALIZE AUTONOMY STACK
        // A. Fusion Engine (CUDA)
        auto cal_data = services::CalibrationData::create_perfect_alignment(1920, 1080);
        auto fusion_engine = std::make_unique<services::FusionEngine>(cal_data);

        // B. Perception Engine (xInfer)
        // NOTE: Ensure 'aura_v1.plan' is in a 'configs' folder
        auto inference_mgr = std::make_unique<services::InferenceManager>("configs/aura_v1.plan");

        // C. Tracking Engine (Kalman)
        auto track_manager = std::make_unique<services::tracking::TrackManager>();

        // 6. INITIALIZE COMMS (Link to UI Station)
        auto station_link = std::make_unique<services::StationLink>(9090);
        if (!station_link->start()) {
            throw std::runtime_error("Failed to bind TCP Port 9090");
        }

        // 7. MAIN GUIDANCE LOOP
        spdlog::info("Guidance Loop Engaged. System is Autonomous.");

        while (g_running) {
            auto loop_start = std::chrono::high_resolution_clock::now();

            // --- PHASE A: SENSOR INGESTION (Zero-Copy) ---
            auto cloud = radar_driver->get_scan();
            auto image = camera_driver->get_frame();
            double sys_time = cloud.timestamp;

            // --- PHASE B: SENSOR FUSION (CUDA) ---
            auto fused_frame = fusion_engine->process(image, cloud);

            // --- PHASE C: PERCEPTION (xInfer) ---
            auto detections = inference_mgr->detect(fused_frame);

            // --- PHASE D: TRACKING (Kalman) ---
            // Bridge: Convert AI detections to a PointCloud for the tracker
            hal::PointCloud ai_cloud;
            ai_cloud.timestamp = sys_time;
            for (const auto& det : detections) {
                if (det.class_id == 0) { // Class 0 = Drone
                    hal::RadarPoint p;
                    // For now, use box center. In prod, use depth map from fusion.
                    p.x = (det.x_min + det.x_max) / 2.0f;
                    p.y = (det.y_min + det.y_max) / 2.0f;
                    p.z = 50.0f; // TODO: Sample depth map at (p.x, p.y)
                    p.snr = det.confidence * 100.0f;
                    ai_cloud.points.push_back(p);
                }
            }
            track_manager->process_scan(ai_cloud);
            auto active_tracks = track_manager->get_tracks();

            // --- PHASE E: COMMAND & CONTROL ---
            ipc::station::CommandPacket ui_cmd;
            bool has_new_cmd = station_link->get_latest_command(ui_cmd);

            ipc::ControlCommand flight_cmd;
            flight_cmd.timestamp = (uint64_t)(sys_time * 1000.0);

            // Auto-Aiming Logic (If no manual override)
            if (has_new_cmd && (ui_cmd.pan_velocity != 0 || ui_cmd.tilt_velocity != 0)) {
                // Manual Override
                flight_cmd.pan_velocity = ui_cmd.pan_velocity;
                flight_cmd.tilt_velocity = ui_cmd.tilt_velocity;
            } else if (!active_tracks.empty()) {
                // AUTO-AIM at the first confirmed track
                // A simple PID controller would go here to generate velocity
                // For now, simple proportional control:
                auto& primary_target = active_tracks[0];
                auto target_pos = primary_target.filter.get_position();

                // Convert XYZ to Azimuth/Elevation
                float target_az = atan2(target_pos[0], target_pos[2]);
                flight_cmd.pan_velocity = target_az * 0.5f; // Proportional gain
            } else {
                flight_cmd.pan_velocity = 0.0f;
                flight_cmd.tilt_velocity = 0.0f;
            }

            // Weapons
            flight_cmd.fire_trigger = has_new_cmd && ui_cmd.arm_system && ui_cmd.fire_trigger;
            if (flight_cmd.fire_trigger) spdlog::warn("WEAPONS RELEASE AUTHORIZED");

            // --- PHASE F: ACTUATION ---
            bridge->send_command(flight_cmd);

            // --- PHASE G: TELEMETRY ---
            int confirmed_threats = 0;
            for(const auto& t : active_tracks) if(t.is_confirmed) confirmed_threats++;
            station_link->broadcast_telemetry(sys_time, 0.f, 0.f, confirmed_threats);

            // --- PACING ---
            auto loop_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = loop_end - loop_start;

            // Maintain ~60Hz Loop
            if (elapsed.count() < 16.66) {
                std::this_thread::sleep_for(std::chrono::milliseconds((int)(16.66 - elapsed.count())));
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::critical("[Core] FATAL ERROR: {}", e.what());
        return -1;
    }

    spdlog::info("[Core] Shutdown complete.");
    return 0;
}