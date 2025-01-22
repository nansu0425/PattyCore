#include <Server/Pch.hpp>
#include <Server/Service.hpp>

int main()
{
    size_t nIo = 4;
    size_t nControl = 3;
    size_t nHandler = 4;
    size_t nTimer = 1;

    try
    {
        Server::Service service(nIo,
                                nControl,
                                nHandler,
                                nTimer,
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
