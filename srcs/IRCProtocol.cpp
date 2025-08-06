#include "IRCProtocol.hpp"
#include <sstream>

std::string formatIRCMessage(const std::string& prefix, const std::string& command, 
                             const std::string& target, const std::string& message) {
    std::ostringstream oss;
    
    if (!prefix.empty()) {
        oss << ":" << prefix << " ";
    }
    
    oss << command;
    
    if (!target.empty()) {
        oss << " " << target;
    }
    
    if (!message.empty()) {
        oss << " :" << message;
    }
    
    oss << "\r\n";
    return oss.str();
}

std::string formatNumericReply(const std::string& code, const std::string& target, 
                              const std::string& message) {
    std::ostringstream oss;
    oss << ":" << "localhost" << " " << code << " " << target << " :" << message << "\r\n";
    return oss.str();
}