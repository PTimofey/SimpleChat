#include<boost/asio.hpp>
#include<iostream>
#include<string>
#include<thread>
#include<deque>
#include<memory>
#include<mutex>
#include<vector>



// MessageHead contains size of message itself
class MessageHead
{
public:
    uint32_t sizeBody;
    uint32_t sizeName;
};


// Message contains body, name of user. 
class Message 
{
public:

    MessageHead head;

    // Separate name and body and write in Message
    friend Message& operator << (Message& msg, const std::string& str)
    {
        msg.name.assign(str.begin(), str.begin()+str.find(':')+1);
        msg.body.assign(str.begin(),str.end());



        msg.head.sizeBody=msg.body.size();
        msg.head.sizeName=msg.name.size();
        return msg;
    }

    // Write date of Message in vector
    friend Message& operator >> (Message& msg, std::vector<char>&buff)
    {
        buff.assign(msg.name.begin(), msg.name.end());
        buff.insert(buff.end(), msg.body.begin(), msg.body.end());
        return msg;
    }

    // Write Message in ostream
    friend std::ostream& operator <<(std::ostream &os, const Message& msg)
    {
        std::string str;
        str.append(msg.name.begin(), msg.name.end());
        str.append(msg.body.begin(), msg.body.end());
        os<<str;
        return os;
    }



public:
    std::vector<char>body;
    std::vector<char>name;
};



// Owned message is message of connection. It contains connection's ID and message itself
struct owned_message
{
    uint16_t ID;
    Message msg;


    // Write owned message is ostream
    friend std::ostream& operator << (std::ostream &os, owned_message &om)
    {
        os<<om.msg;
        return os;
    }
};


// Queue
template<typename T>
class Line
{
public:
    Line()=default;
    Line(const Line<T>&)=delete;
    virtual ~Line(){ clear();}


    const T& front()
    {
        std::scoped_lock<std::mutex> lock(muxQueue);
        return deQueue.front();
    }


    const T& back()
    {
        std::scoped_lock<std::mutex> lock(muxQueue);
        return deQueue.back();
    }


    bool empty()
    {
        std::scoped_lock<std::mutex> lock(muxQueue);
        return deQueue.empty();
    }


    size_t size()
    {  
        std::scoped_lock<std::mutex> lock(muxQueue);
        return deQueue.size();
    }


    T pop_front()
    {
        std::scoped_lock<std::mutex> lock(muxQueue);
        auto str = std::move(deQueue.front());
        deQueue.pop_front();
        return str;
    }


    T pop_back()
    {
        std::scoped_lock<std::mutex> lock(muxQueue);
        auto str = std::move(deQueue.back());
        deQueue.pop_back();
        return str;
    }


    void push_back(const T& item)
    {
        std::scoped_lock<std::mutex> lock(muxQueue);
        deQueue.emplace_back(std::move(item));

        std::unique_lock<std::mutex> ul(muxBlocking);
        cv.notify_one();
    }


    void push_front(const T& item)
    {
        std::scoped_lock<std::mutex> lock(muxQueue);
        deQueue.emplace_front(std::move(item));

        std::unique_lock<std::mutex> ul(muxBlocking);
        cv.notify_one();
    }


    void wait()
    {
        while(empty())
        {
            std::unique_lock<std::mutex> ul(muxBlocking);
            cv.wait(ul);
        }
    }


    void clear()
    {
        std::scoped_lock<std::mutex> lock(muxQueue);
        deQueue.clear();
    }


protected:
    std::mutex muxQueue;
    std::mutex muxBlocking;
    std::deque<T> deQueue;
    std::condition_variable cv;
};


// Declare forward
class server_interface;

// Connection
class Connection : std::enable_shared_from_this<Connection>
{
public:

    // A connection is owned by either a server or a client, and
    // its behavior is slightly different between the two.
    enum class owner
    {
        Server,
        Client
    };

    Connection(owner parent, boost::asio::io_context &io, boost::asio::ip::tcp::socket socket,
    Line<owned_message>& qIn) : m_context(io), m_socket(std::move(socket)), IncomingMessages(qIn)
    {
        Otype=parent;
        if(Otype==owner::Server)
        {

        }
        else
        {

        }
    }

