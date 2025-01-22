#include <Client/Pch.hpp>
#include <Client/Service.hpp>
#include <Client/Config.hpp>

int main() 
{
    try
    {
        size_t nConnects = 0;

        std::cout << "Enter the number of connects: ";
        std::cin >> nConnects;

        Client::Service service(Client::Config::nIoHandlers,
                                Client::Config::nControllers,
                                Client::Config::nMessageHandlers,
                                Client::Config::nTimers);
        
        service.Start(Client::Config::host,
                      Client::Config::service,
                      nConnects);
        service.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
