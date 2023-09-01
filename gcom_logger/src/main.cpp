
#include "logger/logger.h"

//g++ -o logger -I include src/logger.cpp src/main.cpp -lspdlog  -lcjson

int main(int argc, char *argv[])
{
    //signal(SIGTERM, signal_handler);
    //signal(SIGINT, signal_handler);

    Logging::Init("dnp3_client", argc, argv);
    FPS_INFO_LOG("DNP3 Client Setup complete: Entering main loop.");
    return 0;
}