#include "FusionEngine.h"
#include "aegis/platform/CudaAllocator.h" // You built this earlier
#include <spdlog/spdlog.h>

// Forward declare the wrapper from .cu file
namespace aegis::core::services::fusion {
    void launch_projection_kernel(
        const void* points, int num, const float* K, const float* R, const float* T,
        float* depth, float* vel, int w, int h, void* stream
    );
}

namespace aegis::core::services {

    using namespace platform;

    FusionEngine::FusionEngine(const CalibrationData& cal) : cal_(cal) {
        cudaStreamCreate(&stream_);

        // 1. Allocate & Upload Matrices (Constant for whole session)
        d_K_ = (float*)CudaAllocator::alloc_device(9 * sizeof(float));
        d_R_ = (float*)CudaAllocator::alloc_device(9 * sizeof(float));
        d_T_ = (float*)CudaAllocator::alloc_device(3 * sizeof(float));

        cudaMemcpyAsync(d_K_, cal.K, 9 * sizeof(float), cudaMemcpyHostToDevice, stream_);
        cudaMemcpyAsync(d_R_, cal.R, 9 * sizeof(float), cudaMemcpyHostToDevice, stream_);
        cudaMemcpyAsync(d_T_, cal.T, 3 * sizeof(float), cudaMemcpyHostToDevice, stream_);

        // 2. Allocate Output Maps (WxH)
        size_t map_size = cal.width * cal.height * sizeof(float);
        d_depth_map_ = (float*)CudaAllocator::alloc_device(map_size);
        d_vel_map_   = (float*)CudaAllocator::alloc_device(map_size);

        // 3. Pre-allocate Radar Buffer (Max 2048 points to avoid realloc)
        radar_buf_capacity_ = 2048;
        d_radar_points_ = CudaAllocator::alloc_device(radar_buf_capacity_ * sizeof(float) * 5); // 5 floats per point

        cudaStreamSynchronize(stream_); // Ensure init is done
        spdlog::info("[Fusion] Engine Initialized on GPU");
    }

    FusionEngine::~FusionEngine() {
        CudaAllocator::free_device(d_K_);
        CudaAllocator::free_device(d_R_);
        CudaAllocator::free_device(d_T_);
        CudaAllocator::free_device(d_depth_map_);
        CudaAllocator::free_device(d_vel_map_);
        CudaAllocator::free_device(d_radar_points_);
        cudaStreamDestroy(stream_);
    }

    FusedFrame FusionEngine::process(const hal::ImageFrame& img, const hal::PointCloud& radar) {
        // 1. Upload Radar Data
        size_t num_points = radar.points.size();
        if (num_points > 0) {
            // Resize if needed (naive realloc, production would be smarter)
            if (num_points > radar_buf_capacity_) {
                // ... realloc logic ...
                spdlog::warn("[Fusion] Radar point cloud overflow!");
                num_points = radar_buf_capacity_;
            }

            // Memcpy points to GPU
            // Note: hal::RadarPoint matches struct RadarPointRaw layout (5 floats)
            size_t bytes = num_points * sizeof(hal::RadarPoint);
            cudaMemcpyAsync(d_radar_points_, radar.points.data(), bytes, cudaMemcpyHostToDevice, stream_);
        }

        // 2. Run Projection Kernel
        fusion::launch_projection_kernel(
            d_radar_points_,
            static_cast<int>(num_points),
            d_K_, d_R_, d_T_,
            d_depth_map_,
            d_vel_map_,
            cal_.width, cal_.height,
            stream_
        );

        // 3. Prepare Result
        // We pass pointers. The RGB data is assumed to be already on GPU via SimCamera/CudaAllocator.
        // If SimCamera provides CPU ptr, we would upload it here (omitted for Zero-Copy ideal).

        FusedFrame frame;
        frame.width = cal_.width;
        frame.height = cal_.height;
        frame.rgb = img.data_ptr; // Assuming this is device pointer
        frame.depth = d_depth_map_;
        frame.velocity = d_vel_map_;
        frame.stream = stream_;

        // In a pipeline, we don't sync here. We let xInfer sync later.
        return frame;
    }
}