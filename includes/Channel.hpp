#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>
#include "Client.hpp"

class Channel {
private:
    std::string _name;
    std::string _topic;
    std::set<Client*> _members;
    std::set<Client*> _operators;

public:
    Channel(const std::string& name);
    ~Channel();

    const std::string& getName() const;
    void setTopic(const std::string& topic);
    const std::string& getTopic() const;

    void addClient(Client* client);
    void removeClient(Client* client);
    bool hasClient(Client* client) const;

    void addOperator(Client* client);
    bool isOperator(Client* client) const;

    void broadcast(const std::string& message, Client* sender);
};

#endif
