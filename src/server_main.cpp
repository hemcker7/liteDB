#include "Database.h"
#include "Network.h"
#include "Statement.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

namespace {
struct ServerState{
    Database database;
    std::mutex mutex;
    std::optional<std::uint64_t> transactionOwner;

    explicit ServerState(const std::string& filename) : database(filename) {}
};

std::string errorText(ExecuteResult result){
    switch(result){
        case ExecuteResult::DuplicateKey: return "duplicate primary key";
        case ExecuteResult::TableNotFound: return "table not found";
        case ExecuteResult::InvalidOperation: return "invalid operation";
        case ExecuteResult::TransactionAlreadyActive: return "transaction already active";
        case ExecuteResult::NoActiveTransaction: return "no active transaction";
        default: return "";
    }
}

void sendResponse(SocketHandle socket, int code, const std::string& payload){
    const auto header = "RESULT " + std::to_string(code) + " " + std::to_string(payload.size()) + "\n";
    sendAll(socket, header);
    sendAll(socket, payload);
}

void serveClient(SocketHandle socket, std::uint64_t clientId, ServerState& state){
    std::string input;
    while(receiveLine(socket, input)){
        Statement statement;
        const auto prepared = prepareStatement(input, statement);
        if(prepared == PrepareResult::UnrecognizedStatement){
            sendResponse(socket, -1, "Unrecognized SQL statement.\n");
            continue;
        }
        if(prepared == PrepareResult::SyntaxError){
            sendResponse(socket, -2, "Syntax error.\n");
            continue;
        }
        std::ostringstream output;
        ExecuteResult result = ExecuteResult::InvalidOperation;
        {
            std::lock_guard lock(state.mutex);
            if(state.transactionOwner && *state.transactionOwner != clientId){
                sendResponse(socket, -3, "Another client owns the active transaction.\n");
                continue;
            }
            auto* previousBuffer = std::cout.rdbuf(output.rdbuf());
            try{
                result = state.database.execute(statement);
            }catch(const std::exception& error){
                output << "Database error: " << error.what() << '\n';
                result = ExecuteResult::InvalidOperation;
            }
            std::cout.rdbuf(previousBuffer);
            if(statement.type == StatementType::Begin && result == ExecuteResult::Success) state.transactionOwner = clientId;
            if((statement.type == StatementType::Commit || statement.type == StatementType::Rollback) && result == ExecuteResult::Success) state.transactionOwner.reset();
        }
        auto payload = output.str();
        if(result != ExecuteResult::Success) payload += "Error: " + errorText(result) + "\n";
        sendResponse(socket, static_cast<int>(result), payload);
    }
    {
        std::lock_guard lock(state.mutex);
        if(state.transactionOwner && *state.transactionOwner == clientId){
            Statement rollback;
            rollback.type = StatementType::Rollback;
            try{ state.database.execute(rollback); }catch(...){ }
            state.transactionOwner.reset();
        }
    }
    closeSocket(socket);
}
}

int main(int argc, char** argv){
    const unsigned short port = argc > 1 ? static_cast<unsigned short>(std::strtoul(argv[1], nullptr, 10)) : 9001;
    const std::string filename = argc > 2 ? argv[2] : "data/litedb.db";
    if(!initializeSockets()){
        std::cerr << "Unable to initialize sockets.\n";
        return 1;
    }
    ServerState state(filename);
    const auto listener = createListeningSocket(port);
    if(listener == InvalidSocket){
        std::cerr << "Unable to listen on port " << port << ".\n";
        shutdownSockets();
        return 1;
    }
    std::cout << "litedb server listening on port " << port << '\n';
    std::atomic<std::uint64_t> nextClient{1};
    while(true){
        sockaddr_storage address{};
#ifdef _WIN32
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        const auto client = accept(listener, reinterpret_cast<sockaddr*>(&address), &length);
        if(client == InvalidSocket) continue;
        std::thread(serveClient, client, nextClient.fetch_add(1), std::ref(state)).detach();
    }
}
