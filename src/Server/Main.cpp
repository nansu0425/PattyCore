#include <Server/Pch.hpp>
#include <Server/Service.hpp>

int main()
{
    size_t nIoPool = 4;
    size_t nControlPool = 2;
    size_t nHandlerPool = 4;
    size_t nTimerPool = 2;

    try
    {
        Server::Service service(nIoPool,
                                nControlPool,
                                nHandlerPool,
                                nTimerPool,
                                60000);
        
        service.Start();
        service.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
