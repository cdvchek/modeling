#include "core/console/console.hpp"

#include <iostream>
#include <sstream>

void Console::enterCurrentCommand(CommandSystem& commands) {
    if (!m_command.empty()) {
        m_history.push_back(m_command);
        addEntry(ConsoleEntryKind::Command, m_command);
        m_entries.back().historyIndex = static_cast<i32>(m_history.size()) - 1;
    }

    // Everything the command prints lands in one entry under it
    std::ostringstream captured;
    std::streambuf* original = std::cout.rdbuf(captured.rdbuf());
    m_echoTarget = original;
    const bool known = commands.execute(m_command);
    std::cout.rdbuf(original);
    m_echoTarget = nullptr;

    print(captured.str());
    if (!known) printError("unknown command: " + CommandSystem::commandName(m_command) + " (type help to list commands)");

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

void Console::print(const std::string& text) {
    addEntry(ConsoleEntryKind::Output, text);
}

void Console::printError(const std::string& text) {
    addEntry(ConsoleEntryKind::Error, text);
}

void Console::addEntry(ConsoleEntryKind kind, const std::string& text) {
    std::string trimmed = text;
    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r')) trimmed.pop_back();
    if (trimmed.empty()) return;

    if (m_echo) {
        std::ostream out(m_echoTarget ? m_echoTarget : std::cout.rdbuf());
        out << (kind == ConsoleEntryKind::Command ? "> " : "") << trimmed << std::endl;
    }
    m_entries.push_back({ kind, trimmed });

    if (kind == ConsoleEntryKind::Error) {
        ++m_errorCount;
        m_latestError = trimmed;
    }
}

void Console::toggleEntry(u32 index) {
    if (index < m_entries.size() && m_entries[index].collapsible()) m_entries[index].expanded = !m_entries[index].expanded;
}

void Console::collapseEntries() {
    for (ConsoleEntry& entry : m_entries) {
        if (entry.collapsible()) entry.expanded = false;
    }
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
