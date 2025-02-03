#include "Pch.h"
#include "Service.h"
#include "Config.h"

using namespace Server;

int main()
{
    try
    {
        const ServiceBase::ThreadPoolGroup::Info info =
        {
            Config::numSocketThreads,
            Config::numSessionThreads,
            Config::numMessageThreads,
            Config::numTaskThreads,
        };

        Service service(info, Config::port);
        
        service.Start();
        service.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
