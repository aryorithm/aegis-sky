#pragma once
#include "sim/engine/SimEntity.h"
#include <vector>
#include <glm/glm.hpp>

namespace aegis::sim::physics {

    struct BoidConfig {
        double separation_radius = 5.0;
        double view_radius = 50.0;
        double separation_weight = 1.5;
        double alignment_weight = 1.0;
        double cohesion_weight = 1.0;
    };

    class SwarmPhysics {
    public:
        static glm::dvec3 calculate_flocking_force(
            const engine::SimEntity& self,
            const std::vector<std::shared_ptr<engine::SimEntity>>& neighbors,
            const BoidConfig& config
        ) {
            glm::dvec3 sep(0), ali(0), coh(0);
            int count = 0;

            for (const auto& other : neighbors) {
                if (&self == other.get()) continue; // Don't check self
                
                double dist = glm::distance(self.get_position(), other->get_position());
                
                if (dist > 0 && dist < config.view_radius) {
                    // 1. Separation
                    if (dist < config.separation_radius) {
                        glm::dvec3 diff = self.get_position() - other->get_position();
                        sep += glm::normalize(diff) / dist;
                    }
                    
                    // 2. Alignment
                    ali += other->get_velocity();

                    // 3. Cohesion
                    coh += other->get_position();
                    
                    count++;
                }
            }

            if (count > 0) {
                // Average and Normalize
                ali = glm::normalize(ali / (double)count) * config.alignment_weight;
                
                coh = (coh / (double)count) - self.get_position();
                coh = glm::normalize(coh) * config.cohesion_weight;
                
                sep = glm::normalize(sep) * config.separation_weight;
            }

            return sep + ali + coh;
        }
    };
}