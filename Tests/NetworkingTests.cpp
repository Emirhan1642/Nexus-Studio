#include <gtest/gtest.h>
#include "../Engine/Core/DataModel/InstanceRegistry.h"
#include "../Engine/Core/DataModel/RemoteEvent.h"
#include "../Engine/Networking/Transport/NetworkServer.h"
#include "../Engine/Networking/Transport/NetworkClient.h"
#include "../Engine/Networking/Transport/NetworkContext.h"
#include "../Engine/Networking/Replication/ReplicationManager.h"
#include "../Engine/Core/Reflection/TypeRegistry.h"
#include "../Engine/Core/DataModel/Part.h"
#include "../build/Engine/Networking/Messages.pb.h"
#include "../Engine/Core/DataModel/Humanoid.h"
#include "../Engine/Networking/Prediction/HumanoidPredictor.h"
#include <thread>
#include <chrono>
#include <unordered_set>

#include "TestEnvironment.h"

using namespace Engine;
using namespace Engine::Networking;

class NetworkingTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensureTestEnvironmentInitialized();
        // Initialize reflection if needed
        RemoteEvent::registerClass();
    }

    void TearDown() override {
        NetworkServer::instance().stop();
        NetworkClient::instance().disconnect();
        InstanceRegistry::instance().clear();
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

TEST_F(NetworkingTest, TestInterestManagementSpatialCulling) {
    // 1. Start Server & Client
    ASSERT_TRUE(NetworkServer::instance().start(12346));
    ASSERT_TRUE(NetworkClient::instance().connect("127.0.0.1", 12346));

    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        NetworkClient::instance().poll();
        NetworkServer::instance().poll();
    }

    auto clients = NetworkServer::instance().getClients();
    ASSERT_FALSE(clients.empty());
    uint32_t clientId = clients[0].id;

    // 2. Set up Player Character (Position: 0, 0, 0)
    auto typeDesc = Engine::Reflection::TypeRegistry::instance().find("Part");
    ASSERT_NE(typeDesc, nullptr) << "Part class not registered in reflection!";

    auto playerChar = std::make_shared<Part>();
    playerChar->name = "PlayerCharacter";
    playerChar->setPosition({0, 0, 0});
    InstanceRegistry::instance().registerInstance(playerChar);

    NetworkServer::instance().setPlayerCharacter(clientId, playerChar->getInstanceId());

    // 3. Set up Close Part (Position: 50, 0, 0) - within 300 Enter Radius
    auto closePart = std::make_shared<Part>();
    closePart->name = "ClosePart";
    closePart->setPosition({50, 0, 0});
    InstanceRegistry::instance().registerInstance(closePart);

    // 4. Set up Far Part (Position: 500, 0, 0) - outside 300 Enter Radius
    auto farPart = std::make_shared<Part>();
    farPart->name = "FarPart";
    farPart->setPosition({500, 0, 0});
    InstanceRegistry::instance().registerInstance(farPart);

    // 5. Intercept Client Packets
    std::unordered_set<InstanceId> replicatedToClient;
    NetworkClient::instance().setPacketHandler([&](const uint8_t* data, size_t size, NetChannel channel) {
        Proto::NetworkPacket packet;
        if (packet.ParseFromArray(data, size)) {
            if (packet.has_replication()) {
                for (int i = 0; i < packet.replication().updates_size(); ++i) {
                    std::cout << "[TestClient] Replicated instance: " << packet.replication().updates(i).instance_id() << std::endl;
                    replicatedToClient.insert(packet.replication().updates(i).instance_id());
                }
            }
        }
    });

    // 6. Trigger Replication
    NetworkContext::setMode(NetworkMode::Server);
    ReplicationManager::instance().markPropertyDirty(closePart->getInstanceId(), "Position");
    ReplicationManager::instance().markPropertyDirty(farPart->getInstanceId(), "Position");

    ReplicationManager::instance().flushToAllClients(0.1f); // Trigger Relevancy Check (Action::Create)
    ReplicationManager::instance().flushToAllClients(0.1f); // Trigger Initial Sync Sending

    // 7. Pump network
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        NetworkServer::instance().poll();
        NetworkClient::instance().poll();
    }

    // 8. Verify
    NetworkClient::instance().setPacketHandler(nullptr); // restore

    EXPECT_TRUE(replicatedToClient.contains(closePart->getInstanceId()));
    EXPECT_FALSE(replicatedToClient.contains(farPart->getInstanceId()));
}

TEST_F(NetworkingTest, TestClientSidePrediction) {
    // 1. Start Server & Client
    ASSERT_TRUE(NetworkServer::instance().start(12347));
    ASSERT_TRUE(NetworkClient::instance().connect("127.0.0.1", 12347));

    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        NetworkClient::instance().poll();
        NetworkServer::instance().poll();
    }

    auto clients = NetworkServer::instance().getClients();
    ASSERT_FALSE(clients.empty());
    uint32_t clientId = clients[0].id;

    // 2. Setup Player Character & Predictor
    auto playerChar = std::make_shared<Engine::Humanoid>();
    playerChar->name = "PredictorCharacter";
    InstanceRegistry::instance().registerInstance(playerChar);

    auto rootPart = std::make_shared<Part>();
    rootPart->name = "HumanoidRootPart";
    rootPart->setPosition({0, 10, 0});
    InstanceRegistry::instance().registerInstance(rootPart);
    playerChar->setParent(rootPart);

    NetworkServer::instance().setPlayerCharacter(clientId, playerChar->getInstanceId());

    auto localPredictor = std::make_shared<HumanoidPredictor>(playerChar);
    NetworkClient::instance().setLocalPredictor(localPredictor);

    // 3. Send Local Input (Prediction)
    std::cout << "[Test] Sending local input..." << std::endl;
    Engine::Math::Vector3 moveDir(1, 0, 0); // Move +X
    localPredictor->onLocalInput(moveDir, false, 0.16f);
    
    // We expect some pending commands now
    EXPECT_EQ(localPredictor->getPendingCommandsCount(), size_t(1));

    // 4. Pump Network to send the command to server and get the snapshot back
    std::cout << "[Test] Pumping network..." << std::endl;
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        NetworkClient::instance().poll(); 
        NetworkServer::instance().poll(); 
        if (localPredictor->getPendingCommandsCount() == 0) {
            std::cout << "[Test] Pending commands cleared at iteration " << i << std::endl;
            break;
        }
    }

    std::cout << "[Test] Verifying reconciliation..." << std::endl;
    // 5. Verify Reconciliation
    EXPECT_EQ(localPredictor->getPendingCommandsCount(), size_t(0));
    
    // Cleanup NetworkClient state
    NetworkClient::instance().setLocalPredictor(nullptr);
}
