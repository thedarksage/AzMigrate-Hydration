#include <stdafx.h>

#include <cstdlib>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>
#include <openssl/ssl.h>
#include <openssl/md5.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/regex.hpp>

#include "SecureTransport.h"
#include "securityutils.h"
#include "defaultdirs.h"
#include "getpassphrase.h"
#include "errorexception.h"
#include "setsockettimeoutsminor.h"
#include "gencert.h"

#include "RcmClientSettings.h"
#include "AADAuthProvider.h"
#include "proxysettings.h"

using namespace RcmClientLib;
using namespace AADAuthLib;

enum TOKEN_INDEX {
    TOKEN_INDEX_HDR = 0,
    TOKEN_INDEX_TYPE, 
    TOKEN_INDEX_MD5,
    TOKEN_INDEX_LEN,
    TOKEN_INDEX_DATA
};

#define VALID_NUM_DATA_TOKENS    6

enum LOGIN_INDEX {
    LOGIN_INDEX_NONCE = 0,
    LOGIN_INDEX_CLIENTID = 1, 
    LOGIN_INDEX_SERVERID = 2,
    LOGIN_INDEX_CONNECTION_TYPE = 3,
    LOGIN_INDEX_AAD_TOKEN = 4
};

#define VALID_NUM_LOGIN_TOKENS                  7
#define VALID_NUM_LOGIN_TOKENS_NO_AAD           6
#define VALID_NUM_LOGIN_TOKENS_COMPAT_MODE      5

enum AUTH_TOKEN_INDEX {
    AUTH_TOKEN_INDEX_HEADER = 0,
    AUTH_TOKEN_INDEX_DATA = 1,
    AUTH_TOKEN_INDEX_SIGNATURE = 2
};

#define AUTH_VALID_NUM_TOKENS   3

enum LOG_LEVEL {
    LL_ERROR = 0,
    LL_WARNING = 1,
    LL_INFO = 2,
    LL_DEBUG = 3
};

extern void inm_printf(const char * format, ...);
extern void inm_printf(short logLevl, const char* format, ... );

const std::string protocol_header = "VACPST";
const char *delimiter = ":";
const char *login_delimiter = "&";

const char* msgTypes[] = {"LoginRequest", 
                          "LoginReply", 
                          "ServerMessage", 
                          "ClientMessage"};

const std::string ConnectionTypeGeneric("Generic");

extern std::string g_strMasterNodeFingerprint;
extern std::string g_strRcmSettingsPath;
extern std::string g_strProxySettingsPath;

boost::shared_ptr<RcmClientSettings>    g_ptrRcmClientSettings;
boost::shared_ptr<ProxySettings>        g_ptrProxySettings;
boost::shared_ptr<AADAuthProvider >     g_ptrAADAuthProvider;

/// \brief generate a digest
std::string genDigest(std::string const& str)
{
    unsigned char digest[MD5_DIGEST_LENGTH];

    MD5((unsigned char const *)str.c_str(), str.length(), digest);

    std::stringstream digestStr;

    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        digestStr << std::hex << std::setw(2) << std::setfill('0') << (unsigned short)(digest[i]);
    }

    return digestStr.str();
}

/// \brief check if a received message type is valid
///
/// \returns
/// \li true on success
/// \li false on failure
bool isValidMsgType(std::string &type)
{
    std::vector<std::string> vmsgTypes(msgTypes, msgTypes + 4);
    return std::find(vmsgTypes.begin(), vmsgTypes.end(), type) != vmsgTypes.end();
}

/// \brief get the passphrase used to sign the message
std::string getPassphrase()
{
    if (g_strMasterNodeFingerprint.empty())
        return securitylib::getPassphrase();
    else
        return g_strMasterNodeFingerprint;
}

/// \brief verify if two ids are same
bool ids_equal(std::string const& lhsId, std::string const& rhsId)
{
    std::string passphrase = getPassphrase();
    return boost::algorithm::iequals(
                securitylib::genHmac(passphrase.c_str(), 
                                     passphrase.size(), 
                                     lhsId), 
                securitylib::genHmac(passphrase.c_str(), 
                                     passphrase.size(), 
                                     rhsId));
}

/// \brief builds an id with HMAC signature
std::string build_id(const std::string& id1,
    const std::string& id2,
    const std::string& id3,
    const std::string& id4,
    const std::string& nonce)
{
    std::string passphrase = getPassphrase();

    std::stringstream ss1;
    ss1 << nonce << login_delimiter << id1 << login_delimiter << id2 << login_delimiter;
    
    if (!id3.empty())
        ss1 << id3 << login_delimiter;

    if (!id4.empty())
        ss1 << id4 << login_delimiter;

    std::string sigHmac = securitylib::genHmac(passphrase.c_str(), passphrase.size(), ss1.str());

    std::stringstream ss2;
    ss2 << ss1.str() << sigHmac << login_delimiter;

    return ss2.str();
}

/// \brief verify if a given id is valid
/// 
/// \returns
/// \li true on success
/// \li false on failure
bool verify_id(const std::string& id1, 
               const std::string& id2, 
               const std::string& id3,
               const std::string& id4,
               const std::string& nonce, 
               const std::string& id_to_verify)
{
    return ids_equal(id_to_verify, build_id(id1, id2, id3, id4, nonce));
}

