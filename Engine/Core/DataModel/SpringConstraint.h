#pragma once
#include "Constraint.h"

class SpringConstraint : public Constraint {
public:
    std::string getClassName() const override { return "SpringConstraint"; }

    float getFreeLength() const { return freeLength; }
    void setFreeLength(const float& val) { freeLength = val; recreateConstraint(); }

    float getStiffness() const { return stiffness; }
    void setStiffness(const float& val) { stiffness = val; recreateConstraint(); }

    float getDamping() const { return damping; }
    void setDamping(const float& val) { damping = val; recreateConstraint(); }

protected:
    void createJoltConstraint() override;

private:
    float freeLength = 2.0f;
    float stiffness = 10.0f;
    float damping = 2.0f;
};
