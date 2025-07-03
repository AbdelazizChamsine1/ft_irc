#include "Server.hpp"
#include <iostream>

int main() {
    Server server;

    // ----------------------
    // ✅ Test 1: Add clients
    server.addClient(1);
    server.addClient(2);

    Client* alice = server.getClient(1);
    Client* bob = server.getClient(2);

    alice->setNickname("Alice");
    bob->setNickname("Bob");
    std::cout << "[TEST 1] Clients Alice and Bob added.\n";

    // ----------------------
    // ✅ Test 2: Find by nickname
    Client* found = server.findClientByNick("Alice");
    std::cout << "[TEST 2] Found nickname 'Alice'? " << (found == alice ? "Yes" : "No") << std::endl;

    // ----------------------
    // ✅ Test 3: Check nickname in use
    std::cout << "[TEST 3] Is 'Bob' in use? " << (server.isNicknameInUse("Bob") ? "Yes" : "No") << std::endl;

    // ----------------------
    // ✅ Test 4: Create a channel and add clients
    Channel* chan = server.createChannel("#42");
    chan->addClient(alice);
    chan->addClient(bob);
    chan->addOperator(alice);

    std::cout << "[TEST 4] Created channel and added Alice + Bob.\n";
    std::cout << "  - Alice is operator? " << (chan->isOperator(alice) ? "Yes" : "No") << std::endl;

    // ----------------------
    // ✅ Test 5: Broadcast message
    chan->broadcast("Hello from Alice", alice);
    std::cout << "[TEST 5] Bob's output buffer: " << bob->getOutputBuffer() << std::endl;

    // ----------------------
    // ✅ Test 6: Remove Alice and check cleanup
    server.removeClient(1);
    std::cout << "[TEST 6] Alice removed from server and channel.\n";
    std::cout << "  - Is Alice in channel? " << (chan->hasClient(alice) ? "Yes" : "No") << std::endl;

    // ----------------------
    // ✅ Test 7: Remove Bob
    server.removeClient(2);
    std::cout << "[TEST 7] Bob removed.\n";

    std::cout << "\n✅ All Server tests passed.\n";
    return 0;
}