/// \brief verify if the AAD token that has valid data
///
/// \returns
/// \li true on success
/// \li false on failure
bool verify_client_auth_token(const std::string& authtoken)
{
    if (g_strMasterNodeFingerprint.empty())
    {
        // V-A multi node
        return true;
    }
    std::vector<std::string> authtoken_info;
    std::string authtokendata;
    boost::split(authtoken_info, authtoken, boost::is_any_of("."), boost::token_compress_on);

    if (authtoken_info.size() != AUTH_VALID_NUM_TOKENS)
    {
        inm_printf("verify_client_auth_token failed: invalid auth token\n");
        return false;
    }

    authtokendata = securitylib::base64Decode(authtoken_info[AUTH_TOKEN_INDEX_DATA].c_str(),
        authtoken_info[AUTH_TOKEN_INDEX_DATA].size());

    std::stringstream jsonstream(authtokendata);
    boost::property_tree::ptree proptree;
    try {
        boost::property_tree::read_json(jsonstream, proptree);
    }
    catch (boost::property_tree::json_parser_error& pe)
    {
        inm_printf("verify_client_auth_token failed: error reading josn stream %s\n", pe.what());
        return false;
    }
    catch (std::exception& e)
    {
        inm_printf("verify_client_auth_token failed: error reading josn stream error= %s\n", e.what());
        return false;
    }
    catch (...)
    {
        inm_printf("verify_client_auth_token failed: error reading josn stream\n");
        return false;
    }

    if (g_ptrRcmClientSettings.get() == NULL)
        g_ptrRcmClientSettings.reset(new RcmClientSettings(g_strRcmSettingsPath));

    if (g_ptrRcmClientSettings->m_AADTenantId.compare(proptree.get("tid", "")) &&
        g_ptrRcmClientSettings->m_AADClientId.compare(proptree.get("appid", "")) &&
        g_ptrRcmClientSettings->m_AADAudienceUri.compare(proptree.get("aud", "")))
    {
        inm_printf("verify_client_auth_token failed: token verification failed.\n");
        return false;
    }
    inm_printf("verify_client_auth_token: token verification successful.\n");

    return true;
}

/// \brief process the received message. 
/// if valid message is found then return the data in msg_req
/// and type in msg_type
/// 
/// \returns
/// \li true if any valid message is found
/// \li false on failure find valid message
bool process_message(std::string &msg_data,
                     std::string &msg_req,
                     std::string &msg_type)
{
    inm_printf(LL_DEBUG, "process_message() %s\n", msg_data.c_str());
    size_t index;

    if ((index = msg_data.find(protocol_header)) == msg_data.npos)
    {
        if (msg_data.length() > protocol_header.length())
            msg_data.clear();
        return false;
    }

    msg_data.erase(0, index);
    std::vector<std::string> msg_info;
    boost::split(msg_info, msg_data, boost::is_any_of(delimiter), boost::token_compress_on);

    if (msg_info[TOKEN_INDEX_HDR].compare(protocol_header) != 0)
    {
        msg_data.erase(0, msg_info[TOKEN_INDEX_HDR].length() + 1 /* for the delimiter */);
        return process_message(msg_data, msg_req, msg_type);
    }

    if (msg_info.size() < VALID_NUM_DATA_TOKENS)
        return false;

    std::stringstream smsgsize;
    smsgsize << msg_info[TOKEN_INDEX_LEN];
    size_t msgsize; 
    smsgsize >> msgsize;

    if (msgsize >  MAX_LOGIN_MESSAGE_SIZE)
    {
        msg_data.erase(0, protocol_header.length());
        return process_message(msg_data, msg_req, msg_type);
    }

    msg_req = msg_info[TOKEN_INDEX_DATA];

    if ((msgsize >  msg_req.length()) &&
        (msg_info.size() > VALID_NUM_DATA_TOKENS))
    {
        int tokenIndex = TOKEN_INDEX_DATA + 1;
        while(tokenIndex < msg_info.size())
        {
            msg_req += delimiter;
            msg_req += msg_info[tokenIndex];

            if (msg_req.length() >= msgsize) 
                break;
                
            tokenIndex++;
        }

        if ((msgsize >  msg_req.length()) &&
            (tokenIndex == msg_info.size()))
            return false;
    }

    size_t ldsize = msg_info[TOKEN_INDEX_HDR].length();
    ldsize += msg_info[TOKEN_INDEX_TYPE].length();
    ldsize += msg_info[TOKEN_INDEX_MD5].length();
    ldsize += msg_info[TOKEN_INDEX_LEN].length();
    ldsize += TOKEN_INDEX_DATA;  // for delimiters

    if ((!isValidMsgType(msg_info[TOKEN_INDEX_TYPE])) ||
        (msg_info[TOKEN_INDEX_MD5].compare(genDigest(msg_req)) != 0))
    {
        msg_data.erase(0, protocol_header.length());
        return process_message(msg_data, msg_req, msg_type);
    }

    msg_data.erase(0, ldsize);

    msg_type = msg_info[TOKEN_INDEX_TYPE];

    return true;
}

Connection::Connection(boost::asio::io_service &io_service,
                boost::asio::ssl::context &context,
                boost::function<void(const std::string&, const char *, size_t)> req_handler,
                boost::function<bool(const std::string&, const std::string&)> auth_handler,
                boost::function<bool(const std::string&, std::string&)> connection_handler)
    : m_socket(io_service, context),
    m_strand(io_service),
    req_handler(req_handler),
    auth_handler(auth_handler),
    connection_handler(connection_handler),
    m_isloggedin(false)
{
}

Connection::~Connection()
{
    stop();
}

/// \brief start accepting handshake with client
void Connection::start()
{
    if (!setSocketTimeoutOptions(m_socket.lowest_layer().native_handle(), CLIENT_SOCKET_TIMEOUT))
        setSocketTimeoutForAsyncRequests(m_socket.lowest_layer().native_handle(), CLIENT_SOCKET_TIMEOUT);

    m_socket.async_handshake(boost::asio::ssl::stream_base::server,
            boost::bind(&Connection::handle_handshake, shared_from_this(),
                      boost::asio::placeholders::error));
}

/// \brief closes the socket
void Connection::stop()
{
    m_isloggedin = false;
    boost::system::error_code error;
    m_socket.lowest_layer().close(error);
}

