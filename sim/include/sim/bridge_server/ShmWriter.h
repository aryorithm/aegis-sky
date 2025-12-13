#pragma once
#include "aegis_ipc/shm_layout.h"
#include <vector>

namespace aegis::sim::bridge {

    class ShmWriter {
    public:
        ShmWriter();
        ~ShmWriter();

        bool initialize();
        void cleanup();

        // The Main Function: Push data to the Core
        void publish_frame(uint64_t frame_id, double time, const std::vector<ipc::SimRadarPoint>& radar_data);

    private:
        int shm_fd_;            // File Descriptor
        void* mapped_ptr_;      // Raw pointer to shared RAM
        ipc::BridgeHeader* header_;     // Pointer to the struct part
        ipc::SimRadarPoint* radar_buf_; // Pointer to the data part
    };

}