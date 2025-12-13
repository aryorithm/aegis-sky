#pragma once
#include "aegis_ipc/station_protocol.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

namespace aegis::core::services {

    class StationLink {
    public:
        StationLink(int port = ipc::station::DEFAULT_PORT);
        ~StationLink();

        // Start the listener thread
        bool start();
        void stop();

        // Send data to the connected UI
        void broadcast_telemetry(double timestamp, float pan, float tilt, int targets);

        // Get the latest command from the UI
        // Returns true if a new command was received since last check
        bool get_latest_command(ipc::station::CommandPacket& out_cmd);

    private:
        void listen_loop(); // Accepts connections
        void client_loop(); // Reads from active client

        int server_fd_ = -1;
        int client_fd_ = -1;
        int port_;

        std::atomic<bool> is_running_{false};
        std::atomic<bool> client_connected_{false};

        std::thread listen_thread_;
        std::thread client_thread_;

        // Command Data Buffer
        std::mutex cmd_mutex_;
        ipc::station::CommandPacket latest_cmd_ = {0};
        bool new_cmd_available_ = false;
    };
}