#pragma once
#include "KalmanFilter.h"
#include "aegis/hal/IRadar.h" // For PointCloud
#include <map>
#include <vector>

namespace aegis::core::services::tracking {

    struct Track {
        uint32_t id;
        KalmanFilter filter;
        int missed_frames = 0; // For deletion logic
        bool is_confirmed = false;

        // Metadata
        float confidence = 0.0f;
    };

    class TrackManager {
    public:
        TrackManager();

        // The Main Loop
        void process_scan(const hal::PointCloud& cloud);

        // Get active tracks for the UI/Fire Control
        std::vector<Track> get_tracks() const;

    private:
        // Core Logic: Match measurements to existing tracks
        void associate_and_update(const std::vector<hal::RadarPoint>& measurements, double time);

        // Logic to spawn new tracks
        void create_track(const hal::RadarPoint& meas, double time);

        // Logic to delete old tracks (Garbage Collection)
        void prune_tracks();

        std::vector<Track> tracks_;
        uint32_t next_id_ = 1;

        // Config
        const float match_threshold_dist_ = 5.0f; // 5 meters
        const int max_missed_frames_ = 60; // 1 second coasting
    };
}