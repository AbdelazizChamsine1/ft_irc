#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <string>
#include <vector>
#include <set>
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

    // Configuration
    void setPassword(const std::string& password);
    const std::string& getPassword() const;

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

    // Messaging - Enhanced for I/O layer
    void queueMessage(int clientFd, const std::string& message);
        
    // Channel utilities
    std::vector<Channel*> getClientChannels(Client* client);
    void sendMessage(int clientFd, const std::string& message);
    void broadcast(const std::set<int>& targets, const std::string& message);

    // I/O Interface methods
    void flushClientMessages(int clientFd);
    bool hasClientMessagesToSend(int clientFd) const;

    // Timeout handling
    void disconnectIdleClients(int timeoutSeconds);
};

#endif
