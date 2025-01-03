#include <iostream>
#include <csignal>
#include "Server.hpp"

Server* globalServerPointer = NULL;

void signalHandlerWrapper(int signal)
{
    if (signal == SIGINT && globalServerPointer != NULL)
        globalServerPointer->stop();
}

int main(int argc, char* argv[])
{
    const char* configPath;

    if (argc != 2)
        configPath = "Configs/basic.conf";
    else
        configPath = argv[1];
    try
    {
        Server server(configPath);
        globalServerPointer = &server;

        std::signal(SIGINT, signalHandlerWrapper);

        server.run();
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unhandled unknown error occurred!" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