/// \brief return the cipher used by this connection
std::string Connection::get_current_cipher_suite()
{
    std::string sslCipherStr;
    SSL_CIPHER const* sslCipher = SSL_get_current_cipher(m_socket.native_handle());
    if (NULL != sslCipher) {
        sslCipherStr = "Cipher - name: ";
        sslCipherStr += SSL_CIPHER_get_name(sslCipher);
        int algoBits;
        int secretBits = SSL_CIPHER_get_bits(sslCipher, &algoBits);
        sslCipherStr += ", bits: ";
        sslCipherStr += boost::lexical_cast<std::string>(secretBits);
        sslCipherStr += ", ";
        sslCipherStr += boost::lexical_cast<std::string>(algoBits);
        sslCipherStr += ", version: ";
        const char* tmp = SSL_CIPHER_get_version(sslCipher);
        if (0 != tmp) {
            sslCipherStr += tmp;
        }
        else {
            sslCipherStr += "unknonw";
        }
        sslCipherStr += ", description: ";
        std::vector<char> buf(max_length, 0);
        tmp = SSL_CIPHER_description(sslCipher, &buf[0], buf.size());
        if (0 != buf[0]) {
            sslCipherStr += &buf[0];
        }
        else {
            sslCipherStr += "not found";
        }
    }
    return sslCipherStr;
}

/// \brief handler for ssl handshake
void Connection::handle_handshake(const boost::system::error_code& error)
{
    if (!error)
    {
        std::string cipher = get_current_cipher_suite();
        inm_printf(LL_INFO, "Current cipher : %s\n", cipher.c_str());

        async_read_some();
    }
    else
    {
        inm_printf("client handshake failed: %d %s\n", error.value(), error.message().c_str());
        stop();
    }
}


/// \brief send the login reply to client
///
/// \exception throws exception if write fails
///
void Connection::reply_login(const std::string &id1,
    const std::string &id2,
    const std::string &id3,
    const std::string &id4)
{
    std::string nonce = securitylib::genRandNonce(16);
    std::string login_id = build_id(id1, id2, id3, id4, nonce);

    std::stringstream ss1;
    ss1 << protocol_header << delimiter;
    ss1 << msgTypes[LOGIN_REPLY] << delimiter;
    ss1 << genDigest(login_id) << delimiter;
    ss1 << login_id.length() << delimiter;
    ss1 << login_id << delimiter;

    write(ss1.str().c_str(), ss1.str().length());

    inm_printf(LL_DEBUG, "reply_login: id %s nbytes %d\n", ss1.str().c_str(), ss1.str().length());

    return;
}

/// \brief return the client id associated with this connection
std::string Connection::get_client_id()
{
    return m_clientid;
}

/// \brief verifies if the client loggin was successful
bool Connection::isloggedin()
{
    return m_isloggedin;
}

/// \brief process the received data
void Connection::process(std::string &msg_data)
{
    std::string msg_type;
    std::string msg_req;

    if (!process_message(msg_data, msg_req, msg_type))
        return;

    if (msg_type.compare(msgTypes[LOGIN_REQUEST]) == 0)
    {
        process_login_req(msg_req);
    }
    else
    {
        try {
            if (m_isloggedin)
                req_handler(m_connection_type, msg_req.c_str(), msg_req.length());
            else
                inm_printf("Connection::process received data without login, dropped.\n");
        }
        catch (const std::exception &ex)
        {
            inm_printf("Failed to process received data with exception: %s.\n", ex.what());

            // disconnect the client that sent invalid data
            stop();
        }
    }

    msg_data.erase(0, msg_req.length());

    return;
}

/// \brief process the login request
///
/// if the login reques is not valid, closes the connection
///
void Connection::process_login_req(std::string &login_data)
{
    std::vector<std::string> login_info;
    boost::split(login_info, login_data, boost::is_any_of(login_delimiter), boost::token_compress_on);
    size_t pos = login_data.find_last_of(':');
    std::string client_id = login_data.substr(0, pos);
    
    std::string connection_type, aad_token;
    bool bvalidInput = true;

    if (login_info.size() == VALID_NUM_LOGIN_TOKENS)
    {
        connection_type = login_info[LOGIN_INDEX_CONNECTION_TYPE];
        aad_token = login_info[LOGIN_INDEX_AAD_TOKEN];

    }
    else if (g_strMasterNodeFingerprint.empty() && (login_info.size() == VALID_NUM_LOGIN_TOKENS_NO_AAD))
    {
        connection_type = login_info[LOGIN_INDEX_CONNECTION_TYPE];
        aad_token = std::string();
    }
    else if (login_info.size() == VALID_NUM_LOGIN_TOKENS_COMPAT_MODE)
    {
        // try compatibility mode
        connection_type = aad_token = std::string();
    }
    else
    {
        inm_printf("%s : invalid input\n", __FUNCTION__);
        bvalidInput = false;
    }

    if (!bvalidInput ||
        (!verify_id(login_info[LOGIN_INDEX_CLIENTID],
                    login_info[LOGIN_INDEX_SERVERID],
                    connection_type,
                    aad_token,
                    login_info[LOGIN_INDEX_NONCE],
                    client_id)) ||
         !verify_client_auth_token(aad_token))
    {
        std::stringstream errmsg ;
        errmsg << "invalid login data: " << login_data ;
        inm_printf("%s\n", errmsg.str().c_str());
        stop();
    }
    else
    {
        if (auth_handler(login_info[LOGIN_INDEX_CLIENTID], connection_type))
        {
            try {
                
                reply_login(login_info[LOGIN_INDEX_SERVERID],
                            login_info[LOGIN_INDEX_CLIENTID],
                            connection_type,
                            aad_token);

                m_isloggedin = true;
                m_clientid = login_info[LOGIN_INDEX_CLIENTID];

                if (!connection_handler(m_clientid, connection_type))
                    stop();

                m_connection_type = connection_type;

            }
            catch(std::exception& e)
            {
                inm_printf("reply_login failed with %s for %s.\n", 
                            e.what(),
                            login_info[LOGIN_INDEX_CLIENTID].c_str());
                stop();
            }
        }
        else
        {
            inm_printf("client %s not authorized\n", login_info[LOGIN_INDEX_CLIENTID].c_str());
            stop();
        }
    }

    return;
}