    virtual ~Connection(){}

    void ConnectToServer(boost::asio::ip::tcp::resolver::results_type& endpoint)
    {
        if(Otype==owner::Client)
        {
            boost::asio::async_connect(m_socket, endpoint,[this](std::error_code ec, boost::asio::ip::tcp::endpoint ep)
            {
                if(!ec)
                {
                    ReadHead();
                }
            });
        }
    }

    void ConnectToClient()
    {
        if(Otype==owner::Server)
        {
            if(m_socket.is_open())
            {
                ReadHead();
            }
        }
    }

    bool IsConnected() const
    {
        return m_socket.is_open();
    }

    void Disconnect()
    {
        if(IsConnected())
        {
            boost::asio::post(m_context,[this](){m_socket.close();});
        }
    }

    void Send(const Message &msg)
    {
        boost::asio::post(m_context,[this,msg]()
        {
            bool bWritingMassage=!MessagesForDespatch.empty();
            MessagesForDespatch.push_back(msg);
            if(!bWritingMassage)
            {
                WriteHeader();
            }
        });
    }

    uint16_t ID;
private:

    void WriteHeader()
    {
        boost::asio::async_write(m_socket, boost::asio::buffer(&MessagesForDespatch.front().head, sizeof(MessagesForDespatch.front().head)), 
        [this](std::error_code ec, std::size_t length){
            if(!ec)
            {
                if(MessagesForDespatch.front().body.size()>0)
                {
                    WriteBody();
                }
                else
                {
                    MessagesForDespatch.pop_front();
                    if(MessagesForDespatch.empty())
                    {
                        WriteHeader();
                    }
                }
            }
            else
            {
                std::cout<<"[] Write Header fail!!!\n";
                m_socket.close();
            }
        });
    }

    void WriteBody()
    {
        boost::asio::async_write(m_socket, boost::asio::buffer(MessagesForDespatch.front().body.data(), MessagesForDespatch.front().body.size()),
        [this](std::error_code ec, std::size_t length){
            MessagesForDespatch.pop_front();
            if(!ec)
            {
                if(!MessagesForDespatch.empty())
                {
                    WriteHeader();
                }
            }
            else
            {
                std::cout<<"[] Write Body fail!!!\n";
                m_socket.close();
            }
        });
    }

    void ReadHead()
    {
        boost::asio::async_read(m_socket, boost::asio::buffer(&TempMessage.head, sizeof(TempMessage.head)),
        [this](std::error_code ec, std::size_t length){
            if(!ec)
            {
                if(TempMessage.head.sizeName>0 && TempMessage.head.sizeBody>0)
                {
                    TempMessage.body.resize(TempMessage.head.sizeBody);
                    TempMessage.name.resize(TempMessage.head.sizeName);
                    std::cout<<"\nSize Name"<<TempMessage.head.sizeName<<" Size Body"<<TempMessage.head.sizeBody<<"\n";
                    ReadBody();
                }
                else
                {
                    AddToIncomingLine();
                }
            }
            else
            {
                std::cout<<"[] Read Head Fail!!!\n";
            }
        });
    }

    void ReadBody()
    {
        boost::asio::async_read(m_socket, boost::asio::buffer(TempMessage.body.data(), TempMessage.body.size()),
        [this](std::error_code ec, std::size_t length)
        {
            if(!ec)
            {
                AddToIncomingLine();
            }
            else
            {
                std::cout<<"[] Read Body Fail!!!\n";
            }
        });
    }

    void AddToIncomingLine()
    {
        if(Otype==owner::Server)
        {
            IncomingMessages.push_back({ID, TempMessage});
        }
        else
        {
            IncomingMessages.push_back({0, TempMessage});
        }
        ReadHead();
    }
protected:
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::io_context &m_context;

    Line<Message> MessagesForDespatch;
    Line<owned_message> &IncomingMessages;
    Message TempMessage;
    owner Otype;


};

