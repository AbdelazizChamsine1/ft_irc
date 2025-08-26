#include "CommandHandlers.hpp"
#include "Server.hpp"
#include "Channel.hpp"


CommandHandlers::CommandHandlers(Server* server) : _server(server) {}

CommandHandlers::~CommandHandlers() {}

// Authentication commands
void CommandHandlers::handlePass(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        sendErrorReply(client, IRC::ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
        return;
    }

    if (client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_ALREADYREGISTRED, "You may not reregister");
        return;
    }

    const std::string& password = params[0];
    if (password != _server->getPassword()) {
        sendErrorReply(client, IRC::ERR_PASSWDMISMATCH, "Password incorrect");
        return;
    }

    client->setReceivedPass(true);
    client->tryRegister();
    if (client->isRegistered() && !client->welcomeSent()) {
        sendWelcomeSequence(client);
        client->setWelcomeSent(true);
    }
    std::cout << "PASS accepted from client " << client->getFd() << std::endl;
}

void CommandHandlers::handleNick(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        sendErrorReply(client, IRC::ERR_NONICKNAMEGIVEN, "No nickname given");
        return;
    }

    const std::string& nickname = params[0];

    if (!validateNickname(nickname)) {
        sendErrorReply(client, IRC::ERR_ERRONEUSNICKNAME, nickname + " :Erroneous nickname");
        return;
    }

    if (_server->isNicknameInUse(nickname)) {
        sendErrorReply(client, IRC::ERR_NICKNAMEINUSE, nickname + " :Nickname is already in use");
        return;
    }

    client->setNickname(nickname);
    client->setReceivedNick(true);
    client->tryRegister();
    if (client->isRegistered() && !client->welcomeSent()) {
        sendWelcomeSequence(client);
        client->setWelcomeSent(true);
    }
    std::cout << "Client " << client->getFd() << " set nickname to " << nickname << std::endl;
}

void CommandHandlers::handleUser(Client* client, const std::vector<std::string>& params) {
    if (client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_ALREADYREGISTRED, "You may not reregister");
        return;
    }

    if (params.size() < 4) {
        sendErrorReply(client, IRC::ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
        return;
    }

    client->setUsername(params[0]);
    std::cout << "Client " << client->getFd() << " registered with username: " << params[0] << std::endl;
    client->setRealname(params[3]);
    client->setReceivedUser(true);
    client->tryRegister();
    if (client->isRegistered() && !client->welcomeSent()) {
        sendWelcomeSequence(client);
        client->setWelcomeSent(true);
    }
}

// Keepalive commands
void CommandHandlers::handlePing(Client* client, const std::vector<std::string>& params) {
    // RFC: PING [<server1> [<server2>]] or PING :<token>
    std::string token = params.empty() ? "" : params[0];
    if (token.empty()) {
        // Not enough params
        sendErrorReply(client, IRC::ERR_NEEDMOREPARAMS, "PING :Not enough parameters");
        return;
    }
    // Reply with PONG from our server name with same token
    std::string pong = formatIRCMessage("ircserv", "PONG", "ircserv", token);
    _server->queueMessage(client->getFd(), pong);
}

void CommandHandlers::handlePong(Client* client, const std::vector<std::string>& params) {
    // Just update activity; params may contain token
    (void)params;
    client->updateLastActive();
}

