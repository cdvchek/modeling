#include "core/console/command_system.hpp"
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

void CommandSystem::execute(const std::string& command) {
    std::istringstream stream(command);

    std::string commandName;
    stream >> commandName;

    if (commandName.empty()) return;

    CommandArgs args;

    std::string arg;
    while (stream >> arg) {
        args.push_back(arg);
    }

    auto it = commands.find(commandName);

    if (it == commands.end()) {
        // print "unknown command"
        return;
    }

    it->second.callback(args);
}