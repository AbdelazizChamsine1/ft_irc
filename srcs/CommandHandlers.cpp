#include "CommandHandlers.hpp"
#include "Server.hpp"
#include "Channel.hpp"

CommandHandlers::CommandHandlers(Server* server) : _server(server) {}

CommandHandlers::~CommandHandlers() {}

// Check if client should be registered and send welcome if needed
void CommandHandlers::checkRegistration(Client* client) {
    if (client->canRegister() && !client->welcomeSent()) {
        client->tryRegister();
        sendWelcomeSequence(client);
        client->setWelcomeSent(true);
        std::cout << "Client " << client->getFd() << " (" << client->getNickname() << ") registered successfully" << std::endl;
    }
}

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
    checkRegistration(client);
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

    std::string oldNick = client->getNickname();
    client->setNickname(nickname);
    client->setReceivedNick(true);
    
    // If already registered, send nick change notification to channels
    if (client->isRegistered() && !oldNick.empty()) {
        std::vector<Channel*> channels = _server->getClientChannels(client);
        std::string nickMsg = ":" + oldNick + "!" + client->getUsername() + "@" + client->getHostname() + " NICK :" + nickname + "\r\n";
        
        for (std::vector<Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
            (*it)->broadcast(nickMsg, NULL); // Send to all including the client
        }
    }
    
    checkRegistration(client);
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
    client->setRealname(params[3]);
    client->setReceivedUser(true);
    
    checkRegistration(client);
    std::cout << "Client " << client->getFd() << " registered with username: " << params[0] << std::endl;
}

// Keepalive commands
void CommandHandlers::handlePing(Client* client, const std::vector<std::string>& params) {
    std::string token = params.empty() ? "ircserv" : params[0];
    std::string pong = ":" + std::string("ircserv") + " PONG ircserv :" + token + "\r\n";
    _server->queueMessage(client->getFd(), pong);
}

void CommandHandlers::handlePong(Client* client, const std::vector<std::string>& params) {
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

    // Send JOIN confirmation to all channel members
    std::string joinMsg = ":" + client->getHostmask() + " JOIN :" + channelName + "\r\n";
    channel->broadcast(joinMsg, NULL); // Broadcast to all including sender

    // Send topic information to the joining client
    const std::string& topic = channel->getTopic();
    if (topic.empty()) {
        std::string noTopicReply = ":" + std::string("ircserv") + " " + IRC::RPL_NOTOPIC + " " + client->getNickname() + " " + channelName + " :No topic is set\r\n";
        _server->queueMessage(client->getFd(), noTopicReply);
    } else {
        std::string topicReply = ":" + std::string("ircserv") + " " + IRC::RPL_TOPIC + " " + client->getNickname() + " " + channelName + " :" + topic + "\r\n";
        _server->queueMessage(client->getFd(), topicReply);
    }

    // Send NAMES list to the joining client
    std::string namesList = "";
    const std::set<Client*>& members = channel->getMembers();
    for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
        if (!namesList.empty()) namesList += " ";
        if (channel->isOperator(*it)) {
            namesList += "@" + (*it)->getNickname();
        } else {
            namesList += (*it)->getNickname();
        }
    }
    
    std::string namesReply = ":" + std::string("ircserv") + " " + IRC::RPL_NAMREPLY + " " + client->getNickname() + " = " + channelName + " :" + namesList + "\r\n";
    _server->queueMessage(client->getFd(), namesReply);
    
    std::string endNamesReply = ":" + std::string("ircserv") + " " + IRC::RPL_ENDOFNAMES + " " + client->getNickname() + " " + channelName + " :End of /NAMES list\r\n";
    _server->queueMessage(client->getFd(), endNamesReply);
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

        std::string privmsg = ":" + client->getHostmask() + " PRIVMSG " + target + " :" + message + "\r\n";
        channel->broadcast(privmsg, client); // Don't send back to sender
    } else {
        // Private message to user
        Client* targetClient = _server->findClientByNick(target);
        if (!targetClient) {
            sendErrorReply(client, IRC::ERR_NOSUCHNICK, target + " :No such nick/channel");
            return;
        }

        std::string privmsg = ":" + client->getHostmask() + " PRIVMSG " + target + " :" + message + "\r\n";
        _server->queueMessage(targetClient->getFd(), privmsg);
    }
}

