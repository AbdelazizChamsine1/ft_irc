#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <string>
#include "Client.hpp"
#include "Channel.hpp"

class Server {
private:
    std::map<int, Client*> _clients;
    std::map<std::string, Channel*> _channels;

public:
    Client* getClient(int fd);
    Client* findClientByNick(const std::string& nickname);
    void addClient(int fd);
    void removeClient(int fd);

    Channel* getChannel(const std::string& name);
    Channel* createChannel(const std::string& name);
    void removeClientFromAllChannels(Client* client);

    void queueMessage(int clientFd, const std::string& message);
};

#endif
