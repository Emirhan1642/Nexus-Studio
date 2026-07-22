#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/Body.h>
#include <mutex>
#include <vector>
#include <cstdint>

namespace Engine::Physics {

struct ContactEvent {
    uint64_t id1;
    uint64_t id2;
};

class PendingContactEvents {
public:
    static PendingContactEvents& instance() {
        static PendingContactEvents instance;
        return instance;
    }

    void enqueue(const ContactEvent& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(event);
    }

    std::vector<ContactEvent> drainAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ContactEvent> events = std::move(m_events);
        m_events.clear();
        return events;
    }

private:
    std::mutex m_mutex;
    std::vector<ContactEvent> m_events;
};

class ContactListenerImpl : public JPH::ContactListener {
public:
    virtual void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
                                const JPH::ContactManifold& manifold,
                                JPH::ContactSettings& settings) override {
        uint64_t id1 = body1.GetUserData();
        uint64_t id2 = body2.GetUserData();

        if (id1 != 0 && id2 != 0) {
            PendingContactEvents::instance().enqueue({id1, id2});
        }
    }
};

} // namespace Engine::Physics
