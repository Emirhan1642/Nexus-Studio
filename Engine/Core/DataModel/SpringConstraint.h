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

    bool getLimitsEnabled() const { return limitsEnabled; }
    void setLimitsEnabled(const bool& val) { limitsEnabled = val; recreateConstraint(); }

    float getMinLength() const { return minLength; }
    void setMinLength(const float& val) { minLength = val; recreateConstraint(); }

    float getMaxLength() const { return maxLength; }
    void setMaxLength(const float& val) { maxLength = val; recreateConstraint(); }

protected:
    void createJoltConstraint() override;

private:
    float freeLength = 2.0f;
    float stiffness = 10.0f;
    float damping = 2.0f;
    bool limitsEnabled = false;
    float minLength = 0.0f;
    float maxLength = 10.0f;
};
