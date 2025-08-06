#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>
#include <vector>
#include <map>
#include "Client.hpp"
#include "Server.hpp"
#include "CommandHandlers.hpp"
#include "IRCProtocol.hpp"

class Server; // Forward declaration
class Client; // Forward declaration
class CommandHandlers; // Forward declaration

class Command {
private:
    Server* _server;
    CommandHandlers* _handlers;
    std::map<std::string, void (CommandHandlers::*)(Client*, const std::vector<std::string>&)> _commandMap;

    void initializeCommandMap();
    IRCCommand parseRawCommand(const std::string& rawCommand);
    std::vector<std::string> splitParams(const std::string& params);

public:
    Command(Server* server);
    ~Command();

    // Main command processing
    void processClientBuffer(Client* client);
    void executeCommand(Client* client, const IRCCommand& cmd);

    // Buffer processing utilities
    std::vector<std::string> extractCompleteCommands(std::string& buffer);
    bool isCommandComplete(const std::string& buffer);
};

#endif