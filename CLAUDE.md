# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an IRC (Internet Relay Chat) server implementation written in C++. The project follows a traditional IRC protocol architecture with a complete command system and protocol implementation.

### Core Components
- **Server** (`Server.hpp/cpp`): Main server class managing clients and channels
- **Client** (`Client.hpp/cpp`): Represents connected IRC clients with authentication state and I/O buffering
- **Channel** (`Channel.hpp/cpp`): Manages IRC channels, membership, operators, and modes
- **CommandHandlers** (`CommandHandlers.hpp/cpp`): Implements all IRC command handlers
- **IRCProtocol** (`IRCProtocol.hpp/cpp`): Defines IRC numeric codes and protocol utilities

## Development Commands

### Building the Project
**IMPORTANT**: The Makefile is currently empty and needs implementation. When implementing:
- Use C++98 standard (typical for 42 School projects)
- Include standard targets: `all`, `clean`, `fclean`, `re`
- Object files should go in an `obj/` directory
- Final executable should be named `ircserv`

### Testing
- **Comprehensive test suite**: `test.cpp` with 27 test cases covering all core functionality
- **Helper function**: `printTest()` for consistent test result formatting
- **Test compilation**: Once Makefile is implemented, compile test with object files
- **Test execution**: Run the test executable to validate server functionality

## Architecture Overview

### Server-Client Architecture
The server uses file descriptor-based client management:
- `std::map<int, Client*> _clients` maps socket FDs to Client objects
- `std::map<std::string, Channel*> _channels` manages all channels
- All client operations are FD-based for network integration

### Client Lifecycle
1. **Connection**: `Server::addClient(int fd)` creates new Client with FD
2. **Authentication**: 3-step process (PASS → NICK → USER)
3. **Registration**: `Client::tryRegister()` completes after all auth steps
4. **Active**: Client can join channels and send messages
5. **Cleanup**: `Server::removeClient(int fd)` handles disconnection

### Channel Management
- **Creation**: `Server::createChannel()` returns existing or creates new
- **Membership**: `Channel::addClient()` / `Channel::removeClient()`
- **Broadcasting**: `Channel::broadcast()` sends to all members except sender
- **Auto-cleanup**: Empty channels are automatically deleted

### Message Flow
- **Input**: `Client::_inputBuffer` accumulates partial messages
- **Processing**: Command handlers parse and execute IRC commands
- **Output**: `Client::_outputBuffer` queues responses via `Server::queueMessage()`

## Command System

### Authentication Commands (CommandHandlers.hpp:22-24)
- `handlePass()`: Server password authentication
- `handleNick()`: Nickname selection with uniqueness validation
- `handleUser()`: Username and realname registration

### Communication Commands (CommandHandlers.hpp:26-31)
- `handleJoin()`: Channel joining with invite/key validation
- `handlePart()`: Channel leaving with optional message
- `handlePrivmsg()`: Private messages to users or channels
- `handleQuit()`: Client disconnection with broadcast

### Operator Commands (CommandHandlers.hpp:33-37)
- `handleKick()`: Remove users from channels (operators only)
- `handleInvite()`: Invite users to invite-only channels
- `handleTopic()`: Set/get channel topics
- `handleMode()`: Channel mode management (+i, +t, +k, +l, +o)

### IRC Protocol Implementation
- **Numeric replies**: Full RFC-compliant error codes in `IRCProtocol.hpp`
- **Message formatting**: `formatIRCMessage()` and `formatNumericReply()`
- **Validation**: Nickname and channel name validation functions

## Key Implementation Details

### Client Registration State Tracking
```cpp
// Client.hpp:13-16 - Registration flags
bool _receivedPass;  // PASS command received
bool _receivedNick;  // NICK command received  
bool _receivedUser;  // USER command received
bool _registered;    // All three completed
```

### Channel Modes System
```cpp
// Channel.hpp:17-21 - Mode flags
bool _inviteOnly;      // +i mode
bool _topicRestricted; // +t mode
std::string _key;      // +k mode (password)
size_t _userLimit;     // +l mode (0 = no limit)
```

### Invite System
- **Session-based**: `Channel::_invitedClients` set tracks invitations
- **Automatic cleanup**: Invites cleared when client joins or leaves
- **Operator-only**: Only channel operators can send invites

### Buffer Management
- **Input buffering**: Handles partial TCP packets in `Client::_inputBuffer`
- **Output buffering**: Queues messages for async sending in `Client::_outputBuffer`
- **Message queuing**: `Server::queueMessage(int fd, string)` appends to client buffer

## Testing Strategy

The test suite (`test.cpp`) provides comprehensive coverage:
- **Client Management**: Add/remove/lookup operations (Tests 1-7)
- **Channel Operations**: Creation, membership, operators (Tests 8-13)
- **Messaging**: Client queuing, channel broadcasting (Tests 14-16)
- **Cleanup**: Client removal, channel auto-deletion (Tests 17-23)
- **Stress Testing**: Multiple clients/channels, mass operations (Tests 24-25)
- **Edge Cases**: Empty server, invalid operations (Tests 26-27)

## Integration Points

### Network Layer Integration
The server is designed for easy network integration:
- All operations are FD-based
- Input/output buffers ready for async I/O
- Clean separation between protocol logic and network handling

### Missing Components
- **Makefile**: Build system implementation needed
- **Network I/O**: Socket creation, select/poll loop, actual data transmission
- **main.cpp**: Currently minimal, needs server startup logic