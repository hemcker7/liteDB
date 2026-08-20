#pragma once
#include <string>

enum class MetaCommandResult{
    Success,
    Unrecognized,
    Exit
};

class MetaCommandHandler{
public:
    MetaCommandResult execute(const std::string& input) const;
};