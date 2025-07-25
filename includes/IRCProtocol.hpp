#ifndef IRCPROTOCOL_HPP
#define IRCPROTOCOL_HPP

#include <string>
#include <vector>

// IRC Numeric Reply Codes
namespace IRC {
    // Welcome messages
    const std::string RPL_WELCOME = "001";
    const std::string RPL_YOURHOST = "002";
    const std::string RPL_CREATED = "003";
    const std::string RPL_MYINFO = "004";
    
    // Channel operations
    const std::string RPL_NOTOPIC = "331";
    const std::string RPL_TOPIC = "332";
    const std::string RPL_NAMREPLY = "353";
    const std::string RPL_ENDOFNAMES = "366";
    
    // Error codes
    const std::string ERR_NOSUCHNICK = "401";
    const std::string ERR_NOSUCHCHANNEL = "403";
    const std::string ERR_CANNOTSENDTOCHAN = "404";
    const std::string ERR_TOOMANYCHANNELS = "405";
    const std::string ERR_UNKNOWNCOMMAND = "421";
    const std::string ERR_NONICKNAMEGIVEN = "431";
    const std::string ERR_ERRONEUSNICKNAME = "432";
    const std::string ERR_NICKNAMEINUSE = "433";
    const std::string ERR_USERNOTINCHANNEL = "441";
    const std::string ERR_NOTONCHANNEL = "442";
    const std::string ERR_USERONCHANNEL = "443";
    const std::string ERR_NOTREGISTERED = "451";
    const std::string ERR_NEEDMOREPARAMS = "461";
    const std::string ERR_ALREADYREGISTRED = "462";
    const std::string ERR_PASSWDMISMATCH = "464";
    const std::string ERR_CHANNELISFULL = "471";
    const std::string ERR_UNKNOWNMODE = "472";
    const std::string ERR_INVITEONLYCHAN = "473";
    const std::string ERR_BANNEDFROMCHAN = "474";
    const std::string ERR_BADCHANNELKEY = "475";
    const std::string ERR_CHANOPRIVSNEEDED = "482";
}

// IRC Command structure
struct IRCCommand {
    std::string prefix;
    std::string command;
    std::vector<std::string> params;
    std::string trailing;
};

// Protocol utility functions
std::string formatIRCMessage(const std::string& prefix, const std::string& command, 
                             const std::string& target, const std::string& message);
std::string formatNumericReply(const std::string& code, const std::string& target, 
                              const std::string& message);

#endif