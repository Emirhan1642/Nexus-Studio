#include "Constraint.h"
#include "../../Physics/PhysicsWorld.h"

Constraint::~Constraint() {
    destroyJoltConstraint();
}

void Constraint::setEnabled(const bool& val) {
    if (enabled == val) return;
    enabled = val;
    recreateConstraint();
}

void Constraint::setPart0(const std::shared_ptr<Instance>& p) {
    part0 = p;
    recreateConstraint();
}

void Constraint::setPart1(const std::shared_ptr<Instance>& p) {
    part1 = p;
    recreateConstraint();
}

void Constraint::onAddedToWorkspace() {
    inWorkspace = true;
    recreateConstraint();
}

void Constraint::onRemovedFromWorkspace() {
    inWorkspace = false;
    destroyJoltConstraint();
}

void Constraint::destroyJoltConstraint() {
    if (joltConstraint) {
        Engine::Physics::PhysicsWorld::instance().removeConstraint(joltConstraint);
        joltConstraint = nullptr;
    }
}

void Constraint::recreateConstraint() {
    destroyJoltConstraint();
    if (enabled && inWorkspace) {
        auto p0 = part0.lock();
        auto p1 = part1.lock();
        if (p0 && p1) {
            createJoltConstraint();
        }
    }
}

#include "../Reflection/ClassBuilder.h"
namespace {
    struct ConstraintReflectionInit {
        ConstraintReflectionInit() {
            using namespace Engine::Reflection;
            ClassBuilder<Constraint>("Constraint")
                .base("Instance")
                .propertyAccessor("Visible", &Constraint::getVisible, &Constraint::setVisible).category("Appearance")
                .propertyAccessor("Enabled", &Constraint::getEnabled, &Constraint::setEnabled).category("Behavior")
                .objectPropertyAccessor("Part0", &Constraint::getPart0, &Constraint::setPart0).category("Data")
                .objectPropertyAccessor("Part1", &Constraint::getPart1, &Constraint::setPart1).category("Data");
        }
    } g_constraintReflectionInit;
}
