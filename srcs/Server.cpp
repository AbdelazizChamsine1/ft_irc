#include "Server.hpp"
#include <iostream>
#include <ctime>
#include <unistd.h>

// Constructor/Destructor
Server::Server() {}

Server::Server(const std::string& password) : _password(password) {}

Server::~Server() {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        delete it->second;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;
}

// Configuration
void Server::setPassword(const std::string& password) {
    _password = password;
}

const std::string& Server::getPassword() const {
    return _password;
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
        client->enqueueMessage(message);
}

void Server::sendMessage(int clientFd, const std::string& message) {
    queueMessage(clientFd, message);
}

void Server::broadcast(const std::set<int>& targets, const std::string& message) {
    for (std::set<int>::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        queueMessage(*it, message);
    }
}

// -------- I/O INTERFACE METHODS --------

void Server::flushClientMessages(int clientFd) {
    Client* client = getClient(clientFd);
    if (client) {
        client->flushMessagesToOutputBuffer();
    }
}

bool Server::hasClientMessagesToSend(int clientFd) const {
    Client* client = const_cast<Server*>(this)->getClient(clientFd);
    return client ? client->hasMessagesToSend() : false;
}

// -------- TIMEOUT HANDLING --------

// void Server::disconnectIdleClients(int timeoutSeconds) {
//     std::vector<int> clientsToDisconnect;
//     time_t currentTime = time(NULL);

//     for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
//         if (currentTime - it->second->getLastActive() > timeoutSeconds) {
//             clientsToDisconnect.push_back(it->first);
//         }
//     }

//     for (std::vector<int>::iterator it = clientsToDisconnect.begin(); it != clientsToDisconnect.end(); ++it) {
//         int fd = *it;
//         Client* client = getClient(fd);
//         if (client && client->isRegistered()) {
//             // Broadcast QUIT to channels to inform peers
//             std::vector<Channel*> chans = getClientChannels(client);
//             std::string quitMsg = ":" + client->getHostmask() + " QUIT :Ping timeout" + "\r\n";
//             for (std::vector<Channel*>::iterator cit = chans.begin(); cit != chans.end(); ++cit) {
//                 (*cit)->broadcast(quitMsg, client);
//             }
//         }
//         std::cout << "Disconnecting idle client: fd=" << fd << std::endl;
//         close(fd);
//         removeClient(fd);
//     }
// }

void Server::disconnectIdleClients(int timeoutSeconds) {
    std::vector<int> clientsToDisconnect;
    time_t currentTime = time(NULL);

    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (currentTime - it->second->getLastActive() > timeoutSeconds) {
            clientsToDisconnect.push_back(it->first);
        }
    }

    for (std::vector<int>::iterator it = clientsToDisconnect.begin(); it != clientsToDisconnect.end(); ++it) {
        int fd = *it;
        std::cout << "Disconnecting idle client: fd=" << fd << std::endl;
        close(fd);
        handleClientDisconnection(fd);  // Use the new method instead of manual handling
    }
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

void Server::handleClientDisconnection(int fd) {
    Client* client = getClient(fd);
    if (!client) {
        return;
    }

    // Get channels before removing client
    std::vector<Channel*> clientChannels = getClientChannels(client);
    
    // Send QUIT message to channels if client was registered
    if (client->isRegistered()) {
        std::string quitMsg = ":" + client->getHostmask() + " QUIT :Client disconnected\r\n";
        for (std::vector<Channel*>::iterator it = clientChannels.begin();
             it != clientChannels.end(); ++it) {
            (*it)->broadcast(quitMsg, client);
        }
    }

    // Remove client from all channels
    removeClientFromAllChannels(client);

    // Check each channel for operator succession
    for (std::vector<Channel*>::iterator it = clientChannels.begin();
         it != clientChannels.end(); ++it) {
        (*it)->promoteNewOperatorIfNeeded();
    }

    // Remove the client
    removeClient(fd);
}