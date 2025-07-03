#include "Client.hpp"
#include "Channel.hpp"
#include <iostream>

int main() {
    // Simulate two clients with fake file descriptors
    Client* alice = new Client(1);
    alice->setNickname("Alice");
    alice->setUsername("alice42");
    alice->setHostname("localhost");

    Client* bob = new Client(2);
    bob->setNickname("Bob");
    bob->setUsername("bob42");
    bob->setHostname("localhost");

    // ----------------------
    // ✅ Test 1: Channel creation
    Channel* channel = new Channel("#42");
    std::cout << "[TEST 1] Created channel with name: " << channel->getName() << std::endl;

    // ----------------------
    // ✅ Test 2: Add clients to channel
    channel->addClient(alice);
    channel->addClient(bob);
    std::cout << "[TEST 2] Alice and Bob added to channel.\n";

    // Check membership
    std::cout << "  - Is Alice in channel? " << (channel->hasClient(alice) ? "Yes" : "No") << std::endl;
    std::cout << "  - Is Bob in channel?   " << (channel->hasClient(bob) ? "Yes" : "No") << std::endl;

    // ----------------------
    // ✅ Test 3: Promote Alice to operator
    channel->addOperator(alice);
    std::cout << "[TEST 3] Alice promoted to operator." << std::endl;
    std::cout << "  - Is Alice operator? " << (channel->isOperator(alice) ? "Yes" : "No") << std::endl;
    std::cout << "  - Is Bob operator?   " << (channel->isOperator(bob) ? "Yes" : "No") << std::endl;

    // ----------------------
    // ✅ Test 4: Set and get topic
    channel->setTopic("Welcome to #42!");
    std::cout << "[TEST 4] Channel topic: " << channel->getTopic() << std::endl;

    // ----------------------
    // ✅ Test 5: Broadcast message from Alice
    std::cout << "[TEST 5] Alice sends message to channel..." << std::endl;
    channel->broadcast("Hello Bob!", alice);
    std::cout << "  - Bob's output buffer: " << bob->getOutputBuffer() << std::endl;
    std::cout << "  - Alice's output buffer (should be empty): " << alice->getOutputBuffer() << std::endl;

    // ----------------------
    // ✅ Test 6: Remove Alice from channel
    channel->removeClient(alice);
    std::cout << "[TEST 6] Alice removed from channel." << std::endl;
    std::cout << "  - Is Alice in channel? " << (channel->hasClient(alice) ? "Yes" : "No") << std::endl;
    std::cout << "  - Is Alice still operator? " << (channel->isOperator(alice) ? "Yes" : "No") << std::endl;

    // ----------------------
    // ✅ Clean up
    delete alice;
    delete bob;
    delete channel;

    std::cout << "\nAll tests completed successfully." << std::endl;
    return 0;
}
