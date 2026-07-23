#pragma once
#include "../../Core/Math/Vector3.h"
#include <deque>
#include <cstdint>

namespace Engine::Networking {

    struct PredictionState {
        uint32_t sequenceNumber = 0;
        Math::Vector3 position;
        // Other inputs like velocity, jump state etc.
    };

    class ClientPredictor {
    public:
        static constexpr float ERROR_THRESHOLD = 0.1f; // Max acceptable deviation

        void recordLocalState(uint32_t sequence, const Math::Vector3& position);
        
        // Returns true if a reconciliation (correction) is needed
        bool onServerUpdate(uint32_t serverSequence, const Math::Vector3& serverPosition, Math::Vector3& outCorrectedPos);

    private:
        std::deque<PredictionState> m_history;
    };

}
