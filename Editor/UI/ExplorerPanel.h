#pragma once
#include <memory>
class Instance;

class ExplorerPanel {
public:
    void draw();

private:
    void drawInstanceNode(const std::shared_ptr<Instance>& inst);
    void drawInsertObjectMenu(const std::shared_ptr<Instance>& parent);
};
