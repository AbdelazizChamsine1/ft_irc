#include "Client.hpp"

Client::Client(int fd)
    : _fd(fd), _registered(false) {}

Client::~Client() {}

// Getters
int Client::getFd() const {
    return _fd;
}

const std::string& Client::getNickname() const {
    return _nickname;
}

const std::string& Client::getUsername() const {
    return _username;
}

bool Client::isRegistered() const {
    return _registered;
}

// Setters
void Client::setNickname(const std::string& nick) {
    _nickname = nick;
}

void Client::setUsername(const std::string& user) {
    _username = user;
}

void Client::setRegistered(bool status) {
    _registered = status;
}

// Buffer handling
void Client::appendToInputBuffer(const std::string& data) {
    _inputBuffer += data;
}

std::string& Client::getInputBuffer() {
    return _inputBuffer;
}

std::string& Client::getOutputBuffer() {
    return _outputBuffer;
}
