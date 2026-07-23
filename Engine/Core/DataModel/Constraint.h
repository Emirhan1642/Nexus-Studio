#pragma once
#include "Instance.h"
#include "Part.h"
#include <memory>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Constraints/Constraint.h>

class Constraint : public Instance {
public:
    std::string getClassName() const override { return "Constraint"; }

    virtual ~Constraint() override;

    bool getVisible() const { return visible; }
    void setVisible(const bool& val) { visible = val; }

    bool getEnabled() const { return enabled; }
    void setEnabled(const bool& val);

    std::shared_ptr<Instance> getPart0() const { return part0.lock(); }
    void setPart0(const std::shared_ptr<Instance>& p);

    std::shared_ptr<Instance> getPart1() const { return part1.lock(); }
    void setPart1(const std::shared_ptr<Instance>& p);

    void onAddedToWorkspace() override;
    void onRemovedFromWorkspace() override;

protected:
    virtual void createJoltConstraint() = 0;
    void destroyJoltConstraint();
    void recreateConstraint();

    std::weak_ptr<Instance> part0;
    std::weak_ptr<Instance> part1;
    bool visible = true;
    bool enabled = true;
    bool inWorkspace = false;

    JPH::Ref<JPH::Constraint> joltConstraint = nullptr;
};