// Communication commands
void CommandHandlers::handleJoin(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }

    if (params.empty()) {
        sendErrorReply(client, IRC::ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
        return;
    }

    const std::string& channelName = params[0];
    std::string channelKey = (params.size() > 1) ? params[1] : "";

    if (!validateChannelName(channelName)) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    Channel* channel = _server->getChannel(channelName);
    if (!channel) {
        channel = _server->createChannel(channelName);
        // Make the first client an operator
        channel->addOperator(client);
    } else {
        // Check channel restrictions
        if (channel->isInviteOnly() && !channel->isInvited(client)) {
            sendErrorReply(client, IRC::ERR_INVITEONLYCHAN, channelName + " :Cannot join channel (+i)");
            return;
        }

        if (channel->getUserLimit() > 0 && channel->getMembers().size() >= channel->getUserLimit()) {
            sendErrorReply(client, IRC::ERR_CHANNELISFULL, channelName + " :Cannot join channel (+l)");
            return;
        }

        if (!channel->getKey().empty()) {
            if (channelKey != channel->getKey()) {
                sendErrorReply(client, IRC::ERR_BADCHANNELKEY, channelName + " :Cannot join channel (+k)");
                return;
            }
        }
    }

    channel->addClient(client);

    // Remove from invite list once joined (invite consumed)
    if (channel->isInvited(client)) {
        channel->removeInvite(client);
    }

    // Send JOIN confirmation and channel info
    std::string joinMsg = formatIRCMessage(client->getHostmask(), "JOIN", channelName, "");
    channel->broadcast(joinMsg, NULL); // Broadcast to all including sender

    // Send topic information to the joining client
    const std::string& topic = channel->getTopic();
    if (topic.empty()) {
        std::string noTopicReply = formatNumericReply(IRC::RPL_NOTOPIC, client->getNickname(), channelName + " :No topic is set");
        _server->queueMessage(client->getFd(), noTopicReply);
    } else {
        std::string topicReply = formatNumericReply(IRC::RPL_TOPIC, client->getNickname(), channelName + " :" + topic);
        _server->queueMessage(client->getFd(), topicReply);
    }

    // Send NAMES list to the joining client
    std::string nameList = "= " + channelName + " :";
    const std::set<Client*>& members = channel->getMembers();
    for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
        if (channel->isOperator(*it)) {
            nameList += "@" + (*it)->getNickname() + " ";
        } else {
            nameList += (*it)->getNickname() + " ";
        }
    }
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_NAMREPLY, client->getNickname(), nameList));
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_ENDOFNAMES, client->getNickname(), channelName + " :End of /NAMES list"));
}

void CommandHandlers::handlePrivmsg(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }

    if (params.empty()) {
        sendErrorReply(client, IRC::ERR_NORECIPIENT, ":No recipient given (PRIVMSG)");
        return;
    }

    const std::string& target = params[0];
    std::string message = (params.size() > 1) ? params[1] : "";
    if (message.empty()) {
        sendErrorReply(client, IRC::ERR_NOTEXTTOSEND, ":No text to send");
        return;
    }

    if (target[0] == '#') {
        // Channel message
        Channel* channel = _server->getChannel(target);
        if (!channel) {
            sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, target + " :No such channel");
            return;
        }

        if (!channel->hasClient(client)) {
            sendErrorReply(client, IRC::ERR_NOTONCHANNEL, target + " :You're not on that channel");
            return;
        }

        std::string privmsg = formatIRCMessage(client->getHostmask(), "PRIVMSG", target, message);
        channel->broadcast(privmsg, client); // Don't send back to sender
    } else {
        // Private message to user
        Client* targetClient = _server->findClientByNick(target);
        if (!targetClient) {
            sendErrorReply(client, IRC::ERR_NOSUCHNICK, target + " :No such nick/channel");
            return;
        }

        std::string privmsg = formatIRCMessage(client->getHostmask(), "PRIVMSG", target, message);
        _server->queueMessage(targetClient->getFd(), privmsg);
    }
}

void CommandHandlers::handleNotice(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        // NOTICE doesn't send error replies - this is intentional per IRC spec
        return;
    }

    if (params.size() < 2) {
        // NOTICE doesn't send error replies - this is intentional per IRC spec
        return;
    }

    const std::string& target = params[0];
    const std::string& message = params[1];

    if (target[0] == '#') {
        // Channel notice
        Channel* channel = _server->getChannel(target);
        if (!channel) {
            // NOTICE doesn't send error replies - silently ignore
            return;
        }

        if (!channel->hasClient(client)) {
            // NOTICE doesn't send error replies - silently ignore
            return;
        }

        std::string noticeMsg = formatIRCMessage(client->getHostmask(), "NOTICE", target, message);
        channel->broadcast(noticeMsg, client); // Don't send back to sender
    } else {
        // Private notice to user
        Client* targetClient = _server->findClientByNick(target);
        if (!targetClient) {
            // NOTICE doesn't send error replies - silently ignore
            return;
        }

        std::string noticeMsg = formatIRCMessage(client->getHostmask(), "NOTICE", target, message);
        _server->queueMessage(targetClient->getFd(), noticeMsg);
    }
}

