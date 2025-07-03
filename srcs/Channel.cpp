#include "Channel.hpp"

Channel::Channel(const std::string& name)
    : _name(name), _topic("") {}

Channel::~Channel() {}

// Basic info
const std::string& Channel::getName() const {
    return _name;
}

const std::string& Channel::getTopic() const {
    return _topic;
}

void Channel::setTopic(const std::string& topic) {
    _topic = topic;
}

// Membership
void Channel::addClient(Client* client) {
    _members.insert(client);
}

void Channel::removeClient(Client* client) {
    _members.erase(client);
    _operators.erase(client); // Remove operator role if leaving
}

bool Channel::hasClient(Client* client) const {
    return _members.find(client) != _members.end();
}

const std::set<Client*>& Channel::getMembers() const {
    return _members;
}


// Operators
void Channel::addOperator(Client* client) {
    _operators.insert(client);
}

void Channel::removeOperator(Client* client) {
    _operators.erase(client);
}

bool Channel::isOperator(Client* client) const {
    return _operators.find(client) != _operators.end();
}

// Messaging
void Channel::broadcast(const std::string& message, Client* sender) {
    for (std::set<Client*>::iterator it = _members.begin(); it != _members.end(); ++it) {
        if (*it != sender) {
            (*it)->getOutputBuffer() += message + "\r\n";
        }
    }
}
