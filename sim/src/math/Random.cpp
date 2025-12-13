#include "sim/math/Random.h"

namespace aegis::sim::math {

    std::mt19937 Random::generator_;

    void Random::init() {
        std::random_device rd;
        generator_.seed(rd());
    }

    double Random::gaussian(double sigma) {
        if (sigma <= 0.0) return 0.0;
        std::normal_distribution<double> dist(0.0, sigma);
        return dist(generator_);
    }

    double Random::uniform(double min, double max) {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(generator_);
    }
}