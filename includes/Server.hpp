#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <string>
#include <vector>
#include "Client.hpp"
#include "Channel.hpp"

class Server {
private:
    std::map<int, Client*> _clients;                     // socket fd → Client
    std::map<std::string, Channel*> _channels;           // channel name → Channel
    std::string _password;                               // server password

public:
    Server();
    Server(const std::string& password);
    ~Server();

    // Client management
    void addClient(int fd);
    void removeClient(int fd);
    Client* getClient(int fd);
    Client* findClientByNick(const std::string& nickname);
    bool isNicknameInUse(const std::string& nickname);

    // Channel management
    Channel* getChannel(const std::string& name);
    Channel* createChannel(const std::string& name);
    void removeClientFromAllChannels(Client* client);
    void deleteChannelIfEmpty(Channel* channel);

    // Messaging
    void queueMessage(int clientFd, const std::string& message);
    
    // Password management
    const std::string& getPassword() const;
    
    // Channel utilities
    std::vector<Channel*> getClientChannels(Client* client);
};

#endif
