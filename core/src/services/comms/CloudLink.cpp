#include "CloudLink.h"
#include <spdlog/spdlog.h>
#include <google/protobuf/util/time_util.h>

namespace aegis::core::services {

    using google::protobuf::util::TimeUtil;

    CloudLink::CloudLink(const std::string& server_address)
        : server_address_(server_address), unit_id_("AEGIS-POD-001") { // ID should come from config
    }

    CloudLink::~CloudLink() {
        stop();
    }

    void CloudLink::start() {
        if (is_running_) return;

        spdlog::info("[CloudLink] Starting connection to {}", server_address_);
        is_running_ = true;

        // Create a gRPC channel (connection)
        auto channel = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
        stub_ = telemetry::IngestorService::NewStub(channel);

        // Start the writer thread immediately, it will handle reconnections
        writer_thread_ = std::thread(&CloudLink::writer_loop, this);
    }

    void CloudLink::stop() {
        is_running_ = false;
        if (writer_thread_.joinable()) writer_thread_.join();
        if (reader_thread_.joinable()) reader_thread_.join();
    }

    void CloudLink::send_telemetry(const telemetry::TelemetryPacket& packet) {
        if (is_running_) {
            send_queue_.push(packet);
        }
    }

    void CloudLink::writer_loop() {
        while (is_running_) {
            // Establish the stream
            grpc::ClientContext context;
            stream_ = stub_->StreamTelemetry(&context);

            spdlog::info("[CloudLink] Stream to Cloud established.");

            // Start a reader for this stream
            if(reader_thread_.joinable()) reader_thread_.join(); // Join old reader
            reader_thread_ = std::thread(&CloudLink::reader_loop, this);

            while (is_running_) {
                telemetry::TelemetryPacket packet;

                // Blocking pop from the queue
                send_queue_.wait_and_pop(packet);

                // Add metadata
                packet.set_unit_id(unit_id_);
                *packet.mutable_timestamp() = TimeUtil::GetCurrentTime();

                if (!stream_->Write(packet)) {
                    spdlog::error("[CloudLink] Stream write failed. Reconnecting...");
                    stream_->Finish(); // Get final status
                    break; // Break inner loop to reconnect
                }
            }
            // Wait before retrying connection
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    void CloudLink::reader_loop() {
        telemetry::ServerCommand cmd;
        // This loop blocks on Read() until the stream breaks or a message arrives
        while (stream_ && stream_->Read(&cmd)) {
            switch(cmd.command()) {
                case telemetry::ServerCommand::ACK:
                    // spdlog::debug("[CloudLink] ACK received from server.");
                    break;
                case telemetry::ServerCommand::REBOOT:
                    spdlog::warn("[CloudLink] REBOOT command received! (Ignoring for now)");
                    // system("sudo reboot");
                    break;
                default:
                    break;
            }
        }
        spdlog::warn("[CloudLink] Reader stream closed.");
    }
}