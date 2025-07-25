#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>

class Client; // Forward declaration

class Channel {
private:
    std::string _name;
    std::string _topic;
    std::set<Client*> _members;
    std::set<Client*> _operators;
    
    // Channel modes
    bool _inviteOnly;      // +i mode
    bool _topicRestricted; // +t mode
    std::string _key;      // +k mode (password)
    size_t _userLimit;     // +l mode (0 = no limit)
    
    // Invite list (simple session-based)
    std::set<Client*> _invitedClients;

public:
    Channel(const std::string& name);
    ~Channel();

    // Basic info
    const std::string& getName() const;
    const std::string& getTopic() const;
    void setTopic(const std::string& topic);

    // Membership
    void addClient(Client* client);
    void removeClient(Client* client);
    bool hasClient(Client* client) const;
    const std::set<Client*>& getMembers() const;


    // Operators
    void addOperator(Client* client);
    void removeOperator(Client* client);
    bool isOperator(Client* client) const;

    // Messaging
    void broadcast(const std::string& message, Client* sender);
    
    // Modes
    bool isInviteOnly() const;
    bool isTopicRestricted() const;
    const std::string& getKey() const;
    size_t getUserLimit() const;
    
    void setInviteOnly(bool inviteOnly);
    void setTopicRestricted(bool topicRestricted);
    void setKey(const std::string& key);
    void setUserLimit(size_t limit);
    
    std::string getModeString() const;
    
    // Invite management
    void addInvite(Client* client);
    void removeInvite(Client* client);
    bool isInvited(Client* client) const;
};

#endif
