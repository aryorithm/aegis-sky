#include "sim/phenomenology/optics/MockRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>

namespace aegis::sim::phenomenology {

    MockRenderer::MockRenderer(int width, int height) 
        : width_(width), height_(height), mode_(RenderMode::VISIBLE) {
        
        // Allocate Memory (3 bytes per pixel: R, G, B)
        buffer_.resize(width * height * 3);
        
        // Setup Pinhole Camera Matrix (60 degree FOV)
        // Aspect Ratio, Near Plane 0.1m, Far Plane 2000m
        proj_matrix_ = glm::perspective(glm::radians(60.0), (double)width/height, 0.1, 2000.0);
        
        // Default View: Looking North (Z+)
        set_camera_orientation(glm::dvec3(0, 0, 1));
    }

    void MockRenderer::set_render_mode(RenderMode mode) {
        mode_ = mode;
    }

    void MockRenderer::set_camera_orientation(const glm::dvec3& forward_vector) {
        // Camera Position is fixed at Origin (0,0,0) for the pod
        glm::dvec3 eye(0, 0, 0);
        
        // Target is Eye + Forward Direction
        glm::dvec3 center = eye + forward_vector;
        
        // Up vector is Y+ (Assuming the pod doesn't roll)
        glm::dvec3 up(0, 1, 0);

        view_matrix_ = glm::lookAt(eye, center, up);
    }

    void MockRenderer::clear() {
        // Background Color varies by mode
        uint8_t r = 0, g = 0, b = 0;

        if (mode_ == RenderMode::VISIBLE) {
            // Dark Blue Sky
            r = 10; g = 15; b = 40;
        } else {
            // THERMAL: Sky is extremely cold (Black)
            r = 0; g = 0; b = 0;
        }

        // Fast Fill
        for (size_t i = 0; i < buffer_.size(); i += 3) {
            buffer_[i] = r;
            buffer_[i+1] = g;
            buffer_[i+2] = b;
        }
    }

    void MockRenderer::render_entity(const engine::SimEntity& entity, const glm::dvec3& camera_pos) {
        // 1. World -> Clip Space Transformation
        glm::dvec4 world_pos(entity.get_position(), 1.0);
        glm::dvec4 clip_pos = proj_matrix_ * view_matrix_ * world_pos;

        // 2. Clipping (Is it behind the camera or too close?)
        if (clip_pos.w <= 0.1) return;

        // 3. Perspective Division (NDC)
        glm::dvec3 ndc = glm::dvec3(clip_pos) / clip_pos.w;

        // 4. Viewport Transform (NDC -> Screen Coords)
        // Map [-1, 1] to [0, Width]
        int screen_x = (ndc.x + 1.0) * 0.5 * width_;
        int screen_y = (1.0 - ndc.y) * 0.5 * height_; // Flip Y for raster coords

        // 5. Draw Logic
        // Check bounds (leave 2px margin)
        if (screen_x < 2 || screen_x >= width_ - 2 || screen_y < 2 || screen_y >= height_ - 2) {
            return;
        }

        // Determine Color based on Mode
        uint8_t r, g, b;

        if (mode_ == RenderMode::THERMAL) {
            // Thermal Physics: Map Kelvin to Grayscale
            double temp_k = entity.get_temperature();
            
            // Standard mappings:
            // Sky < 270K (Black)
            // Bird ~ 310K (Dark Grey)
            // Drone ~ 330K (White/Hot)
            
            double min_temp = 280.0;
            double max_temp = 340.0;
            double normalized = std::clamp((temp_k - min_temp) / (max_temp - min_temp), 0.0, 1.0);
            
            uint8_t intensity = static_cast<uint8_t>(normalized * 255.0);
            r = g = b = intensity;
        } 
        else {
            // Visible: Draw White Dot
            // In the future, project the 3D model here. For now, a bright light.
            r = 255; g = 255; b = 255;
        }

        // 6. Rasterize a simple 3x3 box
        for(int y = -1; y <= 1; y++) {
            for(int x = -1; x <= 1; x++) {
                int idx = ((screen_y + y) * width_ + (screen_x + x)) * 3;
                buffer_[idx + 0] = r;
                buffer_[idx + 1] = g;
                buffer_[idx + 2] = b;
            }
        }
    }

    const std::vector<uint8_t>& MockRenderer::get_buffer() const {
        return buffer_;
    }
}