#include "sim/engine/WeatherSystem.h"
#include <cmath>

namespace aegis::sim::physics {

    class LaserPhysics {
    public:
        // Returns Joules delivered to target in 'dt' seconds
        static double calculate_damage(
            double range_m, 
            double beam_power_watts, 
            const engine::WeatherState& weather, 
            double dt
        ) {
            // 1. Beam Divergence (Spot size increases with range)
            // Area = pi * (range * divergence)^2
            double divergence = 0.0001; // rad
            double spot_radius = 0.02 + (range_m * divergence); // 2cm starting aperture
            double spot_area = M_PI * spot_radius * spot_radius;

            // 2. Atmospheric Extinction (Beer-Lambert Law)
            // Fog causes massive scattering
            double extinction_coeff = 0.0001 + (weather.fog_density * 0.1); 
            double transmission = std::exp(-extinction_coeff * range_m);

            // 3. Power Density on Target (Watts / m^2)
            double power_on_target = (beam_power_watts * transmission) / spot_area;

            // 4. Coupling Coefficient (How much heat is absorbed vs reflected)
            // White drones reflect more than black drones
            double absorption = 0.4; 

            // Joules = Watts * Seconds * Absorption
            // NOTE: We assume the spot is fully on the drone. 
            // In reality, we multiply by (DroneArea / SpotArea) if Spot > Drone.
            return beam_power_watts * transmission * absorption * dt; 
        }
    };
}