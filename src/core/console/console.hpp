#pragma once

#include <string>
#include <vector>
#include <types>

#include "core/console/command_system.hpp"

class Console {
public:
    void enterCurrentCommand(CommandSystem& commands);
    const std::string& getCurrentCommand();
    const std::vector<std::string>& getHistory();
    void viewNewerCommand();
    void viewOlderCommand();
    void insertToCurrentCommand(char character);
    void removeFromCurrentCommandBack();
    void removeFromCurrentCommandForward();
    void moveCursorLeft();
    void moveCursorRight();
    void setCursor(u32 index);
private:
    u32 m_cursorIndex = 0;
    u32 m_commandIndex = 0;
    std::string m_command;
    std::vector<std::string> m_history;
};