// Placeholder implementations for other commands
void CommandHandlers::handlePart(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }

    if (params.empty()) {
        sendErrorReply(client, IRC::ERR_NEEDMOREPARAMS, "PART :Not enough parameters");
        return;
    }

    const std::string& channelName = params[0];
    std::string partMessage = (params.size() > 1) ? params[1] : "";

    if (!validateChannelName(channelName)) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    Channel* channel = _server->getChannel(channelName);
    if (!channel) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    if (!channel->hasClient(client)) {
        sendErrorReply(client, IRC::ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }

    // Send PART message to all channel members (including sender)
    std::string partMsg = formatIRCMessage(client->getHostmask(), "PART", channelName, partMessage);
    channel->broadcast(partMsg, NULL); // Send to all including sender

    // Remove client from channel
    channel->removeClient(client);

    // Delete channel if empty
    _server->deleteChannelIfEmpty(channel);
}

void CommandHandlers::handleQuit(Client* client, const std::vector<std::string>& params) {
    std::string quitMessage = (params.empty()) ? "Client Quit" : params[0];

    // Send QUIT message to all channels the client is in before removing them
    std::string quitMsg = formatIRCMessage(client->getHostmask(), "QUIT", "", quitMessage);

    // Get all channels this client is in
    std::vector<Channel*> channelsWithClient = _server->getClientChannels(client);

    // Broadcast QUIT to all channels (excluding the quitting client)
    for (std::vector<Channel*>::iterator it = channelsWithClient.begin();
         it != channelsWithClient.end(); ++it) {
        (*it)->broadcast(quitMsg, client); // Don't send to the quitting client
    }

    // Remove client from all channels
    _server->removeClientFromAllChannels(client);

    // Note: The actual client disconnection (closing socket, removing from server)
    // should be handled by the I/O layer, not here. We just handle the IRC protocol part.
}

