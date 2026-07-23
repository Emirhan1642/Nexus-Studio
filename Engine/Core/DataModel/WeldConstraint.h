#pragma once
#include "Constraint.h"

class WeldConstraint : public Constraint {
public:
    std::string getClassName() const override { return "WeldConstraint"; }

protected:
    void createJoltConstraint() override;
};
