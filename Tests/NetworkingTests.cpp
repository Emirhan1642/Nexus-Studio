#include <gtest/gtest.h>
#include "../Engine/Core/DataModel/InstanceRegistry.h"
#include "../Engine/Core/DataModel/RemoteEvent.h"
#include "../Engine/Networking/Transport/NetworkServer.h"
#include "../Engine/Networking/Transport/NetworkClient.h"
#include "../Engine/Networking/Transport/NetworkContext.h"
#include "../Engine/Networking/Replication/ReplicationManager.h"
#include <thread>
#include <chrono>

using namespace Engine::Networking;

class NetworkingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize reflection if needed
        RemoteEvent::registerClass();
    }

    void TearDown() override {
        NetworkServer::instance().stop();
        NetworkClient::instance().disconnect();
        InstanceRegistry::instance().clear();
        GameNetworkingSockets_Kill();
    }
};

TEST_F(NetworkingTest, TestRemoteEventClientToServer) {
    // 1. Start Server
    ASSERT_TRUE(NetworkServer::instance().start(12345));

    // 2. Start Client and connect
    ASSERT_TRUE(NetworkClient::instance().connect("127.0.0.1", 12345));

    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    NetworkClient::instance().poll();
    NetworkServer::instance().poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    NetworkClient::instance().poll();
    NetworkServer::instance().poll();

    // 3. Create a RemoteEvent
    auto re = std::make_shared<RemoteEvent>();
    re->name = "MyRemoteEvent";
    InstanceRegistry::instance().registerInstance(re);

    bool serverReceived = false;
    std::string receivedString = "";

    // 4. Hook up server listener
    re->connectServerEvent([&](const std::vector<std::any>& args) {
        if (!args.empty() && args[0].type() == typeid(std::string)) {
            serverReceived = true;
            receivedString = std::any_cast<std::string>(args[0]);
        }
    });

    // 5. Fire from client
    NetworkContext::setMode(NetworkMode::Client);
    re->FireServer({std::string("Hello Server!")});

    // 6. Process packets
    for (int i = 0; i < 10; ++i) {
        NetworkClient::instance().poll(); // Flush client out
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        NetworkServer::instance().poll(); // Read server in
        if (serverReceived) break;
    }

    ASSERT_TRUE(serverReceived);
    EXPECT_EQ(receivedString, "Hello Server!");
}
