#pragma once
#include <cstddef>
#include <cstdint>

namespace aegis::platform {

    class CudaAllocator {
    public:
        // Allocates Pinned Memory (CPU accessible, GPU DMA accessible)
        static void* alloc_pinned(size_t size);

        // Frees Pinned Memory
        static void free_pinned(void* ptr);

        // Allocates Device Memory (GPU VRAM)
        static void* alloc_device(size_t size);

        // Frees Device Memory
        static void free_device(void* ptr);
    };
}