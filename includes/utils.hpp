#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

// IRC message parsing utilities
namespace IRCUtils {
    // Extract complete IRC lines from a buffer
    std::vector<std::string> extractLines(std::string& buffer);

    // Parse IRC command line into components
    struct IRCMessage {
        std::string prefix;
        std::string command;
        std::vector<std::string> params;
    };

    IRCMessage parseIRCMessage(const std::string& line);

    // IRC reply formatting
    std::string formatReply(int code, const std::string& target, const std::string& message);

    // Common IRC numeric replies
    const int RPL_WELCOME = 001;
    const int RPL_YOURHOST = 002;
    const int RPL_CREATED = 003;
    const int RPL_MYINFO = 004;
    const int ERR_NONICKNAMEGIVEN = 431;
    const int ERR_ERRONEUSNICKNAME = 432;
    const int ERR_NICKNAMEINUSE = 433;
    const int ERR_NEEDMOREPARAMS = 461;
    const int ERR_ALREADYREGISTRED = 462;
    const int ERR_PASSWDMISMATCH = 464;
}

#endif
