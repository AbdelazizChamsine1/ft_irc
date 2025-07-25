#include "Server.hpp"
#include <iostream>

// Constructor/Destructor
Server::Server() {}

Server::Server(const std::string& password) : _password(password) {}

Server::~Server() {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        delete it->second;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;
}

// -------- CLIENT METHODS --------

void Server::addClient(int fd) {
    if (_clients.find(fd) == _clients.end())
        _clients[fd] = new Client(fd);
}

void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it != _clients.end()) {
        removeClientFromAllChannels(it->second);
        delete it->second;
        _clients.erase(it);
    }
}

Client* Server::getClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    return (it != _clients.end()) ? it->second : NULL;
}

Client* Server::findClientByNick(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == nickname)
            return it->second;
    }
    return NULL;
}

bool Server::isNicknameInUse(const std::string& nickname) {
    return findClientByNick(nickname) != NULL;
}

// -------- CHANNEL METHODS --------

Channel* Server::getChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    return (it != _channels.end()) ? it->second : NULL;
}

Channel* Server::createChannel(const std::string& name) {
    if (_channels.find(name) == _channels.end())
        _channels[name] = new Channel(name);
    return _channels[name];
}

void Server::removeClientFromAllChannels(Client* client) {
    std::vector<Channel*> channelsToCheck;
    
    // First, collect all channels that have this client
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        if (it->second->hasClient(client)) {
            channelsToCheck.push_back(it->second);
        }
    }
    
    // Then remove the client from each channel and check if empty
    for (std::vector<Channel*>::iterator it = channelsToCheck.begin(); it != channelsToCheck.end(); ++it) {
        (*it)->removeClient(client);
        deleteChannelIfEmpty(*it);
    }
}

void Server::deleteChannelIfEmpty(Channel* channel) {
    if (!channel)
        return;

    // Check if it's in the map
    std::map<std::string, Channel*>::iterator it = _channels.find(channel->getName());
    if (it == _channels.end())
        return;

    // If no members, delete and erase
    if (channel->getMembers().empty()) {
        delete channel;
        _channels.erase(it);
    }
}


// -------- MESSAGING --------

void Server::queueMessage(int clientFd, const std::string& message) {
    Client* client = getClient(clientFd);
    if (client)
        client->getOutputBuffer() += message + "\r\n";
}

// -------- PASSWORD MANAGEMENT --------

const std::string& Server::getPassword() const {
    return _password;
}

// -------- CHANNEL UTILITIES --------

std::vector<Channel*> Server::getClientChannels(Client* client) {
    std::vector<Channel*> clientChannels;
    
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        if (it->second->hasClient(client)) {
            clientChannels.push_back(it->second);
        }
    }
    
    return clientChannels;
}
