#pragma once
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace Engine::Networking {

    using InstanceId = uint64_t;

    struct ClientRelevancyState {
        std::unordered_set<InstanceId> relevantInstances;
        std::vector<InstanceId> pendingInitialSync;
    };

    class RelevancyTracker {
    public:
        static constexpr float ENTER_RADIUS = 300.0f;
        static constexpr float EXIT_RADIUS = 400.0f;

        enum class Action {
            None,
            Create,
            Destroy
        };

        Action update(ClientRelevancyState& state, InstanceId id, float distance) {
            bool currentlyRelevant = state.relevantInstances.contains(id);

            if (!currentlyRelevant && distance < ENTER_RADIUS) {
                state.relevantInstances.insert(id);
                return Action::Create;
            }
            else if (currentlyRelevant && distance > EXIT_RADIUS) {
                state.relevantInstances.erase(id);
                return Action::Destroy;
            }
            
            return Action::None;
        }
    };

}
