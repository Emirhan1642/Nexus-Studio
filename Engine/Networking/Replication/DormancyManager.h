#pragma once
#include <unordered_map>
#include <algorithm>

namespace Engine::Networking {

    using InstanceId = uint64_t;

    class DormancyManager {
    public:
        void onInstanceDirty(InstanceId id, float currentServerTime) {
            dormantInstances.erase(id);
            lastDirtyTime[id] = currentServerTime;
        }

        template<typename SpatialGridType>
        void updateDormancy(float currentTime, SpatialGridType& spatialGrid) {
            for (auto& [id, lastDirty] : lastDirtyTime) {
                if (currentTime - lastDirty > DORMANCY_THRESHOLD && !dormantInstances.contains(id)) {
                    dormantInstances.insert(id);
                    spatialGrid.removeFromActiveQueries(id);
                }
            }
        }

        bool isDormant(InstanceId id) const {
            return dormantInstances.contains(id);
        }

    private:
        static constexpr float DORMANCY_THRESHOLD = 30.0f;
        std::unordered_set<InstanceId> dormantInstances;
        std::unordered_map<InstanceId, float> lastDirtyTime;
    };

}
