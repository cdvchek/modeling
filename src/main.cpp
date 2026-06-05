#include "application/application.hpp"

int main() {
    AppContext app_ctx;
    Application::initialize(app_ctx);
    Application::run(app_ctx);

    return 0;
}