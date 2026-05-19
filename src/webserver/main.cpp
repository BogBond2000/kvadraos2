#include "WebServer.h"

int main() {
    WebServer server("./webui", 8080);
    server.run();
    return 0;
}