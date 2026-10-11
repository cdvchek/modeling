# Projects

[project.hpp](../engine/project/project.hpp), [project.cpp](../engine/project/project.cpp)

A project is a folder. It holds the project file, named after the folder with the extension `.aev`, and the folders `scenes/`, `data/`, and `assets/`. `Project` is its name and the path of its file; `folder()` is the file's folder.

The project file is text:

```
aevora project 1
name = "My Game"
```

The first line names the format and the version that wrote it. After it comes one `key = "value"` per line; inside the quotes, `\"` and `\\` stand for a quote and a backslash. Blank lines, lines starting with `#`, and keys this version doesn't know are skipped. `name` is the only key so far, and it's required.

Everything is in `namespace ProjectFile`:

| Function | Description |
|---|---|
| `write(project)` | The file's text. |
| `read(text, project, error)` | Fills the name. False, with the reason in `error` and the project untouched, for text that isn't a project file ("not an Aevora Engine project"), is from a newer version, has no name, or has a line it can't read (the line number is given). |
| `save(project, error)` | Writes the text to `name.aev.tmp`, then renames it over the file, so a failed save never leaves half a file. |
| `load(file, project, error)` | Reads a file and sets `project.file` to its full path. |
| `create(folder, project, error)` | Makes the folder if it isn't there, the three project folders, and a project file named after the folder. What's already in the folder is kept. Refuses a folder that already holds a project. |
| `find(folder)` | The `.aev` file in a folder, or an empty path. |

## Tests

[project_tests.cpp](../tests/project_tests.cpp); what it covers is in [testing.md](testing.md).