/// \brief handler for asynch read request
void Connection::handle_read(const boost::system::error_code& error,
                            size_t bytes_transferred)
{
    if (!error)
    {
        if (bytes_transferred) 
        {
            std::string msg_data;
            msg_data.assign(m_recv_buf, bytes_transferred);

            m_recvd_data += msg_data;

            async_read_some();

            process(m_recvd_data);

        }
        else
            inm_printf("Connection::handle_read received no data.\n");
    }
    else
    {
        inm_printf("Connection::handle_read failed with error %d (%s)\n",
            error.value(),
            error.message().c_str());

        stop();
    }
}

/// \brief read some bytes asnychronously
void Connection::async_read_some()
{
    m_socket.async_read_some(boost::asio::buffer(m_recv_buf, max_length),
        m_strand.wrap(boost::bind(&Connection::handle_read, shared_from_this(),
                                  boost::asio::placeholders::error,
                                  boost::asio::placeholders::bytes_transferred)));
}

/// \brief write length bytes
///
/// \returns
/// \li \c length bytes on success
///
/// \exception throws exception on failure
///
size_t Connection::write(const char *data, size_t length)
{
    if (!m_socket.lowest_layer().is_open())
    {
        m_isloggedin = false;
        inm_printf("%s: write failed as socket is not open for client %s of type %s.\n",
            __FUNCTION__,
            m_clientid.c_str(),
            m_connection_type.c_str());
        return 0;
    }

    return boost::asio::write(m_socket, boost::asio::buffer(data, length));
}

SecureServer::SecureServer(unsigned short port,
                        std::string const& certFile,
                        std::string const& keyFile,
                        std::string const& dhFile,
                        boost::function<void(const std::string&, const char *, size_t)> callback)
        : m_io_service(),
        req_handler(callback),
        m_acceptor(m_io_service,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
        m_context(boost::asio::ssl::context::tlsv12)
{
    m_context.set_options(
                boost::asio::ssl::context::default_workarounds
                | boost::asio::ssl::context::no_sslv2
                | boost::asio::ssl::context::no_sslv3
                | boost::asio::ssl::context::no_tlsv1
                | boost::asio::ssl::context::no_tlsv1_1
                | boost::asio::ssl::context::single_dh_use);

    m_context.set_password_callback(boost::bind(&SecureServer::get_password, this));
    m_context.use_certificate_chain_file(certFile);
    m_context.use_private_key_file(keyFile, boost::asio::ssl::context::pem);
    m_context.use_tmp_dh_file(dhFile);

    // ignore failure to set the ciphers
    SSL_CTX *sslCtx = m_context.native_handle();
    if (sslCtx != NULL)
    {
        if (!SSL_CTX_set_cipher_list(sslCtx, "HIGH:!ADH:!AECDH"))
        {
            inm_printf("%s: failed to set cipher list in SSL context.\n", __FUNCTION__);
        }
    }
    else
    {
        inm_printf("%s: failed to get native SSL handle.\n", __FUNCTION__);
    }

    connection_ptr new_connection;
    new_connection.reset(new Connection(m_io_service, 
                        m_context, 
                        req_handler,
                        boost::bind(&SecureServer::is_allowed_client, this, _1, _2),
                        boost::bind(&SecureServer::update_connections, this, _1, _2)));

    m_acceptor.async_accept(new_connection->socket(),
                boost::bind(&SecureServer::handle_accept, 
                            this, 
                            new_connection,
                            boost::asio::placeholders::error));

}

/// \brief starts the io service
///
/// this call blocks until the io service is stoped
void SecureServer::run(int threadCount)
{
    boost::thread_group thread_group;
    for (int i = 0; i < threadCount; i++)
    {
        boost::thread *thread = new boost::thread(boost::bind(&boost::asio::io_service::run, &m_io_service));
        thread_group.add_thread(thread);
    }

    thread_group.join_all();
}

/// \brief posts an async requet to stop the server
void SecureServer::close()
{
    m_io_service.post(boost::bind(&SecureServer::handle_stop_server, this));
}

/// \brief posts an async request to stop accepting new connections
void SecureServer::stop_accepting_connections()
{
    m_io_service.post(boost::bind(&SecureServer::handle_stop_accepting_connections, this));
}

/// \brief handler to stop the server async request
/// 
/// closes all the client connections
///
void SecureServer::handle_stop_server()
{
    m_io_service.stop();
}

/// \brief disconnects a client
void SecureServer::disconnect_client(const std::string &client_id,
    const std::string &connection_type)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);
    std::map<std::string, std::set<connection_ptr> >::iterator mapIter = m_connections.find(connection_type);
    if (mapIter != m_connections.end())
    {
        std::set<connection_ptr>::iterator iter = mapIter->second.begin();
        while (iter != mapIter->second.end())
        {
            if (client_id == (*iter)->get_client_id())
            {
                iter->get()->stop();
                break;
            }
            iter++;
        }
    }
}

/// \brief handler for stop accepting connections async request
void SecureServer::handle_stop_accepting_connections()
{
    m_acceptor.close();
}

std::string SecureServer::get_password() const
{
    return getPassphrase();
}

/// \brief set the allowed clients
void SecureServer::set_allowed_clients(std::set<std::string>& clients)
{
    m_allowed_clients = clients;
}

