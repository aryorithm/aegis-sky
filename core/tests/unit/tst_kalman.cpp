#include <gtest/gtest.h>
#include "services/tracking/KalmanFilter.h"

TEST(KalmanFilterTest, PredictsLinearTrajectory) {
    // 1. Setup
    aegis::core::services::tracking::KalmanFilter kf(0, 0, 0, 0.0);
    kf.update(0, 0, 0); // Initial measurement
    kf.update(1, 0, 0); // Second measurement -> implies vx=1 m/s

    // 2. Action
    kf.predict(2.0); // Predict 1 second into the future

    // 3. Assert
    auto pos = kf.get_position();
    // It should predict the position will be at x=2
    EXPECT_NEAR(pos[0], 2.0, 0.1);

    auto vel = kf.get_velocity();
    // The filter should have figured out the velocity is ~1 m/s
    EXPECT_NEAR(vel[0], 1.0, 0.1);
}