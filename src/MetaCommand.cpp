#include "MetaCommand.h"

MetaCommandResult MetaCommandHandler::execute(const std::string& input) const{
    if(input == ".exit"){
        return MetaCommandResult::Exit;
    }
    return MetaCommandResult::Unrecognized;
}