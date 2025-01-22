#include <Client/Pch.hpp>
#include <Client/Service.hpp>

int main() 
{
    size_t nIo = 4;
    size_t nControl = 2;
    size_t nHandler = 4;
    size_t nTimer = 2;

    try
    {
        Client::Service service(nIo,
                                nControl,
                                nHandler,
                                nTimer);
        
        service.Start("127.0.0.1", "60000", 1000);
        service.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
