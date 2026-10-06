#include "core/console/command_system.hpp"
#include <algorithm>
#include <sstream>

void CommandSystem::registerCommand(
    const std::string& name,
    const std::string& description,
    CommandCallback callback
) {
    commands.emplace(
        name,
        Command{
            description,
            std::move(callback)
        }
    );
}

bool CommandSystem::execute(const std::string& command) {
    std::istringstream stream(command);

    std::string commandName;
    stream >> commandName;

    if (commandName.empty()) return true;

    CommandArgs args;

    std::string arg;
    while (stream >> arg) {
        args.push_back(arg);
    }

    auto it = commands.find(commandName);

    if (it == commands.end()) {
        return false;
    }

    it->second.callback(args);
    return true;
}

std::vector<std::pair<std::string, std::string>> CommandSystem::list() const {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& [name, command] : commands) result.emplace_back(name, command.description);
    std::sort(result.begin(), result.end());
    return result;
}

std::string CommandSystem::commandName(const std::string& command) {
    std::istringstream stream(command);
    std::string name;
    stream >> name;
    return name;
}