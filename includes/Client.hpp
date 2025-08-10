#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <deque>
#include <ctime>

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
    bool _welcomeSent;

    std::string _inputBuffer;
    std::string _outputBuffer;
    std::deque<std::string> _outBufQ;  // Message queue for better I/O handling
    time_t _lastActive;                // For timeout tracking

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
    time_t getLastActive() const;
    bool welcomeSent() const;

    // Setters
    void setNickname(const std::string& nick);
    void setUsername(const std::string& user);
    void setRealname(const std::string& realname);
    void setHostname(const std::string& hostname);
    void setReceivedPass(bool);
    void setReceivedNick(bool);
    void setReceivedUser(bool);
    void tryRegister();
    void updateLastActive();
    void setWelcomeSent(bool v);

    // Buffers
    void appendToInputBuffer(const std::string& data);
    std::string& getInputBuffer();
    std::string& getOutputBuffer();

    // Message queue handling
    void enqueueMessage(const std::string& message);
    bool hasMessagesToSend() const;
    void flushMessagesToOutputBuffer();

    // Line extraction for IRC command parsing
    std::string extractNextLine();
    bool hasCompleteLine() const;
};

#endif
