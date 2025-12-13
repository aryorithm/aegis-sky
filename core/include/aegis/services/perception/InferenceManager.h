#pragma once
#include "aegis/services/fusion/FusionEngine.h" // For FusedFrame
#include "Detection.h"
#include <string>
#include <vector>
#include <memory>

// Forward declare your xInfer Engine class
namespace xinfer { class Engine; }

namespace aegis::core::services {

    class InferenceManager {
    public:
        // Loads the .plan file and allocates GPU memory
        InferenceManager(const std::string& engine_path);
        ~InferenceManager();

        // The main detection function
        std::vector<Detection> detect(const FusedFrame& frame);

    private:
        // Your proprietary xInfer Engine
        std::unique_ptr<xinfer::Engine> engine_;

        cudaStream_t stream_;
        
        // GPU Buffers
        void* d_bindings_[2]; // 0: Input, 1: Output
        float* d_input_tensor_; // The 5-Channel Tensor
        
        // CPU buffer for results
        std::vector<Detection> host_detections_;
    };
}