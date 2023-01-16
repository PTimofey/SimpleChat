#include"ChatNetwork.hpp"

#define PORT 5477



class Server : public server_interface
{
public:
    Server() : server_interface(PORT)
    {
        
    }
    ~Server()
    {

    }
    std::vector<char> str;
    void OnMessage(uint16_t ID, Message &msg) override
    {
        std::vector<char> buf;
        msg>>buf;
        std::cout<<msg;
        MessageAllClient(ID, msg);
    }
    
    
};


int main()
{
    std::cout<<getpid();
    Server serv;
    
    serv.start();

    
    while (1)
    {
        serv.Update(-1,true);
        
    }
    
    return 0;
}