/// \brief check if a client is authorized to connect
///
/// one client is allowed to connect only once per connection type
///
/// \returns
/// true if client id is allowed
/// flase if client id is not allowed or already conneected
bool SecureServer::is_allowed_client(const std::string& client_id,
    const std::string& connection_type)
{
    if (m_allowed_clients.find(client_id) != m_allowed_clients.end())
    {
        boost::lock_guard<boost::mutex> lock(m_mutex);

        // compatibility mode
        if (connection_type.empty())
        {
            assert(m_connections.size() == 2);

            std::map<std::string, std::set<connection_ptr> >::iterator mapIter = m_connections.begin();
            while (mapIter != m_connections.end())
            {
                if (mapIter->first.compare(ConnectionTypeGeneric) != 0)
                {
                    std::set<connection_ptr>::iterator iter = mapIter->second.begin();
                    while (iter != mapIter->second.end())
                    {
                        if (boost::iequals(client_id, (*iter)->get_client_id()))
                        {
                            if ((*iter)->isloggedin())
                            {
                                inm_printf("%s: client %s of type %s already logged in.\n",
                                    __FUNCTION__,
                                    client_id.c_str(),
                                    connection_type.c_str());
                                return false;
                            }
                            else
                            {
                                mapIter->second.erase(iter);
                                return are_connections_accepted(mapIter->first);
                            }
                        }
                        iter++;
                    }
                    return are_connections_accepted(mapIter->first);
                }
                mapIter++;
            }
        }

        if (!are_connections_accepted(connection_type))
            return false;

        std::map<std::string, std::set<connection_ptr> >::iterator mapIter = m_connections.find(connection_type);
        if (mapIter != m_connections.end())
        {
            std::set<connection_ptr>::iterator iter = mapIter->second.begin();
            while (iter != mapIter->second.end())
            {
                if (boost::iequals(client_id, (*iter)->get_client_id()))
                {
                    if ((*iter)->isloggedin())
                    {
                        inm_printf("%s: client %s of type %s already logged in.\n",
                            __FUNCTION__,
                            client_id.c_str(),
                            connection_type.c_str());
                        return false;
                    }
                    else
                    {
                        mapIter->second.erase(iter);
                        return true;
                    }
                }
                iter++;
            }

            return true;
        }
        else
        {
            inm_printf("%s: for client %s of type %s, connections are not open.\n",
                __FUNCTION__,
                client_id.c_str(),
                connection_type.c_str());
            return false;
        }
    }

    inm_printf("%s: client %s of type %s is not allowed.\n",
        __FUNCTION__,
        client_id.c_str(),
        connection_type.c_str());
    return false;
}

/// \brief check if a client has a loggedin connection
///
/// one client is allowed to connect only once per connection type
///
/// \returns
/// true if client id is logged in
/// flase if client id is not logged in
bool SecureServer::is_client_loggedin(const std::string& client_id,
    const std::string& connection_type)
{
    std::map<std::string, std::set<connection_ptr> >::iterator mapIter = m_connections.find(connection_type);
    if (mapIter != m_connections.end())
    {
        std::set<connection_ptr>::iterator iter = mapIter->second.begin();
        for (/*empty*/; iter != mapIter->second.end(); iter++)
        {
            if (client_id == (*iter)->get_client_id())
                return ((*iter)->isloggedin());
        }
    }

    inm_printf("%s: client %s of type %s is not connected.\n",
        __FUNCTION__,
        client_id.c_str(),
        connection_type.c_str());
    return false;
}


/// \brief start new connections based on connection type 
///
/// \returns
/// true if a map for connections of type is created successfully
/// flase if fails to create connections type map
bool SecureServer::open_connections(const std::string& connection_type)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);
    std::map<std::string, std::set<connection_ptr> >::iterator mapIter = m_connections.find(connection_type);
    if (mapIter == m_connections.end())
    {
        std::set<connection_ptr> connections;
        m_connections.insert(std::make_pair(connection_type, connections));
        m_accepted_connection_types.insert(std::make_pair(connection_type, true));
        return true;
    }
    else
    {
        inm_printf("connections of type %s are already open.\n",
            connection_type.c_str());
        return false;
    }
}

/// \brief close connections based on connection type 
///
/// \returns
///     none
void SecureServer::close_connections(const std::string& connection_type)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);
    std::map<std::string, std::set<connection_ptr> >::iterator mapIter = m_connections.find(connection_type);
    if (mapIter != m_connections.end())
    {
        std::set<connection_ptr>::iterator iter = mapIter->second.begin();
        while (iter != mapIter->second.end())
        {
            iter->get()->stop();
            iter++;
        }

        m_connections.erase(mapIter);
    }

    std::map<std::string, bool>::iterator accIter = m_accepted_connection_types.find(connection_type);
    if (accIter != m_accepted_connection_types.end())
        m_accepted_connection_types.erase(accIter);

    return;
}

/// \brief update the connection to the corresponding type map
///
/// if a connection already present, the new connection is closed
///
/// \returns
/// true if connection is updated successfully
/// flase if fails to update the connections type map
bool SecureServer::update_connections(const std::string& client_id, std::string& connection_type)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);
    std::map<std::string, std::set<connection_ptr> >::iterator mapIter;

    // compatibility mode
    if (connection_type.empty())
    {
        assert(m_connections.size() == 2);

        mapIter = m_connections.begin();
        while (mapIter != m_connections.end())
        {
            if (mapIter->first.compare(ConnectionTypeGeneric) != 0)
            {
                connection_type = mapIter->first;
                break;
            }
            mapIter++;
        }
    }
    else
    {
        mapIter = m_connections.find(connection_type);
        if (mapIter != m_connections.end())
        {
            std::set<connection_ptr>::iterator iter = mapIter->second.begin();
            while (iter != mapIter->second.end())
            {
                if (client_id == (*iter)->get_client_id())
                {
                    inm_printf("connection for client %s of type %s already exists.\n",
                        client_id.c_str(),
                        connection_type.c_str());
                    return false;
                }
                iter++;
            }
        }
    }

    std::map<std::string, std::set<connection_ptr> >::iterator genIter = m_connections.find(ConnectionTypeGeneric);
    if (genIter != m_connections.end())
    {
        std::set<connection_ptr>::iterator iter = genIter->second.begin();
        while (iter != genIter->second.end())
        {
            if (client_id == (*iter)->get_client_id())
            {
                if (mapIter != m_connections.end())
                {
                    mapIter->second.insert(*iter);
                    genIter->second.erase(iter);
                    return true;
                }
                else
                {
                    inm_printf("connections for client %s of type %s are not open.\n",
                        client_id.c_str(),
                        connection_type.c_str());
                    return false;
                }
            }
            iter++;
        }
    }

    inm_printf("connections for client %s of type %s are not found.\n",
        client_id.c_str(),
        connection_type.c_str());
    return false;
}

