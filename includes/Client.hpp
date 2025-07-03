#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client {
private:
    int _fd;
    std::string _nickname;
    std::string _username;
    std::string _realname;
    std::string _hostname;
    bool _receivedPass;
    bool _receivedNick;
    bool _receivedUser;
    bool _registered;

    std::string _inputBuffer;
    std::string _outputBuffer;

public:
    Client(int fd);
    ~Client();

    // Getters
    int getFd() const;
    const std::string& getNickname() const;
    const std::string& getUsername() const;
    const std::string& getRealname() const;
    const std::string& getHostname() const;
    std::string getHostmask() const;
    bool isRegistered() const;

    // Setters
    void setNickname(const std::string& nick);
    void setUsername(const std::string& user);
    void setRealname(const std::string& realname);
    void setHostname(const std::string& hostname);
    void setReceivedPass(bool);
    void setReceivedNick(bool);
    void setReceivedUser(bool);
    void tryRegister();

    // Buffers
    void appendToInputBuffer(const std::string& data);
    std::string& getInputBuffer();
    std::string& getOutputBuffer();
};

#endif
