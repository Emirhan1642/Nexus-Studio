#pragma once
#include "RelevancyTracker.h"
#include <unordered_map>
#include <algorithm>

namespace Engine::Networking {

    class PriorityCalculator {
    public:
        static constexpr float MAX_STALE_TIME = 5.0f; // Seconds

        float calculate(float distance, float velocityLength, float timeSinceLastSent) const {
            float distanceFactor = 1.0f - std::clamp(distance / RelevancyTracker::EXIT_RADIUS, 0.0f, 1.0f);
            float velocityFactor = std::clamp(velocityLength / 50.0f, 0.0f, 1.0f);
            float timeSinceLastSentFactor = std::clamp(timeSinceLastSent / MAX_STALE_TIME, 0.0f, 1.0f);

            return distanceFactor * 0.5f + velocityFactor * 0.3f + timeSinceLastSentFactor * 0.2f;
        }
    };

}
