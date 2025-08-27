#include "Command.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include "CommandHandlers.hpp"

Command::Command(Server* server) : _server(server) {
    _handlers = new CommandHandlers(server);
    initializeCommandMap();
}

Command::~Command() {
    delete _handlers;
}

void Command::initializeCommandMap() {
    _commandMap["PASS"] = &CommandHandlers::handlePass;
    _commandMap["NICK"] = &CommandHandlers::handleNick;
    _commandMap["USER"] = &CommandHandlers::handleUser;
    _commandMap["PING"] = &CommandHandlers::handlePing;
    _commandMap["PONG"] = &CommandHandlers::handlePong;
    _commandMap["JOIN"] = &CommandHandlers::handleJoin;
    _commandMap["PART"] = &CommandHandlers::handlePart;
    _commandMap["PRIVMSG"] = &CommandHandlers::handlePrivmsg;
    _commandMap["NOTICE"] = &CommandHandlers::handleNotice;
    _commandMap["QUIT"] = &CommandHandlers::handleQuit;
    _commandMap["KICK"] = &CommandHandlers::handleKick;
    _commandMap["INVITE"] = &CommandHandlers::handleInvite;
    _commandMap["TOPIC"] = &CommandHandlers::handleTopic;
    _commandMap["MODE"] = &CommandHandlers::handleMode;
    _commandMap["CAP"] = &CommandHandlers::handleCap;
    _commandMap["WHO"] = &CommandHandlers::handleWho;
    _commandMap["WHOIS"] = &CommandHandlers::handleWhois;
    _commandMap["LIST"] = &CommandHandlers::handleList;
    _commandMap["NAMES"] = &CommandHandlers::handleNames;
}

void Command::processClientBuffer(Client* client) {
    // Extract complete commands from client buffer using the client's own method
    while (client->hasCompleteLine()) {
        std::string line = client->extractNextLine();
        
        if (line.empty()) {
            continue; // Skip empty lines
        }
        
        // Parse and execute the command
        IRCCommand cmd = parseRawCommand(line);
        executeCommand(client, cmd);
    }
}

std::vector<std::string> Command::extractCompleteCommands(std::string& buffer) {
    std::vector<std::string> commands;
    size_t pos = 0;
    
    while ((pos = buffer.find("\r\n")) != std::string::npos) {
        std::string command = buffer.substr(0, pos);
        commands.push_back(command);
        buffer.erase(0, pos + 2); // Remove command + \r\n
    }
    
    // Also handle lines ending with just \n for better compatibility
    pos = 0;
    while ((pos = buffer.find("\n")) != std::string::npos) {
        std::string command = buffer.substr(0, pos);
        // Remove trailing \r if present (C++98 compatible)
        if (!command.empty() && command[command.length() - 1] == '\r') {
            command.erase(command.length() - 1);
        }
        commands.push_back(command);
        buffer.erase(0, pos + 1); // Remove command + \n
    }
    
    return commands;
}

bool Command::isCommandComplete(const std::string& buffer) {
    return buffer.find("\r\n") != std::string::npos || buffer.find("\n") != std::string::npos;
}

IRCCommand Command::parseRawCommand(const std::string& rawCommand) {
    IRCCommand cmd;
    std::string line = rawCommand;
    
    // Remove trailing whitespace (C++98 compatible)
    while (!line.empty() && (line[line.length() - 1] == ' ' || 
                             line[line.length() - 1] == '\t' || 
                             line[line.length() - 1] == '\r' || 
                             line[line.length() - 1] == '\n')) {
        line.erase(line.length() - 1);
    }
    
    // Handle empty commands
    if (line.empty()) {
        return cmd; // Return empty command
    }
    
    size_t pos = 0;
    
    // Parse prefix (if exists)
    if (!line.empty() && line[0] == ':') {
        pos = line.find(' ');
        if (pos != std::string::npos) {
            cmd.prefix = line.substr(1, pos - 1);
            line = line.substr(pos + 1);
            
            // Skip any extra spaces
            while (!line.empty() && line[0] == ' ') {
                line = line.substr(1);
            }
        }
    }
    
    // Parse command
    pos = line.find(' ');
    if (pos != std::string::npos) {
        cmd.command = line.substr(0, pos);
        line = line.substr(pos + 1);
        
        // Skip any extra spaces
        while (!line.empty() && line[0] == ' ') {
            line = line.substr(1);
        }
        
        // Parse parameters
        pos = line.find(" :");
        if (pos != std::string::npos) {
            // Has trailing parameter
            std::string params = line.substr(0, pos);
            cmd.trailing = line.substr(pos + 2);
            cmd.params = splitParams(params);
        } else if (!line.empty() && line[0] == ':') {
            // Entire remaining line is trailing parameter
            cmd.trailing = line.substr(1);
        } else {
            // No trailing parameter
            cmd.params = splitParams(line);
        }
    } else {
        // Command with no parameters
        cmd.command = line;
    }
    
    // Convert command to uppercase for consistency
    std::transform(cmd.command.begin(), cmd.command.end(), cmd.command.begin(), ::toupper);
    
    return cmd;
}

std::vector<std::string> Command::splitParams(const std::string& params) {
    std::vector<std::string> result;
    
    if (params.empty()) {
        return result;
    }
    
    std::istringstream iss(params);
    std::string param;
    
    while (iss >> param) {
        result.push_back(param);
    }
    
    return result;
}

void Command::executeCommand(Client* client, const IRCCommand& cmd) {
    // Ignore empty commands
    if (cmd.command.empty()) {
        return;
    }
    
    // Find command handler
    std::map<std::string, void (CommandHandlers::*)(Client*, const std::vector<std::string>&)>::iterator it;
    it = _commandMap.find(cmd.command);
    
    if (it != _commandMap.end()) {
        // Prepare parameters (include trailing if exists)
        std::vector<std::string> allParams = cmd.params;
        if (!cmd.trailing.empty()) {
            allParams.push_back(cmd.trailing);
        }
        
        // Call the appropriate handler
        ((_handlers)->*(it->second))(client, allParams);
    } else {
        // Unknown command
        _handlers->sendErrorReply(client, IRC::ERR_UNKNOWNCOMMAND, cmd.command + " :Unknown command");
    }
}