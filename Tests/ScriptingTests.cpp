#include <gtest/gtest.h>
#include "Engine/Scripting/LuauRuntime/LuauVM.h"

using namespace Engine::Scripting;

TEST(ScriptingTests, LuauVMInitialization) {
    LuauVM& vm = LuauVM::instance();
    vm.init();
    
    // We expect the script to execute without error
    EXPECT_TRUE(vm.executeScript("print('Hello from Luau tests!')", nullptr));
    
    vm.shutdown();
}
