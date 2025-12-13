#include "KalmanFilter.h"
#include <cmath>

namespace aegis::core::services::tracking {

    KalmanFilter::KalmanFilter(float x, float y, float z, double timestamp) {
        state_[0] = x; state_[1] = y; state_[2] = z;
        state_[3] = 0; state_[4] = 0; state_[5] = 0; // Init velocity 0

        // High initial uncertainty for velocity, low for position
        P_[0]=1; P_[1]=1; P_[2]=1;
        P_[3]=100; P_[4]=100; P_[5]=100;

        last_time_ = timestamp;
    }

    void KalmanFilter::predict(double current_time) {
        float dt = static_cast<float>(current_time - last_time_);
        if (dt <= 0) return;

        // F Matrix (State Transition):
        // x' = x + vx*dt
        state_[0] += state_[3] * dt;
        state_[1] += state_[4] * dt;
        state_[2] += state_[5] * dt;

        // P Matrix (Covariance prediction): P = FPF' + Q
        // Uncertainty grows over time
        for(int i=0; i<6; ++i) P_[i] += process_noise_ * dt;

        last_time_ = current_time;
    }

    void KalmanFilter::update(float mx, float my, float mz) {
        // Measurement Vector Z: [mx, my, mz]
        float meas[3] = {mx, my, mz};

        // H Matrix (Observation): We observe x,y,z directly, not velocity
        // K = Kalman Gain

        for (int i = 0; i < 3; ++i) {
            // Innovation (Residual) = Measurement - Predicted
            float y = meas[i] - state_[i];

            // Innovation Covariance S = HPH' + R
            float S = P_[i] + measurement_noise_;

            // Kalman Gain K = PH' * inv(S)
            float K = P_[i] / S;

            // Update State: X = X + K*y
            state_[i] += K * y;

            // Update Velocity (Implicitly via correlation, simplified here)
            // Ideally full matrix math, simplified 1D for performance:
            // Velocity gains info from position shift
            float K_vel = P_[i+3] / S;
            state_[i+3] += K_vel * y; // Update velocity based on position error

            // Update Covariance: P = (I - KH)P
            P_[i] *= (1.0f - K);
            P_[i+3] *= (1.0f - K);
        }
    }

    std::array<float, 3> KalmanFilter::get_position() const {
        return {state_[0], state_[1], state_[2]};
    }

    std::array<float, 3> KalmanFilter::get_velocity() const {
        return {state_[3], state_[4], state_[5]};
    }
}