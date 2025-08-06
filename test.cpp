#include "Server.hpp"
#include <iostream>
#include <cassert>

// Helper function to print test results
void printTest(const std::string& testName, bool passed) {
    std::cout << "[" << testName << "] " << (passed ? "✅ PASSED" : "❌ FAILED") << std::endl;
}

int main() {
    std::cout << "=== COMPREHENSIVE SERVER TEST SUITE ===\n\n";

    Server server;
    bool allPassed = true;

    // ==========================================
    // CLIENT MANAGEMENT TESTS
    // ==========================================
    std::cout << "--- CLIENT MANAGEMENT TESTS ---\n";

    // Test 1: Add clients
    server.addClient(1);
    server.addClient(2);
    server.addClient(3);

    Client* alice = server.getClient(1);
    Client* bob = server.getClient(2);
    Client* charlie = server.getClient(3);

    bool test1 = (alice != NULL && bob != NULL && charlie != NULL);
    printTest("Add multiple clients", test1);
    allPassed = allPassed && test1;

    // Test 2: Set nicknames
    alice->setNickname("Alice");
    bob->setNickname("Bob");
    charlie->setNickname("Charlie");

    bool test2 = (alice->getNickname() == "Alice" &&
                  bob->getNickname() == "Bob" &&
                  charlie->getNickname() == "Charlie");
    printTest("Set client nicknames", test2);
    allPassed = allPassed && test2;

    // Test 3: Find client by nickname
    Client* found = server.findClientByNick("Alice");
    bool test3 = (found == alice);
    printTest("Find client by nickname", test3);
    allPassed = allPassed && test3;

    // Test 4: Find non-existent client
    Client* notFound = server.findClientByNick("NonExistent");
    bool test4 = (notFound == NULL);
    printTest("Find non-existent client returns NULL", test4);
    allPassed = allPassed && test4;

    // Test 5: Check nickname in use
    bool test5 = (server.isNicknameInUse("Bob") == true &&
                  server.isNicknameInUse("NonExistent") == false);
    printTest("Check nickname in use", test5);
    allPassed = allPassed && test5;

    // Test 6: Add duplicate client (should not crash)
    server.addClient(1); // Alice's FD again
    Client* aliceAgain = server.getClient(1);
    bool test6 = (aliceAgain == alice); // Should be same client
    printTest("Add duplicate client FD", test6);
    allPassed = allPassed && test6;

    // Test 7: Get non-existent client
    Client* nonExistent = server.getClient(999);
    bool test7 = (nonExistent == NULL);
    printTest("Get non-existent client returns NULL", test7);
    allPassed = allPassed && test7;

    // ==========================================
    // CHANNEL MANAGEMENT TESTS
    // ==========================================
    std::cout << "\n--- CHANNEL MANAGEMENT TESTS ---\n";

    // Test 8: Create channels
    Channel* channel1 = server.createChannel("#general");
    Channel* channel2 = server.createChannel("#random");

    bool test8 = (channel1 != NULL && channel2 != NULL &&
                  channel1->getName() == "#general" &&
                  channel2->getName() == "#random");
    printTest("Create channels", test8);
    allPassed = allPassed && test8;

    // Test 9: Get existing channel
    Channel* foundChannel = server.getChannel("#general");
    bool test9 = (foundChannel == channel1);
    printTest("Get existing channel", test9);
    allPassed = allPassed && test9;

    // Test 10: Get non-existent channel
    Channel* notFoundChannel = server.getChannel("#nonexistent");
    bool test10 = (notFoundChannel == NULL);
    printTest("Get non-existent channel returns NULL", test10);
    allPassed = allPassed && test10;

    // Test 11: Create duplicate channel (should return existing)
    Channel* duplicateChannel = server.createChannel("#general");
    bool test11 = (duplicateChannel == channel1);
    printTest("Create duplicate channel returns existing", test11);
    allPassed = allPassed && test11;

    // Test 12: Add clients to channels
    channel1->addClient(alice);
    channel1->addClient(bob);
    channel2->addClient(alice);
    channel2->addClient(charlie);

    bool test12 = (channel1->hasClient(alice) && channel1->hasClient(bob) &&
                   channel2->hasClient(alice) && channel2->hasClient(charlie));
    printTest("Add clients to channels", test12);
    allPassed = allPassed && test12;

    // Test 13: Set operators
    channel1->addOperator(alice);
    channel2->addOperator(charlie);

    bool test13 = (channel1->isOperator(alice) && channel2->isOperator(charlie) &&
                   !channel1->isOperator(bob) && !channel2->isOperator(alice));
    printTest("Set channel operators", test13);
    allPassed = allPassed && test13;

    // ==========================================
    // MESSAGING TESTS
    // ==========================================
    std::cout << "\n--- MESSAGING TESTS ---\n";

    // Test 14: Queue message to client
    server.queueMessage(2, "Hello Bob!");
    bool test14 = (bob->getOutputBuffer().find("Hello Bob!") != std::string::npos);
    printTest("Queue message to client", test14);
    allPassed = allPassed && test14;

    // Test 15: Queue message to non-existent client (should not crash)
    server.queueMessage(999, "Hello nobody!");
    bool test15 = true; // If we get here, it didn't crash
    printTest("Queue message to non-existent client", test15);
    allPassed = allPassed && test15;

    // Test 16: Channel broadcast
    std::string bobBufferBefore = bob->getOutputBuffer();
    channel1->broadcast("Hello everyone!", alice);
    std::string bobBufferAfter = bob->getOutputBuffer();
    bool test16 = (bobBufferAfter.length() > bobBufferBefore.length());
    printTest("Channel broadcast", test16);
    allPassed = allPassed && test16;

    // ==========================================
    // CLIENT REMOVAL TESTS
    // ==========================================
    std::cout << "\n--- CLIENT REMOVAL TESTS ---\n";

    // Test 17: Remove client from channels (Alice is in both channels)
    bool aliceInChannel1Before = channel1->hasClient(alice);
    bool aliceInChannel2Before = channel2->hasClient(alice);
    
    server.removeClient(1); // Remove Alice

    // Check Alice is removed from channels
    bool aliceInChannel1After = channel1->hasClient(alice);
    bool aliceInChannel2After = channel2->hasClient(alice);

    bool test17 = (aliceInChannel1Before && aliceInChannel2Before &&
                   !aliceInChannel1After && !aliceInChannel2After);
    printTest("Remove client from all channels", test17);
    allPassed = allPassed && test17;

    // Test 18: Check client is removed from server
    Client* aliceAfterRemoval = server.getClient(1);
    bool test18 = (aliceAfterRemoval == NULL);
    printTest("Remove client from server", test18);
    allPassed = allPassed && test18;

    // Test 19: Check nickname is no longer in use
    bool test19 = (!server.isNicknameInUse("Alice"));
    printTest("Nickname freed after client removal", test19);
    allPassed = allPassed && test19;

    // Test 20: Remove client that's not in server (should not crash)
    server.removeClient(999);
    bool test20 = true; // If we get here, it didn't crash
    printTest("Remove non-existent client", test20);
    allPassed = allPassed && test20;

    // ==========================================
    // CHANNEL CLEANUP TESTS
    // ==========================================
    std::cout << "\n--- CHANNEL CLEANUP TESTS ---\n";

    // Test 21: Remove all clients from a channel to test cleanup
    // First, let's see what channels exist
    Channel* generalChannel = server.getChannel("#general");
    Channel* randomChannel = server.getChannel("#random");

    // Remove Bob (last client in #general)
    server.removeClient(2);

    // Check if #general was deleted (should be empty now)
    Channel* generalAfterCleanup = server.getChannel("#general");
    bool test21 = (generalAfterCleanup == NULL);
    printTest("Empty channel cleanup", test21);
    allPassed = allPassed && test21;

    // Test 22: Check that #random still exists (Charlie is still there)
    Channel* randomAfterCleanup = server.getChannel("#random");
    bool test22 = (randomAfterCleanup != NULL && randomAfterCleanup->hasClient(charlie));
    printTest("Non-empty channel preserved", test22);
    allPassed = allPassed && test22;

    // Test 23: Remove last client from remaining channel
    server.removeClient(3); // Remove Charlie

    Channel* randomAfterFinalCleanup = server.getChannel("#random");
    bool test23 = (randomAfterFinalCleanup == NULL);
    printTest("Final channel cleanup", test23);
    allPassed = allPassed && test23;

    // ==========================================
    // STRESS TESTS
    // ==========================================
    std::cout << "\n--- STRESS TESTS ---\n";

    // Test 24: Add many clients and channels
    for (int i = 100; i < 110; ++i) {
        server.addClient(i);
        Client* client = server.getClient(i);
        if (client) {
            std::string nick = "User" + std::string(1, '0' + (i - 100));
            client->setNickname(nick);
        }
    }

    // Create multiple channels and add clients
    for (int i = 1; i <= 5; ++i) {
        std::string channelName = "#test" + std::string(1, '0' + i);
        Channel* testChannel = server.createChannel(channelName);

        // Add some clients to each channel
        for (int j = 100; j < 105; ++j) {
            Client* client = server.getClient(j);
            if (client && testChannel) {
                testChannel->addClient(client);
            }
        }
    }

    bool test24 = true; // If we get here without crashing, it passed
    printTest("Multiple clients and channels", test24);
    allPassed = allPassed && test24;

    // Test 25: Remove all clients (stress test cleanup)
    for (int i = 100; i < 110; ++i) {
        server.removeClient(i);
    }

    // Check all channels are cleaned up
    bool channelsCleanedUp = true;
    for (int i = 1; i <= 5; ++i) {
        std::string channelName = "#test" + std::string(1, '0' + i);
        if (server.getChannel(channelName) != NULL) {
            channelsCleanedUp = false;
            break;
        }
    }

    bool test25 = channelsCleanedUp;
    printTest("Mass client removal and cleanup", test25);
    allPassed = allPassed && test25;

    // ==========================================
    // EDGE CASE TESTS
    // ==========================================
    std::cout << "\n--- EDGE CASE TESTS ---\n";

    // Test 26: Operations on empty server
    Client* emptyServerClient = server.getClient(1);
    bool emptyServerNick = server.isNicknameInUse("Anyone");
    Channel* emptyServerChannel = server.getChannel("#empty");

    bool test26 = (emptyServerClient == NULL &&
                   !emptyServerNick &&
                   emptyServerChannel == NULL);
    printTest("Operations on empty server", test26);
    allPassed = allPassed && test26;

    // Test 27: Client with empty nickname
    server.addClient(50);
    Client* emptyNickClient = server.getClient(50);
    emptyNickClient->setNickname("");

    Client* foundEmpty = server.findClientByNick("");
    bool test27 = (foundEmpty == emptyNickClient);
    printTest("Client with empty nickname", test27);
    allPassed = allPassed && test27;

    server.removeClient(50);

    // ==========================================
    // FINAL RESULTS
    // ==========================================
    std::cout << "\n=== TEST RESULTS ===\n";
    std::cout << "Overall Result: " << (allPassed ? "✅ ALL TESTS PASSED" : "❌ SOME TESTS FAILED") << std::endl;
    std::cout << "Test suite completed successfully!\n";

    return allPassed ? 0 : 1;
}