// \brief set the accepted connection types to false
void SecureServer::stop_accepting_connections(const std::string& connection_type)
{
    std::map<std::string, bool>::iterator accIter = m_accepted_connection_types.find(connection_type);
    if (accIter != m_accepted_connection_types.end())
        accIter->second = false;
}

bool SecureServer::are_connections_accepted(const std::string& connection_type)
{
    std::map<std::string, bool>::iterator accIter = m_accepted_connection_types.find(connection_type);
    if (accIter != m_accepted_connection_types.end())
    {
        if (!accIter->second)
        {
            inm_printf("%s: for type %s, connections are not accepted.\n",
                __FUNCTION__,
                connection_type.c_str());
            return false;
        }
    }
    else
    {
        inm_printf("%s: for type %s, accepted connections not found.\n",
            __FUNCTION__,
            connection_type.c_str());
        return false;
    }

    return true;
}

/// \brief handler for accept
void SecureServer::handle_accept(connection_ptr new_connection,
                            const boost::system::error_code& error)
{
    if (!error)
    {
        boost::lock_guard<boost::mutex> lock(m_mutex);

        std::map<std::string, std::set<connection_ptr> >::iterator mapIter = m_connections.find(ConnectionTypeGeneric);
        if (mapIter != m_connections.end())
        {
            mapIter->second.insert(new_connection);
        }
        else
        {
            std::set<connection_ptr> genericConnections;
            genericConnections.insert(new_connection);
            m_connections.insert(std::make_pair(ConnectionTypeGeneric, genericConnections));
        }

        new_connection->set_connect_time();
        new_connection->start();

        new_connection.reset(new Connection(m_io_service, 
                                m_context, 
                                req_handler,
                                boost::bind(&SecureServer::is_allowed_client, this, _1, _2),
                                boost::bind(&SecureServer::update_connections, this, _1, _2)));

        m_acceptor.async_accept(new_connection->socket(),
                      boost::bind(&SecureServer::handle_accept, 
                                this, 
                                new_connection,
                                boost::asio::placeholders::error));

        // cleanup any old connections of generic type
        mapIter = m_connections.find(ConnectionTypeGeneric);
        if (mapIter != m_connections.end())
        {
            std::set<connection_ptr>::iterator iter = mapIter->second.begin();
            while (iter != mapIter->second.end())
            {
                if (!iter->get()->isloggedin() && 
                    (steady_clock::now() > (iter->get()->get_connect_time() + seconds(120))))
                {
                    mapIter->second.erase(iter++);
                }
                else if ((steady_clock::now() > (iter->get()->get_connect_time() + seconds(120))))
                {
                    iter->get()->stop();
                }
                else
                {
                    iter++;
                }
            }
        }
    }
    else
    {
        inm_printf("SecureServer::handle_accept failed with error %d (%s)\n",
            error.value(),
            error.message().c_str());
    }
}

/// \brief writes length bytes
///
/// blocks until length bytes written or an error
///
/// \returns
/// \li \c number of bytes written (which will always be length)
/// \li \c -1 on failure
size_t SecureServer::send(const std::string& client_id, 
                          const char *data,
                          const size_t length,
                          const std::string& connection_type)
{
    std::map<std::string, std::set<connection_ptr> >::iterator mapIter = m_connections.find(connection_type);
    if (mapIter != m_connections.end())
    {
        std::set<connection_ptr>::iterator iter = mapIter->second.begin();
        while (iter != mapIter->second.end())
        {
            if (client_id == (*iter)->get_client_id())
            {
                std::string strdata;
                strdata.assign(data, length);

                std::stringstream ss1;
                ss1 << protocol_header << delimiter;
                ss1 << msgTypes[SERVER_MSG] << delimiter;
                ss1 << genDigest(strdata) << delimiter;
                ss1 << strdata.length() << delimiter;
                ss1 << strdata << delimiter;

                try {
                    if ((*iter)->isloggedin())
                    {
                        (*iter)->write(ss1.str().c_str(), ss1.str().length());

                        inm_printf(LL_DEBUG,
                            "SecureServer::send success %s msg %s length %d\n",
                            client_id.c_str(),
                            ss1.str().c_str(),
                            ss1.str().length());

                        return length;
                    }
                    else
                    {
                        inm_printf("SecureServer::send failed for %s type %s message %s length %d.\n",
                            client_id.c_str(),
                            connection_type.c_str(),
                            ss1.str().c_str(),
                            ss1.str().length());

                        return -1;
                    }
                }
                catch (std::exception& e)
                {
                    inm_printf("SecureServer::send failed with %s for %s of type %s message %s length %d.\n",
                        e.what(),
                        client_id.c_str(),
                        connection_type.c_str(),
                        ss1.str().c_str(),
                        ss1.str().length());

                    return -1;
                }
            }

            iter++;
        }
    }

    inm_printf("SecureServer::send failed as connection not found for %s type %s msg %s length %d\n",
        client_id.c_str(),
        connection_type.c_str(),
        data,
        length);

    return -1;
}

SecureClient::SecureClient(std::string& client_id,
                        boost::function<void(const char *, size_t)> callback)
            : m_client_id(client_id),
            req_handler(callback),
            m_io_service(),
            m_context(boost::asio::ssl::context::tlsv12),
            m_socket(m_io_service, m_context),
            m_isloggedin(false)
{
    m_context.set_options(
        boost::asio::ssl::context::default_workarounds
        | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::no_sslv3
        | boost::asio::ssl::context::no_tlsv1
        | boost::asio::ssl::context::no_tlsv1_1
        | boost::asio::ssl::context::single_dh_use);

    if (!g_strMasterNodeFingerprint.empty())
    {
        m_socket.set_verify_mode(boost::asio::ssl::verify_peer);
        m_socket.set_verify_callback(boost::bind(&SecureClient::cert_verify_callback, this, _1, _2));
    }
}

