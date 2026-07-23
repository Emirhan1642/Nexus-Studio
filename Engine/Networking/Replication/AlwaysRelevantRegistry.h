#pragma once
#include <unordered_set>

namespace Engine::Networking {

    using InstanceId = uint64_t;

    class AlwaysRelevantRegistry {
    public:
        static AlwaysRelevantRegistry& instance() {
            static AlwaysRelevantRegistry s_instance;
            return s_instance;
        }

        void add(InstanceId id) {
            m_instances.insert(id);
        }

        void remove(InstanceId id) {
            m_instances.erase(id);
        }

        const std::unordered_set<InstanceId>& getAll() const {
            return m_instances;
        }

    private:
        AlwaysRelevantRegistry() = default;
        std::unordered_set<InstanceId> m_instances;
    };

}
