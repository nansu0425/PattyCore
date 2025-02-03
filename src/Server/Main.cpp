#include <Server/Pch.h>
#include <Server/Service.h>
#include <Server/Config.h>

using namespace Server;

int main()
{
    try
    {
        const ServiceBase::Threads::Info threadsInfo =
        {
            Config::nSocketThreads,
            Config::nSessionThreads,
            Config::nMessageThreads,
            Config::nTaskThreads,
        };

        Service service(threadsInfo, Config::port);
        
        service.Start();
        service.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