/// \brief connects to the server at given port
///
/// \returns 
/// \li error_code returned by the socket connect or handshake on failure
/// \li error_code is set to 0 on success
/// 
boost::system::error_code SecureClient::connect(std::string &server,
                                    std::string &port)
{
    boost::system::error_code ec;

    boost::regex ipv4pattern("(\\d{1,3}(\\.\\d{1,3}){3})");
    boost::smatch matches;
    if (boost::regex_search(server, matches, ipv4pattern))
    {
        // boost.asio wants host byte order, inet_addr returns network byte order
        unsigned short iport = boost::lexical_cast<unsigned short>(port);
        boost::asio::ip::address_v4 address(ntohl(inet_addr(server.c_str())));
        boost::asio::ip::tcp::endpoint endpoint(address, (unsigned short)iport);
        ec = connect(endpoint);
    }
    else
    {
        boost::asio::ip::tcp::resolver resolver(m_io_service);
        boost::asio::ip::tcp::resolver::query query(server.c_str(), port.c_str());
        boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query, ec);
        if (ec)
        {
            inm_printf(LL_ERROR,
                "SecureClient::connect: failed to resolve server address %s port %s with error %d: %s\n",
                server.c_str(),
                port.c_str(),
                ec.value(),
                ec.message().c_str());
            return ec;
        }

        for (/*empty*/;
            iterator != boost::asio::ip::tcp::resolver::iterator();
            iterator++)
        {
            if (!iterator->endpoint().address().is_v4())
                continue;

            boost::asio::ip::tcp::endpoint endpoint(iterator->endpoint());
            ec = connect(endpoint);
            if (!ec)
                break;
        }
    }

    if (!ec)
    {
        m_server_id.assign(server);
        m_socket.handshake(boost::asio::ssl::stream_base::client, ec);
    }

    return ec;
}

/// \brief connects to a tcp ip endpoint
///
/// \returns
/// \li error_code returned by the socket connect or handshake on failure
/// \li error_code is set to 0 on success
///
boost::system::error_code SecureClient::connect(boost::asio::ip::tcp::endpoint &endpoint)
{
    boost::system::error_code ec;

    m_socket.lowest_layer().connect(endpoint, ec);
    if (ec)
    {
        inm_printf(LL_ERROR,
            "SecureClient::connect: failed to connect to server at address %s port %d with error %d: %s\n",
            endpoint.address().to_string().c_str(),
            endpoint.port(),
            ec.value(),
            ec.message().c_str());

        m_socket.lowest_layer().close();
    }
    else
    {
        inm_printf(LL_DEBUG,
            "SecureClient::connect: successfully connected to server address %s port %d\n",
            endpoint.address().to_string().c_str(),
            endpoint.port());

        if (!setSocketTimeoutOptions(m_socket.lowest_layer().native_handle(), CLIENT_SOCKET_TIMEOUT))
            setSocketTimeoutForAsyncRequests(m_socket.lowest_layer().native_handle(), CLIENT_SOCKET_TIMEOUT);
    }

    return ec;
}

/// \brief start the io service
/// 
/// this call blocks until the io service is stopped
/// should be called in a seperate thread
///
void SecureClient::run(int threadCount)
{
    async_read_some();

    boost::thread_group thread_group;
    for (int i = 0; i < threadCount; i++)
    {
        boost::thread *thread = new boost::thread(boost::bind(&boost::asio::io_service::run, &m_io_service));
        thread_group.add_thread(thread);
    }

    thread_group.join_all();

    return;
}


/// \brief post a async close
///
void SecureClient::close()
{
    m_io_service.post(boost::bind(&SecureClient::handle_stop, this));
}

/// \brief close the client
///  
/// closes the client socket and stops the io service
///
void SecureClient::handle_stop()
{
    boost::system::error_code ec;
    m_socket.lowest_layer().cancel(ec);
    m_socket.lowest_layer().close(ec);
    m_io_service.stop();
}

/// \brief process the login reply 
void SecureClient::process_login_reply(std::string &login_data)
{
    inm_printf(LL_DEBUG, "SecureClient::process_login_reply %s\n", login_data.c_str());
    std::vector<std::string> login_info;
    boost::split(login_info, login_data, boost::is_any_of(login_delimiter), boost::token_compress_on);
    size_t pos = login_data.find_last_of(':');
    std::string client_id = login_data.substr(0,pos);
    std::string connection_type, aad_token;
    bool bvalidInput = true;

    if (login_info.size() == VALID_NUM_LOGIN_TOKENS)
    {
        connection_type = login_info[LOGIN_INDEX_CONNECTION_TYPE];
        aad_token = login_info[LOGIN_INDEX_AAD_TOKEN];
    }
    else if (g_strMasterNodeFingerprint.empty() && (login_info.size() == VALID_NUM_LOGIN_TOKENS_NO_AAD))
    {
        connection_type = login_info[LOGIN_INDEX_CONNECTION_TYPE];
        aad_token = std::string();
    }
    else if (login_info.size() == VALID_NUM_LOGIN_TOKENS_COMPAT_MODE)
    {
        // try compatibility mode
        connection_type = aad_token = std::string();
    }
    else
    {
        bvalidInput = false;
    }

    if (bvalidInput &&
        (verify_id(login_info[LOGIN_INDEX_CLIENTID], 
                    login_info[LOGIN_INDEX_SERVERID],
                    connection_type,
                    aad_token,
                    login_info[LOGIN_INDEX_NONCE], 
                    client_id)))
    {
        inm_printf(LL_DEBUG, "SecureClient::process_login_reply %s\n", login_info[1].c_str());
        boost::lock_guard<boost::mutex> lock(m_mutex);
        m_isloggedin = true;
        return;
    }

    std::stringstream errmsg;
    errmsg << "invalid login data: "  << login_data;
    inm_printf("%s\n", errmsg.str().c_str());
    return;
}

/// \brief process the received bytes
///
/// if the received bytes contain a login reply, validate login reply
/// otherwise call the request handler to process the data
///
void SecureClient::process(std::string &msg_data)
{
    std::string msg_type;
    std::string msg_req;

    if (!process_message(msg_data, msg_req, msg_type))
        return;

    if (msg_type.compare(msgTypes[LOGIN_REPLY]) == 0)
    {
        process_login_reply(msg_req);
    }
    else
    {
        if (m_isloggedin)
            req_handler(msg_req.c_str(), msg_req.length());
        else
            inm_printf("SecureClient::process received data without login, dropped.\n");
    }

    msg_data.erase(0, msg_req.length());

    return;
}

