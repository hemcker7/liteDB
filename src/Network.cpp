#include "Network.h"
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

bool initializeSockets(){
#ifdef _WIN32
    WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return true;
#endif
}

void shutdownSockets(){
#ifdef _WIN32
    WSACleanup();
#endif
}

void closeSocket(SocketHandle socket){
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

bool sendAll(SocketHandle socket, const char* data, std::size_t size){
    while(size > 0){
#ifdef _WIN32
        const int sent = send(socket, data, static_cast<int>(std::min<std::size_t>(size, 1 << 20)), 0);
#else
        const auto sent = send(socket, data, std::min<std::size_t>(size, 1 << 20), 0);
#endif
        if(sent <= 0) return false;
        data += sent;
        size -= static_cast<std::size_t>(sent);
    }
    return true;
}

bool sendAll(SocketHandle socket, const std::string& data){
    return sendAll(socket, data.data(), data.size());
}

bool receiveLine(SocketHandle socket, std::string& line){
    line.clear();
    char character = 0;
    while(true){
#ifdef _WIN32
        const int received = recv(socket, &character, 1, 0);
#else
        const auto received = recv(socket, &character, 1, 0);
#endif
        if(received <= 0) return false;
        if(character == '\n') return true;
        if(character != '\r') line += character;
        if(line.size() > 1024 * 1024) return false;
    }
}

bool receiveBytes(SocketHandle socket, std::string& data, std::size_t size){
    data.resize(size);
    std::size_t offset = 0;
    while(offset < size){
#ifdef _WIN32
        const int received = recv(socket, data.data() + offset, static_cast<int>(std::min<std::size_t>(size - offset, 1 << 20)), 0);
#else
        const auto received = recv(socket, data.data() + offset, std::min<std::size_t>(size - offset, 1 << 20), 0);
#endif
        if(received <= 0) return false;
        offset += static_cast<std::size_t>(received);
    }
    return true;
}

SocketHandle connectToServer(const std::string& host, unsigned short port){
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    const auto portText = std::to_string(port);
    if(getaddrinfo(host.c_str(), portText.c_str(), &hints, &results) != 0) return InvalidSocket;
    SocketHandle socket = InvalidSocket;
    for(auto* result = results; result != nullptr; result = result->ai_next){
        socket = static_cast<SocketHandle>(::socket(result->ai_family, result->ai_socktype, result->ai_protocol));
        if(socket == InvalidSocket) continue;
        if(connect(socket, result->ai_addr, static_cast<int>(result->ai_addrlen)) == 0) break;
        closeSocket(socket);
        socket = InvalidSocket;
    }
    freeaddrinfo(results);
    return socket;
}

SocketHandle createListeningSocket(unsigned short port){
    SocketHandle socket = static_cast<SocketHandle>(::socket(AF_INET, SOCK_STREAM, 0));
    if(socket == InvalidSocket) return InvalidSocket;
    int reuse = 1;
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if(bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 || listen(socket, 16) != 0){
        closeSocket(socket);
        return InvalidSocket;
    }
    return socket;
}
