#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <vector>
#include <cstring>
#include <cerrno>
#include <ctime>
#include "Server.hpp"
#include "Command.hpp"
#include "utils.hpp"

// Global variables for signal handling
volatile sig_atomic_t g_shutdown = 0;
Server* g_server = NULL;
Command* g_commandProcessor = NULL;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived SIGINT, shutting down gracefully..." << std::endl;
        g_shutdown = 1;
    }
}

void setupSignalHandling() {
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

int createListeningSocket(int port) {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        perror("socket");
        return -1;
    }

    // Set SO_REUSEADDR
    int opt = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt SO_REUSEADDR");
        close(serverFd);
        return -1;
    }

    // Set non-blocking
    if (fcntl(serverFd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl O_NONBLOCK");
        close(serverFd);
        return -1;
    }

    // Bind
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("bind");
        close(serverFd);
        return -1;
    }

    // Listen
    if (listen(serverFd, SOMAXCONN) == -1) {
        perror("listen");
        close(serverFd);
        return -1;
    }

    std::cout << "Server is listening on port " << port << std::endl;
    return serverFd;
}

void handleNewConnection(int serverFd, Server& server, std::vector<pollfd>& pollFds) {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    // Accept all available connections
    while (true) {
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (clientFd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more connections to accept
                break;
            } else {
                perror("accept");
                break;
            }
        }

        // Set client socket non-blocking
        if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
            perror("fcntl client O_NONBLOCK");
            close(clientFd);
            continue;
        }

        // Add client to server
        server.addClient(clientFd);
        
        // Set hostname for the client
        Client* client = server.getClient(clientFd);
        if (client) {
            client->setHostname("localhost");
        }

        // Add to poll vector
        pollfd clientPollFd;
        clientPollFd.fd = clientFd;
        clientPollFd.events = POLLIN;
        clientPollFd.revents = 0;
        pollFds.push_back(clientPollFd);

        // Get client IP address
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        
        std::cout << "New client connected: " << clientFd << " (IP: " << clientIP << ")" << std::endl;
    }
}

std::string getClientDisplayName(Client* client) {
    if (client->getNickname().empty()) {
        return "(unknown)";
    }
    return client->getNickname();
}

void logCommand(int clientFd, const std::string& command, Server& server) {
    Client* client = server.getClient(clientFd);
    if (client) {
        std::cout << "Parsing command from client " << clientFd << " (" 
                  << getClientDisplayName(client) << "): " << command << std::endl;
    }
}

void handleClientRead(int clientFd, Server& server, Command& commandProcessor) {
    Client* client = server.getClient(clientFd);
    if (!client) {
        return;
    }

    char buffer[4096];
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0) {
        if (bytesRead == 0) {
            std::cout << "Client " << clientFd << " (" << getClientDisplayName(client) 
                      << ") disconnected" << std::endl;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cout << "Client " << clientFd << " (" << getClientDisplayName(client) 
                      << ") connection error: " << strerror(errno) << std::endl;
        }
        
        // Handle disconnection - send QUIT to channels
        if (client->isRegistered()) {
            std::vector<Channel*> clientChannels = server.getClientChannels(client);
            std::string quitMsg = ":" + client->getHostmask() + " QUIT :Client disconnected\r\n";
            
            for (std::vector<Channel*>::iterator it = clientChannels.begin(); 
                 it != clientChannels.end(); ++it) {
                (*it)->broadcast(quitMsg, client);
            }
        }
        
        // Remove client from server (this will clean up channels too)
        server.removeClient(clientFd);
        return;
    }

    buffer[bytesRead] = '\0';
    
    // Convert \n to \r\n for IRC compatibility
    std::string data(buffer, bytesRead);
    std::string ircData = "";
    for (size_t i = 0; i < data.length(); i++) {
        if (data[i] == '\n' && (i == 0 || data[i-1] != '\r')) {
            ircData += "\r\n";
        } else if (data[i] != '\r' || (i + 1 < data.length() && data[i+1] != '\n')) {
            ircData += data[i];
        }
    }
    
    client->appendToInputBuffer(ircData);

    // Store registration state before processing
    bool wasRegistered = client->isRegistered();
    
    // Process commands and log them
    std::string& inputBuffer = client->getInputBuffer();
    size_t startPos = 0;
    
    // Extract and log commands before processing
    std::string tempBuffer = inputBuffer;
    while (true) {
        size_t pos = tempBuffer.find("\r\n", startPos);
        if (pos == std::string::npos) break;
        
        std::string command = tempBuffer.substr(startPos, pos - startPos);
        if (!command.empty()) {
            logCommand(clientFd, command, server);
        }
        startPos = pos + 2;
    }
    
    // Process all commands
    commandProcessor.processClientBuffer(client);
    
    // Check if client became registered
    if (!wasRegistered && client->isRegistered()) {
        std::cout << "Welcome sent to " << client->getNickname() << std::endl;
    }
}

void handleClientWrite(int clientFd, Server& server) {
    Client* client = server.getClient(clientFd);
    if (!client) {
        return;
    }

    // Flush queued messages to output buffer
    server.flushClientMessages(clientFd);

    std::string& outputBuffer = client->getOutputBuffer();
    if (outputBuffer.empty()) {
        return;
    }

    ssize_t bytesSent = send(clientFd, outputBuffer.c_str(), outputBuffer.length(), 0);

    if (bytesSent == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("send");
        }
        return;
    }

    // Remove sent bytes from buffer
    outputBuffer.erase(0, bytesSent);
}

void removeClientFromPoll(int clientFd, std::vector<pollfd>& pollFds) {
    for (std::vector<pollfd>::iterator it = pollFds.begin(); it != pollFds.end(); ++it) {
        if (it->fd == clientFd) {
            pollFds.erase(it);
            break;
        }
    }
}

