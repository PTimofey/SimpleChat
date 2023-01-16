#include"ChatNetwork.hpp"

#define PORT 5477

class ChatClient : public client_interface
{
public:
    ChatClient(std::string name, std::string host, uint16_t port)
    {
        this->name="<"+name+">:";
        Connect(host,port);
    }

    void WriteSentence()
    {
        std::string sentence;
        std::cout<<name;
        std::cin>>sentence;

        if(sentence=="exit")
        {
            exit(1);
        }
        sentence=name+sentence;

        msg<<sentence;
        Send(msg);
        sentence.clear();
    }
private:
    std::string name;
    Message msg;
};

class Display
{
public:
    Display(ChatClient& client) : user(client)
    {}

    void OutputMessages()
    {
        bool loop=true;
        while(loop)
        {
            if(!user.Incoming().empty())
            {
                Inmsg=user.Incoming().pop_front();
                std::string str(Inmsg.msg.body.begin(), Inmsg.msg.body.end());
                std::cout<<str<<"\n";
            }
        }

    }

private:
    owned_message Inmsg;
    ChatClient &user;
};



int main(int argc, char *argv[])
{
    if(argc!=3)
{
    std::cout<<"there aren't 3 variable\n";
    return 0;
}

    ChatClient client(argv[1], argv[2], PORT);

    std::thread th([&client]()
    {
        while(true)
        {
            client.WriteSentence();
        }
    });

    Display display(client);

    display.OutputMessages();

    th.join();



}