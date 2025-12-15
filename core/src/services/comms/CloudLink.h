#pragma once

// Generated Headers
#include "telemetry.grpc.pb.h"
#include "telemetry.pb.h"

#include <grpcpp/grpcpp.h>
#include <thread>
#include <atomic>
#include <string>
#include <memory>
#include <concurrent_queue.h> // Using a simple concurrent queue

namespace aegis::core::services {

    class CloudLink {
    public:
        CloudLink(const std::string& server_address);
        ~CloudLink();

        // Starts the connection and streaming threads
        void start();
        void stop();

        // Queues a telemetry packet to be sent to the cloud
        void send_telemetry(const telemetry::TelemetryPacket& packet);

    private:
        // Main worker loop for sending data
        void writer_loop();
        // Main worker loop for receiving commands
        void reader_loop();

        std::string server_address_;
        std::string unit_id_; // Unique ID for this pod

        std::unique_ptr<telemetry::IngestorService::Stub> stub_;
        std::shared_ptr<grpc::ClientReaderWriter<telemetry::TelemetryPacket, telemetry::ServerCommand>> stream_;

        std::thread writer_thread_;
        std::thread reader_thread_;

        std::atomic<bool> is_running_{false};

        // Thread-safe queue for outgoing messages
        concurrency::concurrent_queue<telemetry::TelemetryPacket> send_queue_;
    };
}