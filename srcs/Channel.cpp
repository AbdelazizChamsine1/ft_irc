#include "Channel.hpp"
#include <algorithm>
#include <iostream>

Channel::Channel(const std::string& name)
    : _name(name) {}

Channel::~Channel() {}

const std::string& Channel::getName() const {
    return _name;
}

void Channel::setTopic(const std::string& topic) {
    _topic = topic;
}

const std::string& Channel::getTopic() const {
    return _topic;
}

void Channel::addClient(Client* client) {
    _members.insert(client);
}

void Channel::removeClient(Client* client) {
    _members.erase(client);
    _operators.erase(client);
}

bool Channel::hasClient(Client* client) const {
    return _members.find(client) != _members.end();
}

void Channel::addOperator(Client* client) {
    _operators.insert(client);
}

bool Channel::isOperator(Client* client) const {
    return _operators.find(client) != _operators.end();
}

void Channel::broadcast(const std::string& message, Client* sender) {
    for (std::set<Client*>::iterator it = _members.begin(); it != _members.end(); ++it) {
        if (*it != sender) {
            (*it)->getOutputBuffer() += message + "\r\n";
        }
    }
}
