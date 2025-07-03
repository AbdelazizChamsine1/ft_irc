#include "Client.hpp"

Client::Client(int fd)
    : _fd(fd),
      _receivedPass(false),
      _receivedNick(false),
      _receivedUser(false),
      _registered(false) {}

Client::~Client() {}

// Getters
int Client::getFd() const { return _fd; }

const std::string& Client::getNickname() const { return _nickname; }

const std::string& Client::getUsername() const { return _username; }

const std::string& Client::getRealname() const { return _realname; }

const std::string& Client::getHostname() const { return _hostname; }

std::string Client::getHostmask() const {
    return _nickname + "!" + _username + "@" + _hostname;
}

bool Client::isRegistered() const { return _registered; }

// Setters
void Client::setNickname(const std::string& nick) {
    _nickname = nick;
    _receivedNick = true;
    tryRegister();
}

void Client::setUsername(const std::string& user) {
    _username = user;
    _receivedUser = true;
    tryRegister();
}

void Client::setRealname(const std::string& realname) {
    _realname = realname;
}

void Client::setHostname(const std::string& hostname) {
    _hostname = hostname;
}

void Client::setReceivedPass(bool received) {
    _receivedPass = received;
    tryRegister();
}

void Client::setReceivedNick(bool received) {
    _receivedNick = received;
    tryRegister();
}

void Client::setReceivedUser(bool received) {
    _receivedUser = received;
    tryRegister();
}

// Auto-register when all fields are filled
void Client::tryRegister() {
    if (_receivedPass && _receivedNick && _receivedUser && !_registered) {
        _registered = true;
    }
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
