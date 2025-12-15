#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <csignal>
#include <vector>

// --- INFRASTRUCTURE & SHARED ---
#include "platform/threading/Scheduler.h"
#include "aegis_ipc/shm_layout.h"
#include "aegis_ipc/station_protocol.h"
#include "telemetry.pb.h" // Generated from proto

// --- DRIVERS (HAL) ---
#include "aegis/hal/IRadar.h"
#include "aegis/hal/ICamera.h"
#include "drivers/bridge_client/ShmReader.h"
#include "drivers/bridge_client/SimRadar.cpp"
#include "drivers/bridge_client/SimCamera.cpp"
#include "drivers/real_sensors/GStreamerCamera.h"

// --- SERVICES (The Brain) ---
#include "services/comms/StationLink.h"
#include "services/comms/CloudLink.h"
#include "services/fusion/FusionEngine.h"
#include "services/perception/InferenceManager.h"
#include "services/tracking/TrackManager.h"

// --- LOGGING ---
#include <spdlog/spdlog.h>

using namespace aegis::core;

// Global flag for graceful shutdown
volatile std::sig_atomic_t g_running = 1;

void signal_handler(int) {
    g_running = 0;
}

// Helper for command line arguments
void parse_args(int argc, char** argv, bool& use_sim) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--live") {
            use_sim = false;
        }
    }
}

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================
int main(int argc, char** argv) {
    // 1. SETUP LOGGING & SIGNALS
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [Core] %v");
    std::signal(SIGINT, signal_handler);

    spdlog::info("========================================");
    spdlog::info("   AEGIS CORE: FLIGHT SOFTWARE v1.0     ");
    spdlog::info("========================================");

    // 2. PARSE ARGUMENTS
    bool use_sim_mode = true;
    parse_args(argc, argv, use_sim_mode);
    spdlog::info("Booting in {} Mode.", use_sim_mode ? "SIMULATION" : "LIVE HARDWARE");

    // 3. REAL-TIME PRIORITY
    if (platform::Scheduler::set_realtime_priority(50)) {
        spdlog::info("Running in Real-Time Mode (SCHED_FIFO)");
    } else {
        spdlog::warn("Running in Standard Scheduling Mode (Latency not guaranteed)");
    }

    try {
        // 4. INITIALIZE DRIVERS (HAL)
        std::unique_ptr<hal::IRadar> radar_driver;
        std::unique_ptr<hal::ICamera> camera_driver;
        std::shared_ptr<drivers::ShmReader> bridge;

        if (use_sim_mode) {
            // --- SIMULATION MODE ---
            bridge = std::make_shared<drivers::ShmReader>();
            spdlog::info("Connecting to Matrix Bridge...");
            while (!bridge->connect() && g_running) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (!g_running) return 0;

            radar_driver = std::make_unique<drivers::SimRadar>(bridge);
            camera_driver = std::make_unique<drivers::SimCamera>(bridge);
            spdlog::info("Bridge Connected. Virtual Sensors Online.");
        } else {
            // --- LIVE HARDWARE MODE ---
            // TODO: Implement Real Radar Driver
            bridge = std::make_shared<drivers::ShmReader>(); // Temp for demo
            radar_driver = std::make_unique<drivers::SimRadar>(bridge);

            // Use the GStreamer driver for a real USB/IP camera
            std::string pipeline = "v4l2src device=/dev/video0 ! video/x-raw,width=1920,height=1080,framerate=30/1";
            camera_driver = std::make_unique<drivers::GStreamerCamera>(pipeline);
        }

        if (!camera_driver->initialize() || !radar_driver->initialize()) {
             throw std::runtime_error("Failed to initialize one or more sensor drivers!");
        }

        // 5. INITIALIZE AUTONOMY STACK
        auto cal_data = services::CalibrationData::create_perfect_alignment(1920, 1080);
        auto fusion_engine = std::make_unique<services::FusionEngine>(cal_data);
        auto inference_mgr = std::make_unique<services::InferenceManager>("configs/aura_v1.plan");
        auto track_manager = std::make_unique<services::tracking::TrackManager>();

        // 6. INITIALIZE COMMS
        auto station_link = std::make_unique<services::StationLink>(9090);
        auto cloud_link = std::make_unique<services::CloudLink>("localhost:50051");

        if (!station_link->start()) throw std::runtime_error("Failed to start StationLink.");
        cloud_link->start();

        // 7. MAIN GUIDANCE LOOP
        spdlog::info("Guidance Loop Engaged. System is Autonomous.");

        while (g_running) {
            auto loop_start = std::chrono::high_resolution_clock::now();

            // --- A: SENSOR INGESTION ---
            auto cloud = radar_driver->get_scan();
            auto image = camera_driver->get_frame();
            if (!image.data_ptr && !use_sim_mode) continue;

            double sys_time = cloud.timestamp;

            // --- B: FUSION ---
            auto fused_frame = fusion_engine->process(image, cloud);

            // --- C: PERCEPTION (AI) ---
            auto detections = inference_mgr->detect(fused_frame);

            // --- D: TRACKING ---
            hal::PointCloud ai_cloud;
            ai_cloud.timestamp = sys_time;
            for (const auto& det : detections) {
                if (det.class_id == 0) { // Class 0 = Drone
                    hal::RadarPoint p;
                    // TODO: Sample depth map from 'fused_frame' for real Z
                    p.x = (det.x_min + det.x_max) / 2.0f;
                    p.y = (det.y_min + det.y_max) / 2.0f;
                    p.z = 100.0f;
                    p.snr = det.confidence * 100.0f;
                    ai_cloud.points.push_back(p);
                }
            }
            track_manager->process_scan(ai_cloud);
            auto active_tracks = track_manager->get_tracks();

            // --- E: COMMAND ---
            ipc::station::CommandPacket ui_cmd;
            bool has_new_cmd = station_link->get_latest_command(ui_cmd);
            ipc::ControlCommand flight_cmd = {};
            flight_cmd.timestamp = (uint64_t)(sys_time * 1000.0);

            if (has_new_cmd) {
                flight_cmd.pan_velocity = ui_cmd.pan_velocity;
                flight_cmd.tilt_velocity = ui_cmd.tilt_velocity;
                if (ui_cmd.arm_system && ui_cmd.fire_trigger) flight_cmd.fire_trigger = true;
            }

            // --- F: ACTUATION ---
            if (use_sim_mode && bridge) {
                bridge->send_command(flight_cmd);
            }

            // --- G: TELEMETRY (To Station) ---
            int confirmed_threats = 0;
            for(const auto& t : active_tracks) if(t.is_confirmed) confirmed_threats++;
            station_link->broadcast_telemetry(sys_time, 0.f, 0.f, confirmed_threats);

            // --- H: CLOUD LOGGING ---
            uint64_t frame_count = 0; // In real system, this would be a class member
            if (frame_count++ % 30 == 0) { // 2Hz Health Update
                telemetry::TelemetryPacket p;
                auto h = p.mutable_health();
                h->set_cpu_temperature(60.0f); // Read from /sys/class/thermal
                h->set_gpu_temperature(70.0f);
                cloud_link->send_telemetry(p);
            }
            // Send new confirmed tracks
            // (In prod, check for new tracks instead of sending every frame)
            for (const auto& t : active_tracks) {
                telemetry::TelemetryPacket p;
                auto d = p.mutable_detection();
                d->set_track_id(t.id);
                // ...
                cloud_link->send_telemetry(p);
            }

            // --- PACING ---
            auto loop_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = loop_end - loop_start;
            if (elapsed.count() < 16.66) {
                std::this_thread::sleep_for(std::chrono::milliseconds((int)(16.66 - elapsed.count())));
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::critical("[Core] FATAL ERROR: {}", e.what());
        return -1;
    }

    spdlog::info("[Core] Shutdown sequence initiated.");
    return 0;
}