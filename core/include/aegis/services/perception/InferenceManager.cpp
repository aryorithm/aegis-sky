#include "InferenceManager.h"
#include "aegis/platform/CudaAllocator.h"
#include "xinfer/Engine.h" // Your library's main header
#include <spdlog/spdlog.h>

// Forward declare the CUDA kernel wrappers
namespace aegis::core::services::perception {
    void launch_preprocess_kernel(
        const uint8_t* rgb, const float* d, const float* v,
        float* out, int w, int h, cudaStream_t s
    );

    // (In prod, you'd have a Postprocess.cu for NMS. For MVP, we do it on CPU)
}

namespace aegis::core::services {

    using namespace platform;

    InferenceManager::InferenceManager(const std::string& engine_path) {
        cudaStreamCreate(&stream_);

        // 1. Load xInfer Engine
        // This API is an assumption based on your design
        engine_ = xinfer::Engine::load(engine_path);
        if (!engine_) {
            throw std::runtime_error("Failed to load xInfer engine: " + engine_path);
        }

        // 2. Allocate GPU Buffers
        // Input (5 Channels, 1080p, float)
        size_t input_size = 5 * 1920 * 1080 * sizeof(float);
        d_input_tensor_ = (float*)CudaAllocator::alloc_device(input_size);

        // Output (Assuming model outputs [N x 6] -> [Box, Conf, Class])
        // Let's assume max 100 detections
        size_t output_size = 100 * 6 * sizeof(float);
        void* d_output_buffer = CudaAllocator::alloc_device(output_size);

        // Bind buffers to the engine
        d_bindings_[0] = d_input_tensor_;
        d_bindings_[1] = d_output_buffer;

        // Allocate pinned memory for async CPU copy
        host_detections_.resize(100);

        spdlog::info("[Perception] xInfer Engine '{}' loaded and ready.", engine_path);
    }

    InferenceManager::~InferenceManager() {
        CudaAllocator::free_device(d_bindings_[0]);
        CudaAllocator::free_device(d_bindings_[1]);
        cudaStreamDestroy(stream_);
    }

    std::vector<Detection> InferenceManager::detect(const FusedFrame& frame) {
        // 1. PRE-PROCESSING (Fusion -> 5-Channel Tensor)
        perception::launch_preprocess_kernel(
            frame.rgb, frame.depth, frame.velocity,
            d_input_tensor_, frame.width, frame.height, stream_
        );

        // 2. INFERENCE (Run the Neural Network)
        // The core call to your proprietary library
        engine_->infer(d_bindings_, stream_);

        // 3. POST-PROCESSING (Decode Output)
        // For MVP, we copy results to CPU and parse.
        // In prod, a CUDA kernel for Non-Maximum Suppression (NMS) is faster.

        // Copy output from GPU to CPU
        size_t output_size = host_detections_.capacity() * sizeof(Detection); // Simple size calc
        cudaMemcpyAsync(host_detections_.data(), d_bindings_[1], output_size, cudaMemcpyDeviceToHost, stream_);

        // WAIT for all GPU work to finish
        cudaStreamSynchronize(stream_);

        // Filter results on CPU
        std::vector<Detection> final_detections;
        for (const auto& det : host_detections_) {
            // Your model's output format determines this logic
            // Assuming confidence is the 5th element
            if (det.confidence > 0.5f) { // Confidence Threshold
                final_detections.push_back(det);
            }
        }

        return final_detections;
    }
}