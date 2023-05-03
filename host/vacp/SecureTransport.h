#include <set>
#include <cstdlib>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/chrono.hpp>

using namespace boost::chrono;

#define MAX_LOGIN_MESSAGE_SIZE 2048
#define CLIENT_SOCKET_TIMEOUT   30000 //in ms

enum { LOGIN_REQUEST, LOGIN_REPLY , SERVER_MSG, CLIENT_MSG};

/// \brief maximun size of read buffer
enum { max_length = 2048 };

class Connection;

/// \brief tcp connection for each connected client
class Connection : public boost::enable_shared_from_this<Connection>
{
public:
    Connection(boost::asio::io_service &io_service,
            boost::asio::ssl::context &context,
            boost::function<void(const std::string&,const char *, size_t)> req_handler,
            boost::function<bool(const std::string&, const std::string&)> auth_handler,
            boost::function<bool(const std::string&, std::string&)> connection_handler);

    ~Connection();

    /// \brief starts processing the handshake
    void start();
    
    /// \brief closes the connection
    void stop();

    /// \brief returns true if client login is success
    bool isloggedin();
    
    /// \brief returns client id the connection opened with
    std::string get_client_id();

    /// \brief does ssl handshake
    void handle_handshake(const boost::system::error_code& error);

    /// \brief handler for async read
    void handle_read(const boost::system::error_code& error,
                        size_t bytes_transferred);

    /// \brief synchronously writes buffer to the socket
    size_t write(const char *data, size_t length);

    boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::lowest_layer_type& socket()
    {
            return m_socket.lowest_layer();
    }

    steady_clock::time_point get_connect_time()
    {
        return m_connect_time;
    }

    void set_connect_time()
    {
        m_connect_time = steady_clock::now();
    }

private:

    bool m_isloggedin; //< indicate if the client is logged in

    std::string m_clientid; //< holds the connected client id

    std::string m_recvd_data; //<holds the received data from client

    std::string m_connection_type; // connection type like app, crash, sched

    steady_clock::time_point  m_connect_time; // time at which connection was made

    char m_recv_buf[max_length];  //< holds the received read buffer

    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_socket;

    boost::asio::io_service::strand m_strand; //< strand to not run cuncurrent request hanlders

    /// \brief callback to handle the client data 
    boost::function<void(const std::string&, const char *, size_t)> req_handler;

    /// \brief callback which authenticates if a client can connect
    boost::function<bool(const std::string&, const std::string&)> auth_handler;

    /// \brief callback to update the connections at the server level
    boost::function<bool(const std::string&, std::string&)> connection_handler;

    /// \brief  shows the ssl cipher used in this connection
    std::string get_current_cipher_suite();

    /// \brief asynchronously read some bytes from the socket
    void async_read_some();

    /// \brief process the received data
    void process(std::string &msg_data);

    /// \brief process the login requests 
    void process_login_req(std::string &msg_data);

    /// \brief reply to authenticated login requests 
    void reply_login(const std::string &id1, const std::string &id2, const std::string& id3, const std::string& id4);
};

// shared pointer type for holding connection object
typedef boost::shared_ptr<Connection> connection_ptr;

/// \brief ssl server that accepts connections from clients
class SecureServer
{
public:

    SecureServer(unsigned short port,
            std::string const& certFile,
            std::string const& keyFile,
            std::string const& dhFile,
            boost::function<void(const std::string&, const char *, size_t)> req_handler);

    /// \brief  starts the server 
    void run(int threadCount);

    /// \brief stops the server
    void close();

    /// \brief stops accepting new connections
    void stop_accepting_connections();

    /// \brief stops accepting new connections for a given connection type
    void stop_accepting_connections(const std::string& connection_type);

    /// \brief disconnect client
    void disconnect_client(const std::string &client_id, const std::string &connection_type);

    std::string get_password() const;

    /// \brief handler for accepting new connections asynchronously 
    void handle_accept(connection_ptr new_connection,
                       const boost::system::error_code& error);

    /// \brief send a request to client 
    size_t send(const std::string& client_id, 
                const char *data,
                const size_t length,
                const std::string& connection_type);

    /// \brief set the allowed clients 
    void set_allowed_clients(std::set<std::string>& clients);

    /// \brief check if a client is authorized to connect
    bool is_allowed_client(const std::string& client, const std::string& connection_type);

    /// \brief check if a client has a loggedin connection
    bool is_client_loggedin(const std::string& client, const std::string& connection_type);

    /// \brief start new connections based on connection type 
    bool open_connections(const std::string& connection_type);

    /// \brief close the connection based on connection type 
    void close_connections(const std::string& connection_type);

    /// \brief insert the connection into set of connections based on connection type 
    bool update_connections(const std::string& client_id, std::string& connection_type);

    /// \brief checks if new connections are accepted for a given type
    bool are_connections_accepted(const std::string& connection_type);

private:
    /// \brief handler for stopping the server asynchronously
    void handle_stop_server();

    /// \brief handler for stop accepting new connections asynchronously
    void handle_stop_accepting_connections();

    boost::asio::io_service m_io_service;

    boost::asio::ip::tcp::acceptor m_acceptor;

    boost::asio::ssl::context m_context; //< ssl context
    
    //< a map of connection type and set of connections that are accepted for that connection type
    std::map<std::string, std::set<connection_ptr> > m_connections;

    std::set<std::string> m_allowed_clients; //< set of allowed clients

    boost::function<void(const std::string&, const char *, size_t)> req_handler; //< callback to process the requests

    boost::mutex m_mutex; //< to protect m_connections

    //< a map of accepted connection types
    std::map<std::string, bool> m_accepted_connection_types;
};

/// \brief ssl client
class SecureClient 
{
public:
    SecureClient(std::string &client_id,
                boost::function<void(const char *, size_t)> callback);

    /// \brief connect to a server port
    boost::system::error_code connect(std::string &server,
                                    std::string &port);

    /// \brief close the connection to server
    void close();

    /// \brief  starts the io_server 
    void run(int threadCount);

    /// \brief sends a login request to the server
    void login(const std::string& connection_type);

    /// \brief reas some bytes from socket asynchronously
    void async_read_some();

    /// \brief handler for asynch read 
    void handle_read(const boost::system::error_code& error,
                    size_t bytes_transferred);

    /// \brief send request on socket synchronously
    size_t send(const std::string& dest_id,
                const char *data,
                size_t length,
                const std::string& connection_type);

    /// \brief process the received data
    void process(std::string &msg_data);

    /// \brief process the login reply 
    void process_login_reply(std::string &msg_data);

    /// \brief verify server certificate with fingerprint received in command areguments
    bool cert_verify_callback(bool preverified, boost::asio::ssl::verify_context& ctx);
private:

    /// \brief handler for stopping asynchronously
    void handle_stop();

    boost::asio::io_service m_io_service;

    boost::asio::ssl::context m_context; //< ssl context

    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_socket;

    boost::mutex m_mutex; //< to protect m_isloggedin

    bool m_isloggedin; //< indicates if the client logged in

    std::string m_server_id; //< holds the server id
    std::string m_client_id; //< holds the client id

    char m_recv_buf[max_length]; //< holds the asynchronously read data

    std::string m_recvd_data; //< holds the received data

    boost::function<void(const char *, size_t)> req_handler; //< handler to process the input requests

    /// \brief connect to a server endpoint
    boost::system::error_code connect(boost::asio::ip::tcp::endpoint& endpoint);
};
