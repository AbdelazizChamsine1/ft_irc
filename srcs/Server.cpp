#include "Server.hpp"
#include <iostream>

Client* Server::getClient(int fd) {
    if (_clients.find(fd) != _clients.end())
        return _clients[fd];
    return NULL;
}

Client* Server::findClientByNick(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == nickname)
            return it->second;
    }
    return NULL;
}

void Server::addClient(int fd) {
    if (_clients.find(fd) == _clients.end()) {
        _clients[fd] = new Client(fd);
    }
}

void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it != _clients.end()) {
        removeClientFromAllChannels(it->second);
        delete it->second;
        _clients.erase(it);
    }
}

Channel* Server::getChannel(const std::string& name) {
    if (_channels.find(name) != _channels.end())
        return _channels[name];
    return NULL;
}

Channel* Server::createChannel(const std::string& name) {
    if (_channels.find(name) == _channels.end())
        _channels[name] = new Channel(name);
    return _channels[name];
}

void Server::removeClientFromAllChannels(Client* client) {
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        if (it->second->hasClient(client)) {
            it->second->removeClient(client);
        }
    }
}

void Server::queueMessage(int clientFd, const std::string& message) {
    Client* client = getClient(clientFd);
    if (client) {
        client->getOutputBuffer() += message + "\r\n";
    }
}
