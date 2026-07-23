#pragma once
#include "../../Core/Math/Vector3.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>

namespace Engine::Networking {

    using InstanceId = uint64_t;

    struct GridCoord {
        int x, z;
        bool operator==(const GridCoord& o) const { return x == o.x && z == o.z; }
    };

    struct GridCoordHash {
        size_t operator()(const GridCoord& c) const { 
            return std::hash<int64_t>()(((int64_t)c.x << 32) | (uint32_t)c.z); 
        }
    };

    class SpatialGrid {
    public:
        static constexpr float CELL_SIZE = 100.0f;

        GridCoord worldToGrid(const Math::Vector3& pos) const {
            return { static_cast<int>(std::floor(pos.x / CELL_SIZE)), 
                     static_cast<int>(std::floor(pos.z / CELL_SIZE)) };
        }

        void updateInstancePosition(InstanceId id, const Math::Vector3& newPos) {
            if (activeInstances.find(id) == activeInstances.end()) {
                activeInstances.insert(id);
            }

            GridCoord newCoord = worldToGrid(newPos);
            
            auto it = instanceToCoord.find(id);
            if (it != instanceToCoord.end()) {
                if (it->second == newCoord) return;
                cells[it->second].erase(id);
            }

            cells[newCoord].insert(id);
            instanceToCoord[id] = newCoord;
        }

        void removeInstance(InstanceId id) {
            activeInstances.erase(id);
            auto it = instanceToCoord.find(id);
            if (it != instanceToCoord.end()) {
                cells[it->second].erase(id);
                instanceToCoord.erase(it);
            }
        }

        void removeFromActiveQueries(InstanceId id) {
            removeInstance(id);
        }

        std::vector<InstanceId> queryRadius(const Math::Vector3& center, int cellRadius) const {
            std::vector<InstanceId> result;
            GridCoord centerCoord = worldToGrid(center);

            for (int dx = -cellRadius; dx <= cellRadius; dx++) {
                for (int dz = -cellRadius; dz <= cellRadius; dz++) {
                    auto it = cells.find({centerCoord.x + dx, centerCoord.z + dz});
                    if (it != cells.end()) {
                        result.insert(result.end(), it->second.begin(), it->second.end());
                    }
                }
            }
            return result;
        }

    private:
        std::unordered_set<InstanceId> activeInstances;
        std::unordered_map<GridCoord, std::unordered_set<InstanceId>, GridCoordHash> cells;
        std::unordered_map<InstanceId, GridCoord> instanceToCoord;
    };

}