void CommandHandlers::handleKick(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }

    if (params.size() < 2) {
        sendErrorReply(client, IRC::ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
        return;
    }

    const std::string& channelName = params[0];
    const std::string& targetNick = params[1];
    std::string kickReason = (params.size() > 2) ? params[2] : client->getNickname();

    // Validate channel name
    if (!validateChannelName(channelName)) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    // Get channel
    Channel* channel = _server->getChannel(channelName);
    if (!channel) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    // Check if client is on the channel
    if (!channel->hasClient(client)) {
        sendErrorReply(client, IRC::ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }

    // Check if client is operator
    if (!channel->isOperator(client)) {
        sendErrorReply(client, IRC::ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }

    // Find target client
    Client* targetClient = _server->findClientByNick(targetNick);
    if (!targetClient) {
        sendErrorReply(client, IRC::ERR_NOSUCHNICK, targetNick + " :No such nick/channel");
        return;
    }

    // Check if target is on the channel
    if (!channel->hasClient(targetClient)) {
        sendErrorReply(client, IRC::ERR_USERNOTINCHANNEL, targetNick + " " + channelName + " :They aren't on that channel");
        return;
    }

    // Send KICK message to all channel members (including target and kicker)
    std::string kickMsg = formatIRCMessage(client->getHostmask(), "KICK", channelName + " " + targetNick, kickReason);
    channel->broadcast(kickMsg, NULL); // Send to all including sender

    // Remove target from channel
    channel->removeClient(targetClient);

    // Delete channel if empty
    _server->deleteChannelIfEmpty(channel);
}

void CommandHandlers::handleInvite(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }

    if (params.size() < 2) {
        sendErrorReply(client, IRC::ERR_NEEDMOREPARAMS, "INVITE :Not enough parameters");
        return;
    }

    const std::string& targetNick = params[0];
    const std::string& channelName = params[1];

    // Validate channel name
    if (!validateChannelName(channelName)) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    // Find target client
    Client* targetClient = _server->findClientByNick(targetNick);
    if (!targetClient) {
        sendErrorReply(client, IRC::ERR_NOSUCHNICK, targetNick + " :No such nick/channel");
        return;
    }

    // Get channel
    Channel* channel = _server->getChannel(channelName);
    if (!channel) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    // Check if client is on the channel
    if (!channel->hasClient(client)) {
        sendErrorReply(client, IRC::ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }

    // Check if client is operator (required for INVITE)
    if (!channel->isOperator(client)) {
        sendErrorReply(client, IRC::ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }

    // Check if target is already on the channel
    if (channel->hasClient(targetClient)) {
        sendErrorReply(client, IRC::ERR_USERONCHANNEL, targetNick + " " + channelName + " :is already on channel");
        return;
    }

    // Add target to invite list
    channel->addInvite(targetClient);

    // Send INVITE confirmation to inviter
    std::string inviteReply = formatNumericReply("341", client->getNickname(), targetNick + " " + channelName);
    _server->queueMessage(client->getFd(), inviteReply);

    // Send INVITE notification to target
    std::string inviteMsg = formatIRCMessage(client->getHostmask(), "INVITE", targetNick, channelName);
    _server->queueMessage(targetClient->getFd(), inviteMsg);
}

void CommandHandlers::handleTopic(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }

    if (params.empty()) {
        sendErrorReply(client, IRC::ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
        return;
    }

    const std::string& channelName = params[0];

    // Validate channel name
    if (!validateChannelName(channelName)) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    // Get channel
    Channel* channel = _server->getChannel(channelName);
    if (!channel) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    // Check if client is on the channel
    if (!channel->hasClient(client)) {
        sendErrorReply(client, IRC::ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }

    if (params.size() == 1) {
        // View topic
        const std::string& topic = channel->getTopic();
        if (topic.empty()) {
            std::string noTopicReply = formatNumericReply(IRC::RPL_NOTOPIC, client->getNickname(), channelName + " :No topic is set");
            _server->queueMessage(client->getFd(), noTopicReply);
        } else {
            std::string topicReply = formatNumericReply(IRC::RPL_TOPIC, client->getNickname(), channelName + " :" + topic);
            _server->queueMessage(client->getFd(), topicReply);
        }
    } else {
        // Set topic - check if client has permission
        if (channel->isTopicRestricted() && !channel->isOperator(client)) {
            sendErrorReply(client, IRC::ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
            return;
        }

        const std::string& newTopic = params[1];
        channel->setTopic(newTopic);

        // Broadcast topic change to all channel members
        std::string topicMsg = formatIRCMessage(client->getHostmask(), "TOPIC", channelName, newTopic);
        channel->broadcast(topicMsg, NULL); // Send to all including sender
    }
}

void CommandHandlers::handleMode(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }

    if (params.empty()) {
        sendErrorReply(client, IRC::ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
        return;
    }

    const std::string& target = params[0];

    // Only handle channel modes (target starts with #)
    if (target[0] != '#') {
        sendErrorReply(client, IRC::ERR_UNKNOWNMODE, "User modes not supported");
        return;
    }

    // Validate channel name
    if (!validateChannelName(target)) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, target + " :No such channel");
        return;
    }

    // Get channel
    Channel* channel = _server->getChannel(target);
    if (!channel) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, target + " :No such channel");
        return;
    }

    // Check if client is on the channel
    if (!channel->hasClient(client)) {
        sendErrorReply(client, IRC::ERR_NOTONCHANNEL, target + " :You're not on that channel");
        return;
    }

    if (params.size() == 1) {
        // Query mode - return current channel modes
        std::string modeString = channel->getModeString();
        if (modeString.empty()) modeString = "+";

        std::string modeReply = formatNumericReply("324", client->getNickname(), target + " " + modeString);
        _server->queueMessage(client->getFd(), modeReply);
        return;
    }

    // Setting modes - check if client is operator
    if (!channel->isOperator(client)) {
        sendErrorReply(client, IRC::ERR_CHANOPRIVSNEEDED, target + " :You're not channel operator");
        return;
    }

    const std::string& modeString = params[1];
    std::vector<std::string> modeParams;
    for (size_t i = 2; i < params.size(); ++i) {
        modeParams.push_back(params[i]);
    }

    // Parse and apply modes
    bool adding = true;
    size_t paramIndex = 0;
    std::string appliedAdd = "";
    std::string appliedRemove = "";
    std::string appliedParams = "";

    for (size_t i = 0; i < modeString.length(); ++i) {
        char mode = modeString[i];

        if (mode == '+') {
            adding = true;
            continue;
        }
        if (mode == '-') {
            adding = false;
            continue;
        }

        switch (mode) {
            case 'i': // Invite only
                channel->setInviteOnly(adding);
                if (adding) appliedAdd += "i";
                else appliedRemove += "i";
                break;

            case 't': // Topic restricted
                channel->setTopicRestricted(adding);
                if (adding) appliedAdd += "t";
                else appliedRemove += "t";
                break;

            case 'k': // Channel key (password)
                if (adding && paramIndex < modeParams.size()) {
                    channel->setKey(modeParams[paramIndex]);
                    appliedAdd += "k";
                    appliedParams += " " + modeParams[paramIndex];
                    paramIndex++;
                } else if (!adding) {
                    channel->setKey("");
                    appliedRemove += "k";
                }
                break;

            case 'l': // User limit
                if (adding && paramIndex < modeParams.size()) {
                    int limitInt = std::atoi(modeParams[paramIndex].c_str());
                    if (limitInt > 0) {
                        size_t limit = static_cast<size_t>(limitInt);
                        channel->setUserLimit(limit);
                        appliedAdd += "l";
                        appliedParams += " " + modeParams[paramIndex];
                    }
                    paramIndex++;
                } else if (!adding) {
                    channel->setUserLimit(0);
                    appliedRemove += "l";
                }
                break;

            case 'o': // Operator privilege
                if (paramIndex < modeParams.size()) {
                    Client* targetClient = _server->findClientByNick(modeParams[paramIndex]);
                    if (targetClient && channel->hasClient(targetClient)) {
                        if (adding) {
                            channel->addOperator(targetClient);
                            appliedAdd += "o";
                        } else {
                            channel->removeOperator(targetClient);
                            appliedRemove += "o";
                        }
                        appliedParams += " " + modeParams[paramIndex];
                    }
                    paramIndex++;
                }
                break;

            default:
                sendErrorReply(client, IRC::ERR_UNKNOWNMODE, std::string(1, mode) + " :is unknown mode char to me");
                return;
        }
    }

    // Broadcast mode change to all channel members
    std::string finalModes = "";
    if (!appliedAdd.empty()) {
        finalModes += "+" + appliedAdd;
    }
    if (!appliedRemove.empty()) {
        finalModes += "-" + appliedRemove;
    }

    if (!finalModes.empty()) {
        std::string modeMsg = formatIRCMessage(client->getHostmask(), "MODE", target + " " + finalModes + appliedParams, "");
        channel->broadcast(modeMsg, NULL); // Send to all including sender
    }
}

// Utility functions
void CommandHandlers::sendWelcomeSequence(Client* client) {
    std::string nick = client->getNickname();
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_WELCOME, nick, "Welcome to the IRC Network " + client->getHostmask()));
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_YOURHOST, nick, "Your host is localhost, running version 1.0"));
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_CREATED, nick, "This server was created today"));
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_MYINFO, nick, "localhost 1.0 o o"));
    // Minimal ISUPPORT (005) to improve client compatibility
    _server->queueMessage(client->getFd(), formatNumericReply("005", nick, "CHANTYPES=# PREFIX=(o)@ CASEMAPPING=rfc1459 :are supported by this server"));
}

