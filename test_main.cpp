#include "Client.hpp"
#include "Channel.hpp"
#include "Server.hpp"
#include <iostream>

int main() {
    Server server;

    // Create Alice
    int fdAlice = 1;
    server.addClient(fdAlice);
    Client* alice = server.getClient(fdAlice);
    alice->setNickname("Alice");
    alice->setUsername("alice42");
    alice->setRegistered(true);

    // Create Bob
    int fdBob = 2;
    server.addClient(fdBob);
    Client* bob = server.getClient(fdBob);
    bob->setNickname("Bob");
    bob->setUsername("bob42");
    bob->setRegistered(true);

    // Create a channel and add both clients
    Channel* channel = server.createChannel("#42");
    channel->addClient(alice);
    channel->addClient(bob);

    // Alice sends a message to the channel
    channel->broadcast("Hello #42!", alice);

    // Output checks
    std::cout << "Client nickname: " << alice->getNickname() << std::endl;
    std::cout << "Client username: " << alice->getUsername() << std::endl;
    std::cout << "Alice's output buffer: " << alice->getOutputBuffer() << std::endl;
    std::cout << "Bob's output buffer: " << bob->getOutputBuffer() << std::endl;

    server.removeClient(fdAlice);
    server.removeClient(fdBob);

    return 0;
}

