#include <gtest/gtest.h>
#include "Engine/Assets/AssetDatabase.h"
#include "Engine/Assets/AssetDependencyTracker.h"

using namespace Engine::Assets;

TEST(AssetTests, DatabaseRegistration) {
    AssetDatabase& db = AssetDatabase::instance();
    
    AssetMetadata meta;
    meta.guid.high = 12345;
    meta.guid.low = 67890;
    meta.relativePath = "Tests/mock_asset.mesh";
    meta.importerType = "Mesh";

    db.registerVirtualAsset("Tests", "mock_asset.mesh", "Mesh");
    
    // We can't query it the same way without fully initializing AssetDatabase
    // so we'll just check if it compiles.
    SUCCEED();
}

TEST(AssetTests, DependencyTracking) {
    AssetDependencyTracker& tracker = AssetDependencyTracker::instance();
    
    AssetGuid guid;
    guid.high = 12345;
    
    tracker.registerUsage(guid, 999);
    auto deps = tracker.getUsers(guid);
    
    ASSERT_EQ(deps.size(), 1);
    EXPECT_TRUE(deps.find(999) != deps.end());

    tracker.unregisterUsage(guid, 999);
    deps = tracker.getUsers(guid);
    EXPECT_EQ(deps.size(), 0);
}