void CommandHandlers::sendErrorReply(Client* client, const std::string& code, const std::string& message) {
    std::string nick = client->getNickname().empty() ? "*" : client->getNickname();
    _server->queueMessage(client->getFd(), formatNumericReply(code, nick, message));
}

bool CommandHandlers::validateNickname(const std::string& nickname) {
    if (nickname.empty() || nickname.length() > 9) {
        return false;
    }

    // First character must be letter or special char
    char first = nickname[0];
    if (!std::isalpha(first) && first != '[' && first != ']' && first != '\\' &&
        first != '`' && first != '_' && first != '^' && first != '{' && first != '}') {
        return false;
    }

    // Rest can be alphanumeric or special chars
    for (size_t i = 1; i < nickname.length(); ++i) {
        char c = nickname[i];
        if (!std::isalnum(c) && c != '[' && c != ']' && c != '\\' &&
            c != '`' && c != '_' && c != '^' && c != '{' && c != '}' && c != '-') {
            return false;
        }
    }

    return true;
}

bool CommandHandlers::validateChannelName(const std::string& channel) {
    if (channel.empty() || channel[0] != '#' || channel.length() > 50) {
        return false;
    }

    // Channel names cannot contain spaces, commas, or control characters
    for (size_t i = 1; i < channel.length(); ++i) {
        char c = channel[i];
        if (c == ' ' || c == ',' || c == '\r' || c == '\n' || c == '\0') {
            return false;
        }
    }

    return true;
}

// Information commands implementation
void CommandHandlers::handleCap(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        return;
    }
    
    const std::string& subcommand = params[0];
    std::string nick = client->getNickname().empty() ? "*" : client->getNickname();
    
    if (subcommand == "LS") {
        _server->queueMessage(client->getFd(), formatIRCMessage("ircserv", "CAP", nick + " LS", ""));
    } else if (subcommand == "REQ") {
        _server->queueMessage(client->getFd(), formatIRCMessage("ircserv", "CAP", nick + " NAK", ""));
    } else if (subcommand == "END") {
        // CAP negotiation finished, client ready for normal operation
    }
}

