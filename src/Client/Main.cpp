#include <Client/Pch.h>
#include <Client/Service.h>
#include <Client/Config.h>

using namespace Client;

int main() 
{
    try
    {
        size_t nConnects = 0;

        std::cout << "Enter the number of connects: ";
        std::cin >> nConnects;

        const ServiceBase::Threads::Info threadsInfo =
        {
            Config::nSocketThreads,
            Config::nSessionThreads,
            Config::nMessageThreads,
            Config::nTaskThreads,
        };

        Service service(threadsInfo);
        
        service.Start(Config::host, Config::service, nConnects);
        service.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
