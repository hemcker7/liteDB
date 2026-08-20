#include "InputBuffer.h"
#include <iostream>

void InputBuffer::printPrompt() const{
    std::cout<<"db > ";
}

bool InputBuffer::readInput(){
    return static_cast<bool>(std::getline(std::cin, input_));
}

const std::string& InputBuffer::getInput() const{
    return input_;
}
