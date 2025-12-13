#pragma once
#include <cstdint>

namespace aegis::hal {

    struct ImageFrame {
        double timestamp;
        int width;
        int height;
        int stride;

        // Pointer to GPU Pinned Memory (Zero-Copy)
        // If this is nullptr, the frame is invalid.
        uint8_t* data_ptr = nullptr;

        // Context handle (e.g., CUDA stream or buffer ID for cleanup)
        void* context = nullptr;
    };

    class ICamera {
    public:
        virtual ~ICamera() = default;

        virtual bool initialize() = 0;

        // Blocking call to get latest frame
        virtual ImageFrame get_frame() = 0;
    };
}