#include <Client/Pch.hpp>
#include <Client/Service.hpp>

int main() 
{
    size_t nIoPool = 4;
    size_t nControlPool = 2;
    size_t nHandlerPool = 4;
    size_t nTimerPool = 2;

    try
    {
        Client::Service service(nIoPool,
                                nControlPool,
                                nHandlerPool,
                                nTimerPool,
                                1000);
        
        service.Start("127.0.0.1", "60000");
        service.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