void CommandHandlers::handleWho(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }
    
    std::string target = params.empty() ? "*" : params[0];
    std::string nick = client->getNickname();
    
    if (target.empty() || target == "*") {
        // WHO with no target - send end of WHO
        _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_ENDOFWHO, nick, "* :End of WHO list"));
        return;
    }
    
    if (target[0] == '#') {
        // WHO for a channel
        Channel* channel = _server->getChannel(target);
        if (!channel) {
            _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_ENDOFWHO, nick, target + " :End of WHO list"));
            return;
        }
        
        const std::set<Client*>& members = channel->getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
            Client* member = *it;
            std::string flags = "H"; // Here
            if (channel->isOperator(member)) flags += "@";
            
            std::string whoReply = target + " " + member->getUsername() + " " + member->getHostname() + 
                                 " ircserv " + member->getNickname() + " " + flags + " :0 " + member->getRealname();
            _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_WHOREPLY, nick, whoReply));
        }
    }
    
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_ENDOFWHO, nick, target + " :End of WHO list"));
}

void CommandHandlers::handleWhois(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }
    
    if (params.empty()) {
        sendErrorReply(client, IRC::ERR_NONICKNAMEGIVEN, "No nickname given");
        return;
    }
    
    const std::string& targetNick = params[0];
    Client* target = _server->findClientByNick(targetNick);
    
    if (!target) {
        sendErrorReply(client, IRC::ERR_NOSUCHNICK, targetNick + " :No such nick/channel");
        _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_ENDOFWHOIS, client->getNickname(), targetNick + " :End of WHOIS list"));
        return;
    }
    
    std::string nick = client->getNickname();
    
    // RPL_WHOISUSER
    std::string userInfo = targetNick + " " + target->getUsername() + " " + target->getHostname() + " * :" + target->getRealname();
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_WHOISUSER, nick, userInfo));
    
    // RPL_WHOISSERVER
    std::string serverInfo = targetNick + " ircserv :IRC Server";
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_WHOISSERVER, nick, serverInfo));
    
    // RPL_WHOISCHANNELS (channels the user is on)
    std::vector<Channel*> channels = _server->getClientChannels(target);
    if (!channels.empty()) {
        std::string channelList;
        for (std::vector<Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
            if (!channelList.empty()) channelList += " ";
            if ((*it)->isOperator(target)) channelList += "@";
            channelList += (*it)->getName();
        }
        _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_WHOISCHANNELS, nick, targetNick + " :" + channelList));
    }
    
    // RPL_ENDOFWHOIS
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_ENDOFWHOIS, nick, targetNick + " :End of WHOIS list"));
}

void CommandHandlers::handleList(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }
    
    std::string nick = client->getNickname();
    
    // RPL_LISTSTART
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_LISTSTART, nick, "Channel :Users Name"));
    
    // List all channels (simplified - in real implementation you'd iterate through server channels)
    // For now, we'll use a basic approach since channel iteration method isn't clear from the interface
    
    // RPL_LISTEND
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_LISTEND, nick, ":End of LIST"));
}

void CommandHandlers::handleNames(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }
    
    std::string nick = client->getNickname();
    
    if (params.empty()) {
        // NAMES with no parameters - end immediately
        _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_ENDOFNAMES, nick, "* :End of NAMES list"));
        return;
    }
    
    const std::string& channelName = params[0];
    Channel* channel = _server->getChannel(channelName);
    
    if (!channel) {
        _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_ENDOFNAMES, nick, channelName + " :End of NAMES list"));
        return;
    }
    
    // Build names list
    std::string namesList;
    const std::set<Client*>& members = channel->getMembers();
    for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
        if (!namesList.empty()) namesList += " ";
        if (channel->isOperator(*it)) namesList += "@";
        namesList += (*it)->getNickname();
    }
    
    if (!namesList.empty()) {
        _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_NAMREPLY, nick, "= " + channelName + " :" + namesList));
    }
    _server->queueMessage(client->getFd(), formatNumericReply(IRC::RPL_ENDOFNAMES, nick, channelName + " :End of NAMES list"));
}
