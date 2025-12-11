#pragma once

namespace aegis::sim::engine {

    struct WeatherState {
        double rain_intensity = 0.0; // mm/hr (0=Clear, 10=Heavy, 50=Monsoon)
        double fog_density = 0.0;    // 0.0 to 1.0 (Visibility reduction)
        double wind_speed = 0.0;     // m/s
    };

    class WeatherSystem {
    public:
        void set_condition(double rain, double fog, double wind) {
            state_.rain_intensity = rain;
            state_.fog_density = fog;
            state_.wind_speed = wind;
        }

        const WeatherState& get_state() const { return state_; }

        // Returns Radar Attenuation in dB (Loss per km)
        // Based on ITU-R P.838 for X-Band Radar (10GHz)
        double get_radar_attenuation_db() const {
            if (state_.rain_intensity <= 0) return 0.0;
            // Simplified approximation: 0.01 dB * RainRate
            return 0.02 * state_.rain_intensity; 
        }

    private:
        WeatherState state_;
    };
}