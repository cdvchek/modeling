#include "test.hpp"
#include "core/console/console.hpp"

#include <iostream>

namespace {
    void type(Console& console, const std::string& text) {
        for (char c : text) console.insertToCurrentCommand(c);
    }
}

TEST_CASE(console_browsed_index_follows_up_and_down) {
    Console console;
    CommandSystem commands;

    for (const char* command : { "one", "two", "three" }) {
        type(console, command);
        console.enterCurrentCommand(commands);
    }
    CHECK(console.getBrowsedIndex() == -1);

    console.viewOlderCommand();
    CHECK(console.getBrowsedIndex() == 2);
    CHECK(console.getCurrentCommand() == "three");

    console.viewOlderCommand();
    console.viewOlderCommand();
    console.viewOlderCommand();
    CHECK(console.getBrowsedIndex() == 0);

    console.viewNewerCommand();
    CHECK(console.getBrowsedIndex() == 1);

    // Back past the newest is a fresh, empty command
    console.viewNewerCommand();
    console.viewNewerCommand();
    CHECK(console.getBrowsedIndex() == -1);
    CHECK(console.getCurrentCommand().empty());
}

TEST_CASE(console_editing_a_recalled_command_stops_browsing) {
    Console console;
    CommandSystem commands;
    type(console, "light list");
    console.enterCurrentCommand(commands);

    console.viewOlderCommand();
    CHECK(console.getBrowsedIndex() == 0);

    console.insertToCurrentCommand('s');
    CHECK(console.getBrowsedIndex() == -1);
    CHECK(console.getCurrentCommand() == "light lists");
}

TEST_CASE(console_output_becomes_an_entry_under_its_command) {
    Console console;
    CommandSystem commands;
    commands.registerCommand("list", "Prints two lines", [](const CommandArgs&) { std::cout << "first\nsecond\n"; });

    type(console, "list");
    console.enterCurrentCommand(commands);

    const std::vector<ConsoleEntry>& entries = console.getEntries();
    CHECK(entries.size() == 2);
    CHECK(entries[0].kind == ConsoleEntryKind::Command);
    CHECK(entries[0].text == "list");
    CHECK(entries[0].historyIndex == 0);
    CHECK(entries[1].kind == ConsoleEntryKind::Output);
    CHECK(entries[1].text == "first\nsecond");
    CHECK(entries[1].collapsible());
    CHECK(entries[1].expanded);
}

TEST_CASE(console_unknown_command_is_an_error) {
    Console console;
    CommandSystem commands;

    type(console, "nope 1 2");
    console.enterCurrentCommand(commands);

    const std::vector<ConsoleEntry>& entries = console.getEntries();
    CHECK(entries.size() == 2);
    CHECK(entries[1].kind == ConsoleEntryKind::Error);
    CHECK(entries[1].text.find("nope") != std::string::npos);
    CHECK(!entries[1].collapsible());
}

TEST_CASE(console_collapses_old_multiline_entries_only) {
    Console console;
    console.print("one line");
    console.print("two\nlines");
    console.printError("error\nwith detail");

    // Closing the console collapses what's there
    console.collapseEntries();
    CHECK(console.getEntries()[0].expanded);
    CHECK(!console.getEntries()[1].expanded);
    CHECK(!console.getEntries()[2].expanded);

    // Anything printed after that is new, so it stays open
    console.printError("new\nerror");
    CHECK(console.getEntries()[3].expanded);

    // Clicking toggles multi-line entries; one-line entries can't collapse
    console.toggleEntry(1);
    CHECK(console.getEntries()[1].expanded);
    console.toggleEntry(0);
    CHECK(console.getEntries()[0].expanded);
}

TEST_CASE(console_print_drops_trailing_breaks_and_empty_text) {
    Console console;
    console.print("done\n\n");
    console.print("\n");
    CHECK(console.getEntries().size() == 1);
    CHECK(console.getEntries()[0].text == "done");
}

TEST_CASE(command_system_lists_sorted_and_reports_unknown) {
    CommandSystem commands;
    commands.registerCommand("zoom", "Z", [](const CommandArgs&) {});
    commands.registerCommand("add", "A", [](const CommandArgs&) {});

    const auto list = commands.list();
    CHECK(list.size() == 2);
    CHECK(list[0].first == "add");
    CHECK(list[1].second == "Z");

    CHECK(commands.execute("add 1"));
    CHECK(commands.execute("   "));
    CHECK(!commands.execute("missing"));
    CHECK(CommandSystem::commandName("  light list") == "light");
}

TEST_CASE(console_tracks_the_latest_error) {
    Console console;
    CHECK(console.getErrorCount() == 0);

    console.print("fine");
    console.printError("first problem");
    console.printError("second problem\nwith detail");

    CHECK(console.getErrorCount() == 2);
    CHECK(console.getLatestError() == "second problem\nwith detail");
}
