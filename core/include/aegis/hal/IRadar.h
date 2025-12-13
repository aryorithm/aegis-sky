#pragma once
#include <vector>
#include <cstdint>

namespace aegis::hal {

    struct RadarPoint {
        float x, y, z;      // Position (Sensor Relative)
        float velocity;     // Radial Doppler (m/s)
        float snr;          // Signal-to-Noise Ratio (dB)
        uint32_t track_id;  // Hardware tracker ID (if available)
    };

    struct PointCloud {
        double timestamp;
        std::vector<RadarPoint> points;
    };

    class IRadar {
    public:
        virtual ~IRadar() = default;

        // Initialize hardware connection (CAN/Ethernet/SharedMem)
        virtual bool initialize() = 0;

        // Blocking call to get the latest scan
        virtual PointCloud get_scan() = 0;
    };
}