#pragma once

#include <streambuf>
#include <string>
#include <vector>
#include <types>

#include "core/console/command_system.hpp"

enum class ConsoleEntryKind : u8 {
    Command,
    Output,
    Error
};

// One item in the console's list: a command that was entered, or text something printed
struct ConsoleEntry {
    ConsoleEntryKind kind = ConsoleEntryKind::Output;
    std::string text;           // output and errors can span several lines
    bool expanded = true;
    i32 historyIndex = -1;      // commands: their place in the Up/Down history

    // Only output and errors longer than one line can be collapsed
    bool collapsible() const { return kind != ConsoleEntryKind::Command && text.find('\n') != std::string::npos; }
};

class Console {
public:
    // Runs the command; whatever it prints becomes one entry under it, and an unknown command becomes an error
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
    u32 getCursor() const { return m_cursorIndex; }

    // The history entry Up/Down brought back, or -1 when typing a new command (editing it counts as new)
    i32 getBrowsedIndex() const { return m_commandIndex < m_history.size() ? static_cast<i32>(m_commandIndex) : -1; }

    // Adds text as an entry (trailing line breaks dropped); with echo on, also writes it to stdout
    void print(const std::string& text);
    void printError(const std::string& text);
    void setEcho(bool echo) { m_echo = echo; }

    const std::vector<ConsoleEntry>& getEntries() const { return m_entries; }

    // Errors so far and the newest one, so the app can notice and show a new error elsewhere
    u32 getErrorCount() const { return m_errorCount; }
    const std::string& getLatestError() const { return m_latestError; }

    void toggleEntry(u32 index);

    // Called when the console closes, so everything already there opens collapsed next time
    void collapseEntries();

private:
    void addEntry(ConsoleEntryKind kind, const std::string& text);

    u32 m_cursorIndex = 0;
    u32 m_commandIndex = 0;
    std::string m_command;
    std::vector<std::string> m_history;
    std::vector<ConsoleEntry> m_entries;
    bool m_echo = false;
    // While a command runs std::cout is captured; echoes go here instead, so entries the command adds itself aren't captured twice
    std::streambuf* m_echoTarget = nullptr;
    u32 m_errorCount = 0;
    std::string m_latestError;
};
