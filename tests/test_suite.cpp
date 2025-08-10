// Minimal offline test harness for ft_irc core logic (no real sockets)
#include "Server.hpp"
#include "Channel.hpp"
#include "Client.hpp"
#include "Command.hpp"
#include "IRCProtocol.hpp"
#include <iostream>
#include <vector>
#include <string>

static int testsRun = 0;
static int testsFailed = 0;

static void assertTrue(bool cond, const std::string &msg) {
    ++testsRun;
    if (!cond) {
        ++testsFailed;
        std::cout << "[FAIL] " << msg << std::endl;
    } else {
        std::cout << "[ OK ] " << msg << std::endl;
    }
}

static void test_server_client_channel_basics() {
    Server s;
    s.addClient(100);
    s.addClient(101);
    Client* a = s.getClient(100);
    Client* b = s.getClient(101);
    assertTrue(a && b, "clients created and retrievable");

    a->setNickname("Alice");
    b->setNickname("Bob");
    assertTrue(s.findClientByNick("Alice") == a, "find client by nick");
    assertTrue(s.isNicknameInUse("Bob"), "nickname in use check");

    Channel* ch = s.createChannel("#42");
    assertTrue(ch != NULL, "channel created");
    ch->addClient(a);
    ch->addClient(b);
    ch->addOperator(a);
    assertTrue(ch->hasClient(a) && ch->hasClient(b), "members added to channel");
    assertTrue(ch->isOperator(a) && !ch->isOperator(b), "operator set correctly");

    ch->setTopic("Welcome!");
    assertTrue(ch->getTopic() == "Welcome!", "topic set");

    // Broadcast formatting check (message should already be CRLF-terminated by formatIRCMessage)
    std::string msg = formatIRCMessage(a->getHostmask(), "PRIVMSG", "#42", "Hello");
    ch->broadcast(msg, a);
    // We cannot directly inspect b's output here since networking layer queues in Server;
    // this validates no crashes and uses existing path.
}

static void test_numeric_helpers() {
    std::string line = formatNumericReply(IRC::RPL_WELCOME, "Alice", "Welcome to the IRC Network Alice!\r\n");
    assertTrue(line.find(" 001 ") != std::string::npos, "RPL_WELCOME numeric formatting");
}

static std::string drainMessages(Server &s, Client* c) {
    std::string all;
    int fd = c->getFd();
    int guard = 0;
    while (s.hasClientMessagesToSend(fd) && guard < 1000) {
        s.flushClientMessages(fd);
        all += c->getOutputBuffer();
        c->getOutputBuffer().clear();
        ++guard;
    }
    return all;
}

static void test_registration_and_welcome() {
    Server s;
    s.setPassword("pw");
    s.addClient(10);
    Client* c = s.getClient(10);
    Command cmd(&s);

    // Simulate PASS/NICK/USER sequence through handlers indirectly by crafting input buffer and processing
    c->appendToInputBuffer("PASS pw\r\nNICK test\r\nUSER u 0 * :r\r\n");
    cmd.processClientBuffer(c);
    assertTrue(c->isRegistered(), "client registered after PASS/NICK/USER");
    std::string welcome = drainMessages(s, c);
    assertTrue(welcome.find(" 001 ") != std::string::npos, "welcome sequence includes 001");
    assertTrue(welcome.find(" 005 ") != std::string::npos, "ISUPPORT 005 included");
}

static void test_privmsg_errors() {
    Server s;
    s.setPassword("pw");
    s.addClient(20);
    Client* c = s.getClient(20);
    Command cmd(&s);
    // Register quickly
    c->appendToInputBuffer("PASS pw\r\nNICK t\r\nUSER u 0 * :r\r\n");
    cmd.processClientBuffer(c);
    (void)drainMessages(s, c); // discard welcome

    // No recipient
    c->appendToInputBuffer("PRIVMSG\r\n");
    cmd.processClientBuffer(c);
    std::string out = drainMessages(s, c);
    assertTrue(out.find(" 411 ") != std::string::npos, "ERR_NORECIPIENT (411) returned for PRIVMSG with no target");
    // No text to send
    c->appendToInputBuffer("PRIVMSG #x\r\n");
    cmd.processClientBuffer(c);
    out = drainMessages(s, c);
    assertTrue(out.find(" 412 ") != std::string::npos, "ERR_NOTEXTTOSEND (412) returned for PRIVMSG with no text");
}

static void test_ping_pong() {
    Server s;
    s.setPassword("pw");
    s.addClient(30);
    Client* c = s.getClient(30);
    Command cmd(&s);
    c->appendToInputBuffer("PASS pw\r\nNICK t\r\nUSER u 0 * :r\r\nPING :token\r\nPONG :res\r\n");
    cmd.processClientBuffer(c);
    assertTrue(c->isRegistered(), "registered before PING/PONG processed");
    std::string out2 = drainMessages(s, c);
    assertTrue(out2.find(" PONG ") != std::string::npos, "PONG reply sent on PING");
}

int main() {
    std::cout << "Running ft_irc tests..." << std::endl;
    test_server_client_channel_basics();
    test_numeric_helpers();
    test_registration_and_welcome();
    test_privmsg_errors();
    test_ping_pong();
    std::cout << "Tests run: " << testsRun << ", failed: " << testsFailed << std::endl;
    return testsFailed ? 1 : 0;
}
