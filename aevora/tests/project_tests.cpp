#include "test.hpp"
#include "engine/project/project.hpp"

#include <fstream>

namespace {
    // An empty folder under the system's temporary folder, removed when the test ends
    struct TemporaryFolder {
        std::filesystem::path path;

        explicit TemporaryFolder(const char* name) {
            path = std::filesystem::temp_directory_path() / "aevora_tests" / name;
            std::filesystem::remove_all(path);
            std::filesystem::create_directories(path);
        }

        ~TemporaryFolder() {
            std::error_code code;
            std::filesystem::remove_all(path, code);
        }
    };
}

TEST_CASE(project_text_round_trips) {
    Project project;
    project.name = "Skies of \"Aevora\" \\ test";

    const std::string text = ProjectFile::write(project);
    CHECK(text.rfind("aevora project 1\n", 0) == 0);

    Project loaded;
    std::string error;
    CHECK(ProjectFile::read(text, loaded, error));
    CHECK(loaded.name == project.name);
    CHECK(ProjectFile::write(loaded) == text);
}

TEST_CASE(project_text_skips_what_it_does_not_know) {
    Project loaded;
    std::string error;
    // Comments, blank lines, Windows line ends, and a key from some later version
    CHECK(ProjectFile::read("aevora project 1\r\n# a note\r\n\r\nname = \"Game\"\r\nfuture = \"thing\"\r\n", loaded, error));
    CHECK(loaded.name == "Game");
}

TEST_CASE(project_text_refuses_bad_files) {
    Project loaded;
    loaded.name = "untouched";
    std::string error;

    CHECK(!ProjectFile::read("", loaded, error));
    CHECK(error == "not an Aevora Engine project");
    CHECK(!ProjectFile::read("valuma project 1\nname = \"x\"\n", loaded, error));
    CHECK(error == "not an Aevora Engine project");
    CHECK(!ProjectFile::read("aevora project one\nname = \"x\"\n", loaded, error));
    CHECK(error == "not an Aevora Engine project");

    CHECK(!ProjectFile::read("aevora project 2\nname = \"x\"\n", loaded, error));
    CHECK(error == "made by a newer version of Aevora Engine (format 2)");

    CHECK(!ProjectFile::read("aevora project 1\n", loaded, error));
    CHECK(error == "the project has no name");
    CHECK(!ProjectFile::read("aevora project 1\nname = \"\"\n", loaded, error));
    CHECK(error == "the project has no name");
    CHECK(!ProjectFile::read("aevora project 1\nname = Game\n", loaded, error));
    CHECK(error == "line 2: the name must be in quotes");
    CHECK(!ProjectFile::read("aevora project 1\nname = \"a\"b\"\n", loaded, error));
    CHECK(!ProjectFile::read("aevora project 1\nname = \"ends in a slash\\\"\n", loaded, error));
    CHECK(!ProjectFile::read("aevora project 1\njust words\n", loaded, error));
    CHECK(error == "line 2 has no '='");

    // A refused file leaves the project alone
    CHECK(loaded.name == "untouched");
}

TEST_CASE(project_create_makes_the_folders_and_file) {
    TemporaryFolder temporary("create");
    const std::filesystem::path folder = temporary.path / "My Game";

    Project project;
    std::string error;
    CHECK(ProjectFile::create(folder, project, error));
    CHECK(project.name == "My Game");
    CHECK(project.file == folder / "My Game.aev");
    CHECK(project.folder() == folder);
    CHECK(std::filesystem::is_regular_file(project.file));
    for (const char* sub : ProjectFile::FOLDERS) CHECK(std::filesystem::is_directory(folder / sub));
    CHECK(ProjectFile::find(folder) == project.file);

    // No temporary file is left behind, and the file loads back
    CHECK(!std::filesystem::exists(folder / "My Game.aev.tmp"));
    Project loaded;
    CHECK(ProjectFile::load(project.file, loaded, error));
    CHECK(loaded.name == "My Game");
    CHECK(loaded.file == project.file);

    // The same folder can't be made into a project twice
    Project again;
    CHECK(!ProjectFile::create(folder, again, error));
    CHECK(error.find("already holds a project") != std::string::npos);
    CHECK(again.name.empty());
}

TEST_CASE(project_create_keeps_what_is_already_in_the_folder) {
    TemporaryFolder temporary("existing");
    const std::filesystem::path folder = temporary.path / "Started";
    std::filesystem::create_directories(folder / "assets");
    { std::ofstream(folder / "assets" / "rock.vlmobj") << "data"; }

    // A trailing separator names the same folder
    Project project;
    std::string error;
    CHECK(ProjectFile::create(folder / "", project, error));
    CHECK(project.name == "Started");
    CHECK(std::filesystem::exists(folder / "assets" / "rock.vlmobj"));
}

TEST_CASE(project_save_and_load_follow_the_file) {
    TemporaryFolder temporary("save");
    Project project;
    std::string error;
    CHECK(ProjectFile::create(temporary.path / "Saved", project, error));

    project.name = "Renamed";
    CHECK(ProjectFile::save(project, error));
    Project loaded;
    CHECK(ProjectFile::load(project.file, loaded, error));
    CHECK(loaded.name == "Renamed");

    CHECK(!ProjectFile::load(temporary.path / "missing.aev", loaded, error));
    CHECK(error.find("couldn't open") != std::string::npos);
    CHECK(ProjectFile::find(temporary.path / "nowhere").empty());
}
