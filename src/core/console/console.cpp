#include "core/console/console.hpp"

void Console::enterCurrentCommand() {
    // get command at command index
    const std::string& commandToSubmit = m_commandIndex == static_cast<u32>(m_history.size()) ? m_command : m_history[m_commandIndex];
    // check if command was valid
    // if valid
        // do the command
        // push command to history
    m_history.push_back(commandToSubmit);
        // set command index to size of history
    m_commandIndex = static_cast<u32>(m_history.size());
    // if not valid
        // display invalid message
    // clear the commmand
    m_command.clear();
}

const std::string& Console::getCurrentCommand() {
    if (m_commandIndex == static_cast<u32>(m_history.size())) return m_command;
    else return m_history[m_commandIndex];
}

void Console::viewNewerCommand() {
    if (m_commandIndex < static_cast<u32>(m_history.size())) m_commandIndex++;
}

void Console::viewOlderCommand() {
    if (m_commandIndex > 0) m_commandIndex--;
}

void Console::insertToCurrentCommand(char character) {
    m_command.insert(m_cursorIndex, 1, character);
    m_cursorIndex++;
}

void Console::removeFromCurrentCommandBack() {
    m_command.erase(m_cursorIndex - 1, 1);
    m_cursorIndex--;
}

void Console::removeFromCurrentCommandForward() {
    if (m_cursorIndex < static_cast<u32>(m_command.size())) m_command.erase(m_cursorIndex, 1);
}

void Console::moveCursorLeft() {
    if (m_cursorIndex > 0) m_cursorIndex--;
}

void Console::moveCursorRight() {
    if (m_cursorIndex < static_cast<u32>(m_command.size())) m_cursorIndex++;
}

void Console::setCursor(u32 index) {
    if (index <= static_cast<u32>(m_command.size())) m_cursorIndex = index;
}