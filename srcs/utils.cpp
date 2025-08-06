#include "utils.hpp"
#include <sstream>

namespace IRCUtils {
    std::vector<std::string> extractLines(std::string& buffer) {
        std::vector<std::string> lines;
        size_t pos = 0;

        while ((pos = buffer.find("\r\n")) != std::string::npos) {
            lines.push_back(buffer.substr(0, pos));
            buffer.erase(0, pos + 2);
        }

        return lines;
    }

    IRCMessage parseIRCMessage(const std::string& line) {
        IRCMessage msg;
        std::istringstream iss(line);
        std::string token;

        // Check for prefix
        if (line[0] == ':') {
            iss >> msg.prefix;
            msg.prefix = msg.prefix.substr(1); // Remove the ':'
        }

        // Get command
        iss >> msg.command;

        // Get parameters
        std::string param;
        while (iss >> param) {
            if (param[0] == ':') {
                // Trailing parameter - collect rest of line
                std::string trailing = param.substr(1);
                std::string rest;
                std::getline(iss, rest);
                if (!rest.empty()) {
                    trailing += rest;
                }
                msg.params.push_back(trailing);
                break;
            } else {
                msg.params.push_back(param);
            }
        }

        return msg;
    }

    std::string formatReply(int code, const std::string& target, const std::string& message) {
        std::ostringstream oss;
        oss << ":" << "irc.server.local" << " ";

        if (code < 100) oss << "0";
        if (code < 10) oss << "0";
        oss << code << " " << target << " " << message;

        return oss.str();
    }
}
