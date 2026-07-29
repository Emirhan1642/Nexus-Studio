#pragma once
#include "Constraint.h"

class HingeConstraint : public Constraint {
public:
    std::string getClassName() const override { return "HingeConstraint"; }
    Engine::Math::Vector3 getPivot() const { return pivot; }
    void setPivot(const Engine::Math::Vector3& val) { pivot = val; recreateConstraint(); }

    Engine::Math::Vector3 getAxis() const { return axis; }
    void setAxis(const Engine::Math::Vector3& val) { axis = val; recreateConstraint(); }

    bool getLimitsEnabled() const { return limitsEnabled; }
    void setLimitsEnabled(const bool& val) { limitsEnabled = val; recreateConstraint(); }

    float getLowerLimit() const { return lowerLimit; }
    void setLowerLimit(const float& val) { lowerLimit = val; recreateConstraint(); }

    float getUpperLimit() const { return upperLimit; }
    void setUpperLimit(const float& val) { upperLimit = val; recreateConstraint(); }

protected:
    void createJoltConstraint() override;

private:
    Engine::Math::Vector3 pivot{0.0f, 0.0f, 0.0f};
    Engine::Math::Vector3 axis{0.0f, 1.0f, 0.0f};
    bool limitsEnabled = false;
    float lowerLimit = -3.14159f;
    float upperLimit = 3.14159f;
};
