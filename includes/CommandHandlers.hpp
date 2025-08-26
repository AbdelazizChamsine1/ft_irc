#ifndef COMMANDHANDLERS_HPP
#define COMMANDHANDLERS_HPP

#include <string>
#include <vector>
#include "Client.hpp"
#include "Server.hpp"
#include "IRCProtocol.hpp"
#include <iostream>
#include <cstdlib>

class Server; // Forward declaration
class Client; // Forward declaration

class CommandHandlers {
private:
    Server* _server;

public:
    CommandHandlers(Server* server);
    ~CommandHandlers();

    // Authentication commands
    void handlePass(Client* client, const std::vector<std::string>& params);
    void handleNick(Client* client, const std::vector<std::string>& params);
    void handleUser(Client* client, const std::vector<std::string>& params);

    // Communication commands
    void handleJoin(Client* client, const std::vector<std::string>& params);
    void handlePart(Client* client, const std::vector<std::string>& params);
    void handlePrivmsg(Client* client, const std::vector<std::string>& params);
    void handleNotice(Client* client, const std::vector<std::string>& params);
    void handleQuit(Client* client, const std::vector<std::string>& params);

    // Keepalive
    void handlePing(Client* client, const std::vector<std::string>& params);
    void handlePong(Client* client, const std::vector<std::string>& params);

    // Channel operator commands
    void handleKick(Client* client, const std::vector<std::string>& params);
    void handleInvite(Client* client, const std::vector<std::string>& params);
    void handleTopic(Client* client, const std::vector<std::string>& params);
    void handleMode(Client* client, const std::vector<std::string>& params);

    // Information commands
    void handleCap(Client* client, const std::vector<std::string>& params);
    void handleWho(Client* client, const std::vector<std::string>& params);
    void handleWhois(Client* client, const std::vector<std::string>& params);
    void handleList(Client* client, const std::vector<std::string>& params);
    void handleNames(Client* client, const std::vector<std::string>& params);

    // Utility functions
    void sendWelcomeSequence(Client* client);
    void sendErrorReply(Client* client, const std::string& code, const std::string& message);
    bool validateNickname(const std::string& nickname);
    bool validateChannelName(const std::string& channel);
};

#endif