class client_interface
{
public:
    client_interface()
    {}
    ~client_interface()
    {

    }

    bool Connect(std::string& host, const uint16_t port)
    {
        try
        {
            boost::asio::ip::tcp::resolver resolver(m_context);
            boost::asio::ip::tcp::resolver::results_type endpoints=resolver.resolve(host,std::to_string(port));

            m_connection=std::make_unique<Connection>(Connection::owner::Client, m_context, boost::asio::ip::tcp::socket(m_context), MessageIn);

            m_connection->ConnectToServer(endpoints);
            thrContext=std::thread([this](){m_context.run();});
        }
        catch(const std::exception& e)
        {
            std::cerr<<"Client Exception: " << e.what() << '\n';
        }
        return true;
    }

    bool IsConnected()
    {
        if(m_connection)
            return m_connection->IsConnected();
        else
        {
            return false;
        }
    }

    void Disconnect()
    {
        if(IsConnected())
        {
            m_connection->Disconnect();
        }

        m_context.stop();

        if(thrContext.joinable())
        {
            thrContext.join();
        }
        m_connection.release();
    }

    void Send(const Message& msg)
    {
        if(IsConnected())
        {
            m_connection->Send(msg);
        }
    }

    Line<owned_message> &Incoming()
    {
        return MessageIn;
    }



protected:
    boost::asio::io_context m_context;
    std::thread thrContext;
    std::unique_ptr<Connection> m_connection;
private:
    Line<owned_message> MessageIn;
};




class server_interface
{
public:
    server_interface(uint16_t port) : ContextAcceptor(context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    {
        uint16_t countID=0;
    }
    uint16_t countID;
    ~server_interface()
    {
        stop();
    } 

    void stop()
    {
        context.stop();
        if(contexThread.joinable())
        {
            contexThread.join();
        }
        std::cout<<"[SERVER] Stopped\n";
    }

    void WaitForClientConnection()
    {
        ContextAcceptor.async_accept([this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket)
        {
            if(!ec)
            {
                std::cout<<"[SERVER] New connection: "<<socket.remote_endpoint()<<'\n';

                std::shared_ptr<Connection> newconn = std::make_shared<Connection>(Connection::owner::Server, context, std::move(socket), IncomingMessages);
                countID++;
                newconn->ID=countID;

                deqConnections.push_back(newconn);
                deqConnections.back()->ConnectToClient();
            }
            else
            {
                std::cout<<"[SERVER] Connection denied\n";
            }
            WaitForClientConnection();
        });
        
    }

    bool start()
    {
        try
        {
            WaitForClientConnection();
            contexThread=std::thread([this](){context.run();});
        }
        catch(const std::exception& e)
        {
            std::cerr<<"[SERVER] Exception" << e.what() << '\n';
            return false;
        }
        return true;
    }


    void MessageClient(std::shared_ptr<Connection>&client, const Message& msg)
    {
        if(client->IsConnected())
        {
            client->Send(msg);
        }
        else
        {
            client.reset();
            deqConnections.erase(std::remove(deqConnections.begin(), deqConnections.end(), client),deqConnections.end());
        }
    }

    void MessageAllClient(uint16_t ID, const Message& msg)
    {
        for(auto& client : deqConnections)
        {
            if(client->IsConnected() && client)
            {
                if(client->ID!=ID)
                    {client->Send(msg);}
            }
        }
    }

    void Update(size_t MaxMsgs= -1, bool bwait=false)
    {
        if(bwait) 
            {IncomingMessages.wait();}

        size_t msgCount=0;
        while(msgCount < MaxMsgs && !IncomingMessages.empty())
        {
            
            auto msg = IncomingMessages.pop_front();
            
            OnMessage(msg.ID, msg.msg);
            msgCount++;
        }
    }
    
protected:

    virtual void OnMessage(uint16_t ID, Message &msg)
    {
        
    }

    Line<owned_message> IncomingMessages;
    std::deque<std::shared_ptr<Connection>> deqConnections;

    boost::asio::io_context context;
    boost::asio::ip::tcp::acceptor ContextAcceptor;
    
    std::thread contexThread;
};