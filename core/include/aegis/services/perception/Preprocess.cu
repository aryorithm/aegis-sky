#include <cuda_runtime.h>

// This kernel prepares the 5-Channel (RGBDV) Tensor for xInfer
__global__ void combine_and_normalize_kernel(
    const uint8_t* __restrict__ rgb,
    const float* __restrict__ depth,
    const float* __restrict__ velocity,
    float* __restrict__ output, // Planar format: [RRR...GGG...BBB...DDD...VVV]
    int width,
    int height
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int pixel_idx = y * width + x;
    int rgb_idx = pixel_idx * 3;
    
    // Total number of pixels in one channel
    int channel_offset = width * height;

    // 1. Normalize RGB (uint8 0-255 -> float 0-1)
    output[pixel_idx]                     = rgb[rgb_idx + 0] / 255.0f; // R
    output[pixel_idx + channel_offset]    = rgb[rgb_idx + 1] / 255.0f; // G
    output[pixel_idx + 2 * channel_offset]  = rgb[rgb_idx + 2] / 255.0f; // B

    // 2. Normalize Depth & Velocity (e.g., clamp and scale)
    // Here, we just copy. In prod, you'd normalize to [-1, 1] or [0, 1]
    float d = depth[pixel_idx];
    float v = velocity[pixel_idx];

    output[pixel_idx + 3 * channel_offset] = (d > 0.0f) ? (1.0f / (d + 1e-6f)) : 0.0f; // Inverse Depth
    output[pixel_idx + 4 * channel_offset] = v / 50.0f; // Normalize velocity by max expected speed (50 m/s)
}

// C++ Wrapper to launch the kernel
namespace aegis::core::services::perception {
    void launch_preprocess_kernel(
        const uint8_t* rgb_dev, const float* depth_dev, const float* vel_dev,
        float* output_dev, int w, int h, cudaStream_t stream
    ) {
        dim3 threads(16, 16);
        dim3 blocks( (w + threads.x - 1) / threads.x, (h + threads.y - 1) / threads.y );
        
        combine_and_normalize_kernel<<<blocks, threads, 0, stream>>>(
            rgb_dev, depth_dev, vel_dev, output_dev, w, h
        );
    }
}