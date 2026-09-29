#include "core/console/console.hpp"

void Console::enterCurrentCommand(CommandSystem& commands) {
    // check if command is valid
    // if valid:
        // execute command
        commands.execute(m_command);

    if (!m_command.empty()) m_history.push_back(m_command);

    m_command.clear();
    m_commandIndex = static_cast<u32>(m_history.size());
    m_cursorIndex = 0;
}

const std::string& Console::getCurrentCommand() {
    return m_command;
}

const std::vector<std::string>& Console::getHistory() {
    return m_history;
}

void Console::viewNewerCommand() {
    if (m_commandIndex < static_cast<u32>(m_history.size())) {
        m_commandIndex++;

        if (m_commandIndex == static_cast<u32>(m_history.size())) m_command.clear();
        else m_command = m_history[m_commandIndex];

        m_cursorIndex = static_cast<u32>(m_command.size());
    }
}

void Console::viewOlderCommand() {
    if (m_commandIndex > 0) {
        m_commandIndex--;
        m_command = m_history[m_commandIndex];
        m_cursorIndex = static_cast<u32>(m_command.size());
    }
}

void Console::insertToCurrentCommand(char character) {
    m_command.insert(m_cursorIndex, 1, character);
    m_cursorIndex++;

    m_commandIndex = static_cast<u32>(m_history.size());
}

void Console::removeFromCurrentCommandBack() {
    if (m_cursorIndex > 0) {
        m_command.erase(m_cursorIndex - 1, 1);
        m_cursorIndex--;

        m_commandIndex = static_cast<u32>(m_history.size());
    }
}

void Console::removeFromCurrentCommandForward() {
    if (m_cursorIndex < static_cast<u32>(m_command.size())) {
        m_command.erase(m_cursorIndex, 1);

        m_commandIndex = static_cast<u32>(m_history.size());
    }
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