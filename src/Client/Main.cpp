#include "Pch.h"
#include "Service.h"
#include "Config.h"

using namespace Client;

int main() 
{
    try
    {
        size_t numConnects = 0;

        std::cout << "Enter the number of connects: ";
        std::cin >> numConnects;

        const ServiceBase::ThreadPoolGroup::Info info =
        {
            Config::numSocketThreads,
            Config::numSessionThreads,
            Config::numMessageThreads,
            Config::numTaskThreads,
        };

        Service service(info);
        
        service.Start(Config::host, Config::service, numConnects);
        service.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
