#include <uWebSockets/App.h>
#include <iostream>

int main() {
    uWS::App()
        .get("/*", [](auto *res, auto *req) {
            res->end("Battery Server is live!");
        })
        .listen(9001, [](auto *listen_socket) {
            if (listen_socket) {
                std::cout << "✅ Listening on port 9001" << std::endl;
            }
        })
        .run();
}

