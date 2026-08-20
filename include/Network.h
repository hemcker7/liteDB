#pragma once

#include <cstddef>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
using SocketHandle = SOCKET;
inline constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
#include <sys/socket.h>
using SocketHandle = int;
inline constexpr SocketHandle InvalidSocket = -1;
#endif

bool initializeSockets();
void shutdownSockets();
void closeSocket(SocketHandle socket);
bool sendAll(SocketHandle socket, const char* data, std::size_t size);
bool sendAll(SocketHandle socket, const std::string& data);
bool receiveLine(SocketHandle socket, std::string& line);
bool receiveBytes(SocketHandle socket, std::string& data, std::size_t size);
SocketHandle connectToServer(const std::string& host, unsigned short port);
SocketHandle createListeningSocket(unsigned short port);
