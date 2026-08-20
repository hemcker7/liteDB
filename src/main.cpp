#include "InputBuffer.h"
#include "MetaCommand.h"
#include "Statement.h"
#include "Executor.h"
#include "Database.h"
#include  "Serialization.hpp"
#include <iostream>

int main(){
    InputBuffer inputBuffer;
    MetaCommandHandler metaCommandHandler;
    Database database;
    
    while(true){
        inputBuffer.printPrompt();
        if(!inputBuffer.readInput()){
            std::cout<<"Input Read Error!\n";
            break;
        }
        const std::string& input = inputBuffer.getInput();
        if(!input.empty() && input[0]=='.'){
            switch(metaCommandHandler.execute(input)){
                case MetaCommandResult::Exit :
                    return 0;
                case MetaCommandResult::Success :
                    continue;
                case MetaCommandResult::Unrecognized :
                    std::cout<<"Unrecognized MetaCommand : "<<input<<"\n";
                    continue;
            }
        }
        
        Statement statement;
        PrepareResult result =  prepareStatement(input, statement);
        if(result == PrepareResult::UnrecognizedStatement){
            std::cout<<"Unrecognized SQL statement.\n";
            continue;
        }
        else if(result == PrepareResult::Success){
            const auto executeResult = database.execute(statement);
            if(executeResult == ExecuteResult::DuplicateKey) std::cout << "Error: duplicate id.\n";
            else if(executeResult == ExecuteResult::TableNotFound) std::cout << "Error: table not found.\n";
            else if(executeResult == ExecuteResult::InvalidOperation) std::cout << "Error: operation requires a matching id.\n";
            else if(executeResult == ExecuteResult::TransactionAlreadyActive) std::cout << "Error: transaction already active.\n";
            else if(executeResult == ExecuteResult::NoActiveTransaction) std::cout << "Error: no active transaction.\n";

        }
        else if(result == PrepareResult::SyntaxError){
            std::cout<<"Syntax Error!\n";
        }
    }
    return 0;
}