#include "sim/phenomenology/optics/MockRenderer.h"
#include <glm/gtc/matrix_transform.hpp>

namespace aegis::sim::phenomenology {

    MockRenderer::MockRenderer(int width, int height) 
        : width_(width), height_(height) {
        
        // Allocate Memory (3 bytes per pixel)
        buffer_.resize(width * height * 3);
        
        // Setup simple Pinhole Camera Matrix (60 degree FOV)
        // 0.1 near plane, 1000.0 far plane
        proj_matrix_ = glm::perspective(glm::radians(60.0), (double)width/height, 0.1, 1000.0);
    }

    void MockRenderer::clear() {
        // Fill with black (0) or dark blue (20) for sky
        std::fill(buffer_.begin(), buffer_.end(), 20); 
    }

    void MockRenderer::render_entity(const engine::SimEntity& entity, const glm::dvec3& camera_pos) {
        // 1. Update View Matrix (Camera looking at Z+)
        // In a real sim, the camera rotates. Here it's fixed looking forward.
        view_matrix_ = glm::lookAt(camera_pos, camera_pos + glm::dvec3(0,0,1), glm::dvec3(0,1,0));

        // 2. World -> Clip Space
        glm::dvec4 world_pos(entity.get_position(), 1.0);
        glm::dvec4 clip_pos = proj_matrix_ * view_matrix_ * world_pos;

        // 3. Clip Check (Is it behind the camera?)
        if (clip_pos.w <= 0.0) return;

        // 4. Normalized Device Coordinates (NDC)
        glm::dvec3 ndc = glm::dvec3(clip_pos) / clip_pos.w;

        // 5. Screen Space
        // Map [-1, 1] to [0, Width]
        int screen_x = (ndc.x + 1.0) * 0.5 * width_;
        int screen_y = (1.0 - ndc.y) * 0.5 * height_; // Flip Y for image coords

        // 6. Draw "Drone" (A simple 3x3 white box)
        if (screen_x > 1 && screen_x < width_-1 && screen_y > 1 && screen_y < height_-1) {
            for(int y=-1; y<=1; y++) {
                for(int x=-1; x<=1; x++) {
                    int idx = ((screen_y+y) * width_ + (screen_x+x)) * 3;
                    buffer_[idx + 0] = 255; // R
                    buffer_[idx + 1] = 255; // G
                    buffer_[idx + 2] = 255; // B
                }
            }
        }
    }

    const std::vector<uint8_t>& MockRenderer::get_buffer() const {
        return buffer_;
    }
}