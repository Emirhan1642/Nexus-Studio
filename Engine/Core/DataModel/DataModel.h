#pragma once
#include "Instance.h"

// Roblox'taki "game" degiskenine karsilik gelir — tum sahnenin koku
class DataModel : public Instance {
public:
    DataModel() { name = "DataModel"; }

    std::shared_ptr<Instance> workspace;   
    std::shared_ptr<Instance> playerService;

    static std::shared_ptr<DataModel> instance() {
        static std::shared_ptr<DataModel> dm = std::make_shared<DataModel>();
        return dm;
    }
};

// Registration (Normally in a cpp file, but inline here for brevity)
namespace {
    struct DataModelReflectionInit {
        DataModelReflectionInit() {
            using namespace Engine::Reflection;
            ClassBuilder<DataModel>("DataModel")
                .base("Instance");
        }
    } g_dataModelReflectionInit;
}
