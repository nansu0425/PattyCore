#include <Server/Pch.hpp>
#include <Server/Service.hpp>
#include <Server/Config.hpp>

int main()
{
    try
    {
        Server::Service service(Server::Config::nIoHandlers,
                                Server::Config::nControllers,
                                Server::Config::nMessageHandlers,
                                Server::Config::nTimers,
                                Server::Config::port);
        
        service.Start();
        service.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
