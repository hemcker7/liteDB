#pragma once
#include <string>

class InputBuffer{
public:
    void printPrompt() const;
    bool readInput();

    const std::string& getInput() const;

private:
    std::string input_;
};