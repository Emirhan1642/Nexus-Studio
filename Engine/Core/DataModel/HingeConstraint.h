#pragma once
#include "Constraint.h"

class HingeConstraint : public Constraint {
public:
    std::string getClassName() const override { return "HingeConstraint"; }

protected:
    void createJoltConstraint() override;
};
