#include "ShmReader.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace aegis::core::drivers {

    ShmReader::ShmReader() {}
    ShmReader::~ShmReader() { disconnect(); }

    bool ShmReader::connect() {
        // 1. Open the existing SHM file (Created by Sim)
        shm_fd_ = shm_open(ipc::BRIDGE_NAME, O_RDWR, 0666);
        if (shm_fd_ == -1) {
            std::cerr << "[BridgeClient] Waiting for Sim... (shm_open failed)" << std::endl;
            return false;
        }

        // 2. Map Memory
        mapped_ptr_ = mmap(0, ipc::BRIDGE_SIZE_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (mapped_ptr_ == MAP_FAILED) {
            close(shm_fd_);
            return false;
        }

        // 3. Setup Pointers
        header_ = static_cast<ipc::BridgeHeader*>(mapped_ptr_);

        if (header_->magic_number != ipc::BRIDGE_MAGIC) {
            std::cerr << "[BridgeClient] SHM Magic Mismatch!" << std::endl;
            disconnect();
            return false;
        }

        // Calculate offsets based on layout
        // [Header] [RadarData...] [CommandStruct] [VideoData...]
        uint8_t* base = static_cast<uint8_t*>(mapped_ptr_);

        radar_buf_ = reinterpret_cast<ipc::SimRadarPoint*>(base + sizeof(ipc::BridgeHeader));

        // Command buffer is at specific offset defined in header or calculated fixed size
        size_t radar_capacity_bytes = 1024 * sizeof(ipc::SimRadarPoint); // Assuming max 1024 targets
        cmd_buf_ = reinterpret_cast<ipc::ControlCommand*>(base + sizeof(ipc::BridgeHeader) + radar_capacity_bytes);

        // Video starts after command
        video_buf_ = reinterpret_cast<uint8_t*>(cmd_buf_ + 1);

        std::cout << "[BridgeClient] Connected to Matrix." << std::endl;
        return true;
    }

    void ShmReader::disconnect() {
        if (mapped_ptr_) munmap(mapped_ptr_, ipc::BRIDGE_SIZE_BYTES);
        if (shm_fd_ != -1) close(shm_fd_);
        mapped_ptr_ = nullptr;
    }

    bool ShmReader::has_new_frame(uint64_t& out_frame_id) {
        if (!header_) return false;

        // Atomic Load logic would go here in prod
        // 1 = Ready to Read
        if (header_->state_flag == 1 && header_->frame_id > last_frame_id_) {
            out_frame_id = header_->frame_id;
            return true;
        }
        return false;
    }

    bool ShmReader::read_sensor_data(double& out_time, std::vector<ipc::SimRadarPoint>& out_radar, std::vector<uint8_t>& out_video) {
        if (!header_) return false;

        // 1. Read Metadata
        out_time = header_->sim_time;
        last_frame_id_ = header_->frame_id;
        uint32_t num_points = header_->num_radar_points;

        // 2. Copy Radar Data
        out_radar.resize(num_points);
        if (num_points > 0) {
            std::memcpy(out_radar.data(), radar_buf_, num_points * sizeof(ipc::SimRadarPoint));
        }

        // 3. Copy Video Data (Optional, heavy copy)
        // Only if video buffer size is defined/valid
        // For MVP we might skip video copy here and let the specific Camera driver handle it via zero-copy pointer

        return true;
    }

    void ShmReader::send_command(const ipc::ControlCommand& cmd) {
        if (cmd_buf_) {
            // Direct write to shared memory
            std::memcpy(cmd_buf_, &cmd, sizeof(ipc::ControlCommand));
        }
    }
}