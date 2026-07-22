#pragma once
#include "../Core/DataModel/Instance.h"
#include <string>

class Script : public Instance {
public:
    Script() {
        name = "Script";
    }

    std::string getClassName() const override { return "Script"; }

    std::string source;

    void onAddedToWorkspace() override;
    void onRemovedFromWorkspace() override;

private:
    bool m_hasRun = false;
};