void updatePollEvents(std::vector<pollfd>& pollFds, Server& server) {
    for (size_t i = 1; i < pollFds.size(); ++i) { // Skip server socket at index 0
        Client* client = server.getClient(pollFds[i].fd);
        if (client) {
            pollFds[i].events = POLLIN;
            // Add POLLOUT if there's data to send
            if (server.hasClientMessagesToSend(pollFds[i].fd)) {
                pollFds[i].events |= POLLOUT;
            }
        }
    }
}

void handleClientDisconnection(int clientFd, Server& server, std::vector<pollfd>& pollFds) {
    Client* client = server.getClient(clientFd);
    if (client && client->isRegistered()) {
        // Send QUIT to all channels the client is in
        std::vector<Channel*> clientChannels = server.getClientChannels(client);
        std::string quitMsg = ":" + client->getHostmask() + " QUIT :Client disconnected\r\n";
        
        for (std::vector<Channel*>::iterator it = clientChannels.begin(); 
             it != clientChannels.end(); ++it) {
            (*it)->broadcast(quitMsg, client);
        }
    }
    
    close(clientFd);
    server.removeClient(clientFd);
    removeClientFromPoll(clientFd, pollFds);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "Error: Invalid port number" << std::endl;
        return 1;
    }

    std::string password = argv[2];
    if (password.empty()) {
        std::cerr << "Error: Password cannot be empty" << std::endl;
        return 1;
    }

    // Print password as first line
    std::cout << password << std::endl;

    // Setup signal handling
    setupSignalHandling();

    // Create listening socket
    int serverFd = createListeningSocket(port);
    if (serverFd == -1) {
        return 1;
    }

    // Create server instance
    Server server;
    server.setPassword(password);
    g_server = &server;

    // Create command processor
    Command commandProcessor(&server);
    g_commandProcessor = &commandProcessor;

    // Setup poll vector
    std::vector<pollfd> pollFds;
    pollfd serverPollFd;
    serverPollFd.fd = serverFd;
    serverPollFd.events = POLLIN;
    serverPollFd.revents = 0;
    pollFds.push_back(serverPollFd);

    // Variables for timeout handling
    time_t lastTimeoutCheck = time(NULL);
    const int TIMEOUT_CHECK_INTERVAL = 60; // Check every 60 seconds
    const int CLIENT_TIMEOUT = 300; // 5 minutes idle timeout

    // Main poll loop
    while (!g_shutdown) {
        // Update poll events for clients with data to send
        updatePollEvents(pollFds, server);

        // Check for idle clients periodically
        time_t currentTime = time(NULL);
        if (currentTime - lastTimeoutCheck >= TIMEOUT_CHECK_INTERVAL) {
            server.disconnectIdleClients(CLIENT_TIMEOUT);
            lastTimeoutCheck = currentTime;
        }

        // Poll with 100ms timeout
        int pollResult = poll(&pollFds[0], pollFds.size(), 100);

        if (pollResult == -1) {
            if (errno == EINTR) {
                // Interrupted by signal, continue
                continue;
            }
            perror("poll");
            break;
        }

        if (pollResult == 0) {
            // Timeout, continue
            continue;
        }

        // Check server socket for new connections
        if (pollFds[0].revents & POLLIN) {
            handleNewConnection(serverFd, server, pollFds);
        }

        // Check client sockets
        for (size_t i = 1; i < pollFds.size(); ) {
            int clientFd = pollFds[i].fd;
            short revents = pollFds[i].revents;

            // Check for errors or hangup
            if (revents & (POLLERR | POLLHUP)) {
                Client* client = server.getClient(clientFd);
                std::cout << "Client " << clientFd << " (" << getClientDisplayName(client) 
                          << ") error/hangup" << std::endl;
                
                // Handle disconnection - send QUIT to channels
                if (client && client->isRegistered()) {
                    std::vector<Channel*> clientChannels = server.getClientChannels(client);
                    std::string quitMsg = ":" + client->getHostmask() + " QUIT :Client disconnected\r\n";
                    
                    for (std::vector<Channel*>::iterator it = clientChannels.begin(); 
                         it != clientChannels.end(); ++it) {
                        (*it)->broadcast(quitMsg, client);
                    }
                }
                
                close(clientFd);
                server.removeClient(clientFd);
                removeClientFromPoll(clientFd, pollFds);
                continue; // Don't increment i
            }

            // Handle read
            if (revents & POLLIN) {
                handleClientRead(clientFd, server, commandProcessor);

                // Check if client disconnected during command processing
                Client* client = server.getClient(clientFd);
                if (!client) {
                    std::cout << "Client " << clientFd << " disconnected during processing" << std::endl;
                    close(clientFd);
                    removeClientFromPoll(clientFd, pollFds);
                    continue; // Don't increment i
                }
            }

            // Handle write
            if (revents & POLLOUT) {
                handleClientWrite(clientFd, server);
            }

            ++i;
        }
    }

    // Cleanup
    std::cout << "Shutting down server..." << std::endl;
    close(serverFd);

    // Close all client connections gracefully
    for (size_t i = 1; i < pollFds.size(); ++i) {
        int clientFd = pollFds[i].fd;
        Client* client = server.getClient(clientFd);
        if (client && client->isRegistered()) {
            // Send a final message to registered clients
            std::string shutdownMsg = ":localhost ERROR :Server shutting down\r\n";
            send(clientFd, shutdownMsg.c_str(), shutdownMsg.length(), 0);
        }
        close(clientFd);
    }

    std::cout << "Server shutdown complete." << std::endl;
    return 0;
}