void CommandHandlers::handleNotice(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        return; // NOTICE doesn't send error replies
    }

    if (params.size() < 2) {
        return; // NOTICE doesn't send error replies
    }

    const std::string& target = params[0];
    const std::string& message = params[1];

    if (target[0] == '#') {
        // Channel notice
        Channel* channel = _server->getChannel(target);
        if (!channel || !channel->hasClient(client)) {
            return; // NOTICE doesn't send error replies
        }

        std::string noticeMsg = ":" + client->getHostmask() + " NOTICE " + target + " :" + message + "\r\n";
        channel->broadcast(noticeMsg, client);
    } else {
        // Private notice to user
        Client* targetClient = _server->findClientByNick(target);
        if (!targetClient) {
            return; // NOTICE doesn't send error replies
        }

        std::string noticeMsg = ":" + client->getHostmask() + " NOTICE " + target + " :" + message + "\r\n";
        _server->queueMessage(targetClient->getFd(), noticeMsg);
    }
}

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
    std::string partMsg = ":" + client->getHostmask() + " PART " + channelName;
    if (!partMessage.empty()) {
        partMsg += " :" + partMessage;
    }
    partMsg += "\r\n";
    
    channel->broadcast(partMsg, NULL); // Send to all including sender

    // Remove client from channel
    channel->removeClient(client);

    channel->promoteNewOperatorIfNeeded();

    // Delete channel if empty
    _server->deleteChannelIfEmpty(channel);
}

void CommandHandlers::handleQuit(Client* client, const std::vector<std::string>& params) {
    std::string quitMessage = params.empty() ? "Client Quit" : params[0];

    // Send QUIT message to all channels the client is in
    std::vector<Channel*> channelsWithClient = _server->getClientChannels(client);
    std::string quitMsg = ":" + client->getHostmask() + " QUIT :" + quitMessage + "\r\n";

    for (std::vector<Channel*>::iterator it = channelsWithClient.begin();
         it != channelsWithClient.end(); ++it) {
        (*it)->broadcast(quitMsg, client); // Don't send to the quitting client
    }

    // Remove client from all channels
    std::vector<Channel*> channelsToCheck = channelsWithClient;
    
    _server->removeClientFromAllChannels(client);

    for (std::vector<Channel*>::iterator it = channelsToCheck.begin();
         it != channelsToCheck.end(); ++it) {
        (*it)->promoteNewOperatorIfNeeded();
    }
}

