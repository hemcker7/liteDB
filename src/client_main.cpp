#include "Network.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv){
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const unsigned short port = argc > 2 ? static_cast<unsigned short>(std::strtoul(argv[2], nullptr, 10)) : 9001;
    if(!initializeSockets()){
        std::cerr << "Unable to initialize sockets.\n";
        return 1;
    }
    const auto socket = connectToServer(host, port);
    if(socket == InvalidSocket){
        std::cerr << "Unable to connect to " << host << ':' << port << ".\n";
        shutdownSockets();
        return 1;
    }
    std::cout << "Connected to litedb server.\n";
    std::string input;
    while(std::cout << "litedb> " && std::getline(std::cin, input)){
        if(input == ".exit") break;
        if(!sendAll(socket, input + "\n")) break;
        std::string header;
        if(!receiveLine(socket, header)) break;
        std::istringstream response(header);
        std::string marker;
        int code = -1;
        std::size_t size = 0;
        if(!(response >> marker >> code >> size) || marker != "RESULT"){
            std::cerr << "Invalid server response.\n";
            break;
        }
        std::string payload;
        if(!receiveBytes(socket, payload, size)) break;
        if(!payload.empty()) std::cout << payload;
    }
    closeSocket(socket);
    shutdownSockets();
}
