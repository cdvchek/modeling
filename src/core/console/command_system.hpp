#pragma once

#include <types>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

using CommandArgs = std::vector<std::string>;
using CommandCallback = std::function<void(const CommandArgs&)>;

struct Command {
    std::string description;
    CommandCallback callback;
};

class CommandSystem {
public:
    void registerCommand(
        const std::string& name,
        const std::string& description,
        CommandCallback callback
    );

    // False only for a command name nobody registered (an empty line is fine)
    bool execute(const std::string& command);

    // Every command with its description, sorted by name
    std::vector<std::pair<std::string, std::string>> list() const;

    // The first word of a command line
    static std::string commandName(const std::string& command);

private:
    std::unordered_map<std::string, Command> commands;
};