// Operator commands (simplified implementations)
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

    if (!channel->isOperator(client)) {
        sendErrorReply(client, IRC::ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }

    Client* targetClient = _server->findClientByNick(targetNick);
    if (!targetClient) {
        sendErrorReply(client, IRC::ERR_NOSUCHNICK, targetNick + " :No such nick/channel");
        return;
    }

    if (!channel->hasClient(targetClient)) {
        sendErrorReply(client, IRC::ERR_USERNOTINCHANNEL, targetNick + " " + channelName + " :They aren't on that channel");
        return;
    }

    // Send KICK message to all channel members
    std::string kickMsg = ":" + client->getHostmask() + " KICK " + channelName + " " + targetNick + " :" + kickReason + "\r\n";
    channel->broadcast(kickMsg, NULL);

    // Remove target from channel
    channel->removeClient(targetClient);

    channel->promoteNewOperatorIfNeeded();

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

    if (!validateChannelName(channelName)) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    Client* targetClient = _server->findClientByNick(targetNick);
    if (!targetClient) {
        sendErrorReply(client, IRC::ERR_NOSUCHNICK, targetNick + " :No such nick/channel");
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

    if (!channel->isOperator(client)) {
        sendErrorReply(client, IRC::ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }

    if (channel->hasClient(targetClient)) {
        sendErrorReply(client, IRC::ERR_USERONCHANNEL, targetNick + " " + channelName + " :is already on channel");
        return;
    }

    // Add target to invite list
    channel->addInvite(targetClient);

    // Send INVITE confirmation to inviter
    std::string inviteReply = ":" + std::string("ircserv") + " 341 " + client->getNickname() + " " + targetNick + " " + channelName + "\r\n";
    _server->queueMessage(client->getFd(), inviteReply);

    // Send INVITE notification to target
    std::string inviteMsg = ":" + client->getHostmask() + " INVITE " + targetNick + " :" + channelName + "\r\n";
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

    if (params.size() == 1) {
        // View topic
        const std::string& topic = channel->getTopic();
        if (topic.empty()) {
            std::string noTopicReply = ":" + std::string("ircserv") + " " + IRC::RPL_NOTOPIC + " " + client->getNickname() + " " + channelName + " :No topic is set\r\n";
            _server->queueMessage(client->getFd(), noTopicReply);
        } else {
            std::string topicReply = ":" + std::string("ircserv") + " " + IRC::RPL_TOPIC + " " + client->getNickname() + " " + channelName + " :" + topic + "\r\n";
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
        std::string topicMsg = ":" + client->getHostmask() + " TOPIC " + channelName + " :" + newTopic + "\r\n";
        channel->broadcast(topicMsg, NULL);
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

    // Only handle channel modes
    if (target[0] != '#') {
        sendErrorReply(client, IRC::ERR_UNKNOWNMODE, "User modes not supported");
        return;
    }

    if (!validateChannelName(target)) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, target + " :No such channel");
        return;
    }

    Channel* channel = _server->getChannel(target);
    if (!channel) {
        sendErrorReply(client, IRC::ERR_NOSUCHCHANNEL, target + " :No such channel");
        return;
    }

    if (!channel->hasClient(client)) {
        sendErrorReply(client, IRC::ERR_NOTONCHANNEL, target + " :You're not on that channel");
        return;
    }

    if (params.size() == 1) {
        // Query mode - return current channel modes
        std::string modeString = channel->getModeString();
        if (modeString.empty()) modeString = "+";

        std::string modeReply = ":" + std::string("ircserv") + " 324 " + client->getNickname() + " " + target + " " + modeString + "\r\n";
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

    // Parse and apply modes (simplified)
    bool adding = true;
    size_t paramIndex = 0;
    std::string appliedModes = "";
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
                if (appliedModes.empty() || appliedModes[appliedModes.length() - 1] != (adding ? '+' : '-')) {
                    appliedModes += (adding ? '+' : '-');
                }
                appliedModes += "i";
                break;

            case 't': // Topic restricted
                channel->setTopicRestricted(adding);
                if (appliedModes.empty() || appliedModes[appliedModes.length() - 1] != (adding ? '+' : '-')) {
                    appliedModes += (adding ? '+' : '-');
                }
                appliedModes += "t";
                break;

            case 'k': // Channel key
                if (adding && paramIndex < modeParams.size()) {
                    channel->setKey(modeParams[paramIndex]);
                    if (appliedModes.empty() || appliedModes[appliedModes.length() - 1] != '+') {
                        appliedModes += "+";
                    }
                    appliedModes += "k";
                    appliedParams += " " + modeParams[paramIndex];
                    paramIndex++;
                } else if (!adding) {
                    channel->setKey("");
                    if (appliedModes.empty() || appliedModes[appliedModes.length() - 1] != '-') {
                        appliedModes += "-";
                    }
                    appliedModes += "k";
                }
                break;

            case 'l': // User limit
                if (adding && paramIndex < modeParams.size()) {
                    size_t limit = static_cast<size_t>(std::atoi(modeParams[paramIndex].c_str()));
                    if (limit > 0) {
                        channel->setUserLimit(limit);
                        if (appliedModes.empty() || appliedModes[appliedModes.length() - 1] != '+') {
                            appliedModes += "+";
                        }
                        appliedModes += "l";
                        appliedParams += " " + modeParams[paramIndex];
                    }
                    paramIndex++;
                } else if (!adding) {
                    channel->setUserLimit(0);
                    if (appliedModes.empty() || appliedModes[appliedModes.length() - 1] != '-') {
                        appliedModes += "-";
                    }
                    appliedModes += "l";
                }
                break;

            case 'o': // Operator privilege
                if (paramIndex < modeParams.size()) {
                    Client* targetClient = _server->findClientByNick(modeParams[paramIndex]);
                    if (targetClient && channel->hasClient(targetClient)) {
                        if (adding) {
                            channel->addOperator(targetClient);
                        } else {
                            channel->removeOperator(targetClient);
                        }
                        if (appliedModes.empty() || appliedModes[appliedModes.length() - 1] != (adding ? '+' : '-')) {
                            appliedModes += (adding ? '+' : '-');
                        }
                        appliedModes += "o";
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
    if (!appliedModes.empty()) {
        std::string modeMsg = ":" + client->getHostmask() + " MODE " + target + " " + appliedModes + appliedParams + "\r\n";
        channel->broadcast(modeMsg, NULL);
    }
}

// Information commands
void CommandHandlers::handleCap(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        return;
    }
    
    const std::string& subcommand = params[0];
    std::string nick = client->getNickname().empty() ? "*" : client->getNickname();
    
    if (subcommand == "LS") {
        std::string capReply = ":" + std::string("ircserv") + " CAP " + nick + " LS :\r\n";
        _server->queueMessage(client->getFd(), capReply);
    } else if (subcommand == "REQ") {
        std::string capReply = ":" + std::string("ircserv") + " CAP " + nick + " NAK :\r\n";
        _server->queueMessage(client->getFd(), capReply);
    } else if (subcommand == "END") {
        // CAP negotiation finished
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
        std::string endReply = ":" + std::string("ircserv") + " " + IRC::RPL_ENDOFWHO + " " + nick + " * :End of WHO list\r\n";
        _server->queueMessage(client->getFd(), endReply);
        return;
    }
    
    if (target[0] == '#') {
        Channel* channel = _server->getChannel(target);
        if (channel) {
            const std::set<Client*>& members = channel->getMembers();
            for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
                Client* member = *it;
                std::string flags = "H";
                if (channel->isOperator(member)) flags += "@";
                
                std::string whoReply = ":" + std::string("ircserv") + " " + IRC::RPL_WHOREPLY + " " + nick + " " + target + " " + 
                                     member->getUsername() + " " + member->getHostname() + " ircserv " + 
                                     member->getNickname() + " " + flags + " :0 " + member->getRealname() + "\r\n";
                _server->queueMessage(client->getFd(), whoReply);
            }
        }
    }
    
    std::string endReply = ":" + std::string("ircserv") + " " + IRC::RPL_ENDOFWHO + " " + nick + " " + target + " :End of WHO list\r\n";
    _server->queueMessage(client->getFd(), endReply);
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
        std::string endReply = ":" + std::string("ircserv") + " " + IRC::RPL_ENDOFWHOIS + " " + client->getNickname() + " " + targetNick + " :End of WHOIS list\r\n";
        _server->queueMessage(client->getFd(), endReply);
        return;
    }
    
    std::string nick = client->getNickname();
    
    // RPL_WHOISUSER
    std::string userReply = ":" + std::string("ircserv") + " " + IRC::RPL_WHOISUSER + " " + nick + " " + targetNick + " " + 
                           target->getUsername() + " " + target->getHostname() + " * :" + target->getRealname() + "\r\n";
    _server->queueMessage(client->getFd(), userReply);
    
    // RPL_WHOISSERVER
    std::string serverReply = ":" + std::string("ircserv") + " " + IRC::RPL_WHOISSERVER + " " + nick + " " + targetNick + " ircserv :IRC Server\r\n";
    _server->queueMessage(client->getFd(), serverReply);
    
    // RPL_WHOISCHANNELS
    std::vector<Channel*> channels = _server->getClientChannels(target);
    if (!channels.empty()) {
        std::string channelList;
        for (std::vector<Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
            if (!channelList.empty()) channelList += " ";
            if ((*it)->isOperator(target)) channelList += "@";
            channelList += (*it)->getName();
        }
        std::string channelsReply = ":" + std::string("ircserv") + " " + IRC::RPL_WHOISCHANNELS + " " + nick + " " + targetNick + " :" + channelList + "\r\n";
        _server->queueMessage(client->getFd(), channelsReply);
    }
    
    // RPL_ENDOFWHOIS
    std::string endReply = ":" + std::string("ircserv") + " " + IRC::RPL_ENDOFWHOIS + " " + nick + " " + targetNick + " :End of WHOIS list\r\n";
    _server->queueMessage(client->getFd(), endReply);
}

void CommandHandlers::handleList(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }
    
    (void)params; // Unused for now
    std::string nick = client->getNickname();
    
    // RPL_LISTSTART
    std::string startReply = ":" + std::string("ircserv") + " " + IRC::RPL_LISTSTART + " " + nick + " Channel :Users Name\r\n";
    _server->queueMessage(client->getFd(), startReply);
    
    // RPL_LISTEND
    std::string endReply = ":" + std::string("ircserv") + " " + IRC::RPL_LISTEND + " " + nick + " :End of LIST\r\n";
    _server->queueMessage(client->getFd(), endReply);
}

void CommandHandlers::handleNames(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        sendErrorReply(client, IRC::ERR_NOTREGISTERED, "You have not registered");
        return;
    }
    
    std::string nick = client->getNickname();
    
    if (params.empty()) {
        std::string endReply = ":" + std::string("ircserv") + " " + IRC::RPL_ENDOFNAMES + " " + nick + " * :End of NAMES list\r\n";
        _server->queueMessage(client->getFd(), endReply);
        return;
    }
    
    const std::string& channelName = params[0];
    Channel* channel = _server->getChannel(channelName);
    
    if (channel) {
        std::string namesList;
        const std::set<Client*>& members = channel->getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
            if (!namesList.empty()) namesList += " ";
            if (channel->isOperator(*it)) namesList += "@";
            namesList += (*it)->getNickname();
        }
        
        if (!namesList.empty()) {
            std::string namesReply = ":" + std::string("ircserv") + " " + IRC::RPL_NAMREPLY + " " + nick + " = " + channelName + " :" + namesList + "\r\n";
            _server->queueMessage(client->getFd(), namesReply);
        }
    }
    
    std::string endReply = ":" + std::string("ircserv") + " " + IRC::RPL_ENDOFNAMES + " " + nick + " " + channelName + " :End of NAMES list\r\n";
    _server->queueMessage(client->getFd(), endReply);
}

