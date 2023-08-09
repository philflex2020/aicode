/* C Standard Library Dependencies */
#include <cstring>
#include <csignal>
/* C++ Standard Library Dependencies */
/* External Dependencies */
/* System Internal Dependencies */
#include <Logger.h>

bool running = true;
void signal_handler (int sig)
{
    running = false;
    FPS_WARNING_LOG("signal of type %d caught.\n", sig);
    signal(sig, SIG_DFL);
}

int main(int argc, char **argv)
{
    // Init Logger, passing optional command line argument for config file path
    Logging::Init("gcom_interface", argc, argv);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    FPS_INFO_LOG("Some Info  argc %d.\n", argc);
    FPS_WARNING_LOG("A Warning [%s].\n", argv[0]);
    FPS_ERROR_LOG("An Error.\n");
    return 0;
}
