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

    void execute(const std::string& command);

private:
    std::unordered_map<std::string, Command> commands;
};