// Utility functions
void CommandHandlers::sendWelcomeSequence(Client* client) {
    std::string nick = client->getNickname();
    std::string hostname = client->getHostname();
    
    std::string welcome = ":" + std::string("ircserv") + " " + IRC::RPL_WELCOME + " " + nick + " :Welcome to the IRC Network " + client->getHostmask() + "\r\n";
    _server->queueMessage(client->getFd(), welcome);
    
    std::string yourhost = ":" + std::string("ircserv") + " " + IRC::RPL_YOURHOST + " " + nick + " :Your host is ircserv, running version 1.0\r\n";
    _server->queueMessage(client->getFd(), yourhost);
    
    std::string created = ":" + std::string("ircserv") + " " + IRC::RPL_CREATED + " " + nick + " :This server was created today\r\n";
    _server->queueMessage(client->getFd(), created);
    
    std::string myinfo = ":" + std::string("ircserv") + " " + IRC::RPL_MYINFO + " " + nick + " ircserv 1.0 o o\r\n";
    _server->queueMessage(client->getFd(), myinfo);
    
    // ISUPPORT
    std::string isupport = ":" + std::string("ircserv") + " 005 " + nick + " CHANTYPES=# PREFIX=(o)@ CASEMAPPING=rfc1459 :are supported by this server\r\n";
    _server->queueMessage(client->getFd(), isupport);
}

void CommandHandlers::sendErrorReply(Client* client, const std::string& code, const std::string& message) {
    std::string nick = client->getNickname().empty() ? "*" : client->getNickname();
    std::string reply = ":" + std::string("ircserv") + " " + code + " " + nick + " " + message + "\r\n";
    _server->queueMessage(client->getFd(), reply);
}

bool CommandHandlers::validateNickname(const std::string& nickname) {
    if (nickname.empty() || nickname.length() > 9) {
        return false;
    }

    char first = nickname[0];
    if (!std::isalpha(first) && first != '[' && first != ']' && first != '\\' &&
        first != '`' && first != '_' && first != '^' && first != '{' && first != '}') {
        return false;
    }

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

    for (size_t i = 1; i < channel.length(); ++i) {
        char c = channel[i];
        if (c == ' ' || c == ',' || c == '\r' || c == '\n' || c == '\0') {
            return false;
        }
    }

    return true;
}