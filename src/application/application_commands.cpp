#include "application/application.hpp"

#include <iostream>

void Application::registerCommands(AppContext& ctx) {
    ctx.systems.commands.registerCommand(
        "test",
        "This is a test command to make sure everything works",
        [&ctx](const CommandArgs& args) {
            std::cout << "Testing" << std::endl;
        }
    );
}