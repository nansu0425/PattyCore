#include <Server/Pch.hpp>
#include <Server/Service.hpp>

int main()
{
    try
    {
        PattyCore::Server::Service service(4, 0, 60000);
        service.Start();

        service.JoinWorkers();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
