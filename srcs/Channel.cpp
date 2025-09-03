#include "Channel.hpp"
#include "Client.hpp"

Channel::Channel(const std::string& name)
    : _name(name), _topic(""), _inviteOnly(false), _topicRestricted(true), _key(""), _userLimit(0) {}

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
    _invitedClients.erase(client); // Remove from invite list when leaving
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
            // Message is expected to already be a complete IRC line (ends with CRLF)
            (*it)->getOutputBuffer() += message;
        }
    }
}

// Mode methods
bool Channel::isInviteOnly() const {
    return _inviteOnly;
}

bool Channel::isTopicRestricted() const {
    return _topicRestricted;
}

const std::string& Channel::getKey() const {
    return _key;
}

size_t Channel::getUserLimit() const {
    return _userLimit;
}

void Channel::setInviteOnly(bool inviteOnly) {
    _inviteOnly = inviteOnly;
}

void Channel::setTopicRestricted(bool topicRestricted) {
    _topicRestricted = topicRestricted;
}

void Channel::setKey(const std::string& key) {
    _key = key;
}

void Channel::setUserLimit(size_t limit) {
    _userLimit = limit;
}

std::string Channel::getModeString() const {
    std::string modes = "+";

    if (_inviteOnly) modes += "i";
    if (_topicRestricted) modes += "t";
    if (!_key.empty()) modes += "k";
    if (_userLimit > 0) modes += "l";

    if (modes == "+") modes = "";

    return modes;
}

// Invite management
void Channel::addInvite(Client* client) {
    _invitedClients.insert(client);
}

void Channel::removeInvite(Client* client) {
    _invitedClients.erase(client);
}

bool Channel::isInvited(Client* client) const {
    return _invitedClients.find(client) != _invitedClients.end();
}


void Channel::promoteNewOperatorIfNeeded() {
    if (_operators.empty() && !_members.empty()) {
        Client* newOperator = *_members.begin(); // Get first member
        addOperator(newOperator);
        
        std::string modeMsg = ":ircserv MODE " + _name + " +o " + newOperator->getNickname() + "\r\n";
        broadcast(modeMsg, NULL);
    }
}