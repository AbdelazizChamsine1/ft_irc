# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Development Commands

### Building the Application
- `make` or `make all` - Build the IRC server executable (`ircserv`)
- `make clean` - Remove object files  
- `make fclean` - Remove object files and executables
- `make re` - Clean rebuild (fclean + all)

### Testing
- `make test` - Build and run the test suite (`irc_tests`)
- Tests are located in `tests/test_suite.cpp` and provide offline unit tests for core IRC logic

### Running the Server
```bash
./ircserv <port> <password>
```
Example: `./ircserv 6667 mypassword`

## Project Architecture

This is a **C++98-compliant IRC server implementation** using a **poll-based I/O multiplexing architecture** with the following key design patterns:

### Core Components

1. **Server Class** (`Server.hpp/cpp`)
   - Central coordinator managing clients and channels
   - Handles client lifecycle, message queuing, and I/O buffering
   - Maintains maps of clients (by socket FD) and channels (by name)

2. **Client Class** (`Client.hpp/cpp`)
   - Represents an IRC client connection with state management
   - Handles registration process (PASS → NICK → USER)
   - Manages input/output buffers for non-blocking I/O

3. **Channel Class** (`Channel.hpp/cpp`) 
   - IRC channel implementation with member management
   - Supports operators, topics, and message broadcasting
   - Handles channel modes and permissions

4. **Command Processing System**
   - **Command** (`Command.hpp/cpp`) - Main command parser and dispatcher
   - **CommandHandlers** (`CommandHandlers.hpp/cpp`) - Individual IRC command implementations
   - **IRCProtocol** (`IRCProtocol.hpp/cpp`) - IRC protocol constants and message formatting

### Architecture Flow

The server follows this event-driven pattern:
1. **main.cpp** - Sets up poll() loop with signal handling and timeout management
2. **Server** - Manages client/channel state and message queuing 
3. **Command** - Parses IRC commands from client input buffers
4. **CommandHandlers** - Executes specific IRC commands (JOIN, PRIVMSG, etc.)
5. I/O layer flushes queued messages to client output buffers

### Key Design Decisions

- **Non-blocking I/O**: All sockets use `O_NONBLOCK` with poll() for scalability
- **Buffer Management**: Separate input/output buffers per client for command parsing and response queuing
- **C++98 Compliance**: Uses STL containers, no C++11+ features
- **Signal Safety**: Graceful shutdown on SIGINT with proper cleanup
- **Idle Timeout**: 5-minute client timeout with periodic cleanup

### IRC Protocol Support

The server implements core IRC functionality:
- Client registration (PASS/NICK/USER)
- Channel operations (JOIN/PART/PRIVMSG/NOTICE) 
- Channel management (KICK/INVITE/TOPIC/MODE)
- Connection management (PING/PONG/QUIT)

## Development Notes

- Uses `poll()` for cross-platform I/O multiplexing
- Message formatting handles IRC CRLF line endings
- Client registration requires all three commands (PASS, NICK, USER) before full access
- Channel names must start with '#' 
- Operator privileges managed per-channel