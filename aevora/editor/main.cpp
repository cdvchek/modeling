#include "editor/editor.hpp"

int main(int argumentCount, char** arguments) {
    EditorContext ctx;
    if (!Editor::initialize(ctx)) return 1;

    // A project file given on the command line opens at startup
    if (argumentCount > 1) Editor::openProject(ctx, std::filesystem::path(arguments[1]));

    Editor::run(ctx);
    Editor::shutdown(ctx);
    return 0;
}
