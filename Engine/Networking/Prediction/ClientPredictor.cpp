#define NOMINMAX
#include "ClientPredictor.h"
#include <algorithm>
#include <iostream>

namespace Engine::Networking {

    void ClientPredictor::recordLocalState(uint32_t sequence, const Math::Vector3& position) {
        m_history.push_back({sequence, position});
        
        // Keep a bounded history
        if (m_history.size() > 120) {
            m_history.pop_front();
        }
    }

    bool ClientPredictor::onServerUpdate(uint32_t serverSequence, const Math::Vector3& serverPosition, Math::Vector3& outCorrectedPos) {
        // Discard history older than serverSequence
        while (!m_history.empty() && m_history.front().sequenceNumber < serverSequence) {
            m_history.pop_front();
        }

        if (m_history.empty()) {
            return false; // No history to compare against
        }

        // The front of the queue is exactly the state we predicted for the serverSequence
        if (m_history.front().sequenceNumber == serverSequence) {
            PredictionState predicted = m_history.front();
            m_history.pop_front();

            float errorDistance = (predicted.position - serverPosition).length();
            if (errorDistance > ERROR_THRESHOLD) {
                // Server and Client diverged. We must snap to server state.
                outCorrectedPos = serverPosition;
                
                // Usually, you would replay the remaining history (unacknowledged inputs) here
                // For simplicity, we just return true to snap the object.
                std::cout << "[ClientPredictor] Reconciliation triggered! Error: " << errorDistance << "\n";
                return true;
            }
        }

        return false;
    }

}