/// \brief send login request to server
///
/// blocks until login completes or an error
///
/// \exception throws ERROR_EXCEPTION on failure
///
void SecureClient::login(const std::string& connection_type)
{
    inm_printf(LL_DEBUG, "SecureClient::login client_id %s  server_id %s\n", m_client_id.c_str(), m_server_id.c_str());
    std::string aad_token;
    if (!g_strMasterNodeFingerprint.empty())
    {
        if (g_ptrRcmClientSettings.get() == NULL)
            g_ptrRcmClientSettings.reset( new RcmClientSettings(g_strRcmSettingsPath));

        if (g_ptrAADAuthProvider.get() == NULL)
            g_ptrAADAuthProvider.reset(new AADAuthProvider(*g_ptrRcmClientSettings.get()));

        if (!g_strProxySettingsPath.empty() &&
            boost::filesystem::exists(g_strProxySettingsPath))
        {
            if (g_ptrProxySettings.get() == NULL)
            g_ptrProxySettings.reset(new ProxySettings(g_strProxySettingsPath));

            g_ptrAADAuthProvider->SetHttpProxy(g_ptrProxySettings->m_Address, g_ptrProxySettings->m_Port, g_ptrProxySettings->m_Bypasslist);
        }

        if (SVS_OK != g_ptrAADAuthProvider->GetBearerToken(aad_token))
        {
            // Just log and continue, let client verification fail in server.
            inm_printf(LL_ERROR, "Failed to get bearer token. Error=%d\n", g_ptrAADAuthProvider->GetErrorCode());
        }
    }

    std::string nonce = securitylib::genRandNonce(16);
    std::string login_id = build_id(m_client_id, m_server_id, connection_type, aad_token, nonce);

    std::stringstream ss1;
    ss1 << protocol_header << delimiter;
    ss1 << msgTypes[LOGIN_REQUEST] << delimiter;
    ss1 << genDigest(login_id) << delimiter;
    ss1 << login_id.length() << delimiter;
    ss1 << login_id << delimiter;

    inm_printf(LL_DEBUG, "SecureClient::login req %s\n", ss1.str().c_str());

    size_t nbytes = boost::asio::write(m_socket, 
        boost::asio::buffer(ss1.str().c_str(), ss1.str().length()));

    inm_printf(LL_DEBUG, "SecureClient::login nbytes %d\n", nbytes);

    boost::asio::deadline_timer t(m_io_service, boost::posix_time::seconds(1));
    t.wait();

    boost::unique_lock<boost::mutex> lock(m_mutex);
    inm_printf(LL_DEBUG, "SecureClient::login m_isloggedin %d.\n", m_isloggedin);

    std::stringstream errormsg;
    errormsg << "login from client " << m_client_id << " to server ";
    errormsg << m_server_id << " failed." ;

    if (!m_isloggedin)
        throw ERROR_EXCEPTION << errormsg.str().c_str();

    return;
}

/// \brief reads some bytes asynchronously from socket
void SecureClient::async_read_some()
{
    m_socket.async_read_some(boost::asio::buffer(m_recv_buf, max_length),
            boost::bind(&SecureClient::handle_read, 
                        this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
}

/// \brief callback to handle async reads 
void SecureClient::handle_read(const boost::system::error_code& error,
                size_t bytes_transferred)
{
    if (!error)
    {
        if (bytes_transferred) 
        {
            std::string msg_data;
            msg_data.assign(m_recv_buf, bytes_transferred);

            m_recvd_data += msg_data;
            async_read_some();

            process(m_recvd_data);
        }
        else
            inm_printf("SecureClient::handle_read received no data.\n");
    }
    else
    {
        inm_printf("SecureClient::handle_read failed with error %d %s\n",
            error.value(),
            error.message().c_str());
    }

    return;
}

/// \brief writes length bytes
///
/// blocks until length bytes written or an error
///
/// \returns
/// \li \c number of bytes written (which will always be length)
/// \li \c -1 on failure
size_t SecureClient::send(const std::string& destId,
                          const char *data,
                          size_t length,
                          const std::string& connection_type)
{
    std::string strdata;
    strdata.assign(data, length);

    std::stringstream ss1;
    ss1 << protocol_header << delimiter;
    ss1 << msgTypes[CLIENT_MSG] << delimiter ;
    ss1 << genDigest(strdata) << delimiter;
    ss1 << strdata.length() << delimiter;
    ss1 << strdata << delimiter;

    try {
        if (!m_socket.lowest_layer().is_open())
        {
            m_isloggedin = false;
            inm_printf("SecureClient::send failed as socket is closed for %s\n", m_client_id.c_str());
            return -1;
        }

        boost::asio::write(m_socket, 
                       boost::asio::buffer(ss1.str().c_str(), 
                                           ss1.str().length()));
    }
    catch(std::exception &e)
    {
        inm_printf("SecureClient::send failed with %s msg %s len %d\n", 
                   e.what(),
                   ss1.str().c_str(), 
                   ss1.str().length());
        return -1;
    }

    inm_printf(LL_DEBUG, "SecureClient::send msg %s len %d\n", ss1.str().c_str(), ss1.str().length());

    return length;
}

bool SecureClient::cert_verify_callback(bool preverified, boost::asio::ssl::verify_context& vctx)
{
    X509* mncert = X509_STORE_CTX_get_current_cert(vctx.native_handle());
    std::string mnfingerprint = securitylib::GenCert::extractFingerprint(mncert);

    inm_printf(LL_DEBUG, "%s: g_strMasterNodeFingerprint= %s, mnfingerprint %s\n",
        __FUNCTION__,
        g_strMasterNodeFingerprint.c_str(),
        mnfingerprint.c_str());
    return (boost::iequals(mnfingerprint, g_strMasterNodeFingerprint));
}
