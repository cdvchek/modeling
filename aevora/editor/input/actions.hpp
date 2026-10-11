#pragma once

#include <types>

enum class Action : u8 {
    Quit,
    ToggleConsole,
    EnterCommand,
    ConsoleBackspace,
    ConsoleDelete,
    ConsoleCursorLeft,
    ConsoleCursorRight,
    ConsoleHistoryOlder,
    ConsoleHistoryNewer,
    NewProject,
    OpenProject,
    Count
};
