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

#include "SecureTransport.h"
#include "securityutils.h"
#include "defaultdirs.h"
#include "getpassphrase.h"
#include "errorexception.h"
#include "setsockettimeoutsminor.h"
#include "gencert.h"
#include "inmsafecapis.h"
#include "proxysettings.h"

#define TRIM_CHARS "\n"

#define TIME_STAMP_LENGTH 70

#define MAX_VACP_LOG_FILE_SIZE (10 * 1024 * 1024)

const int MAX_OUTPUT_SIZE = 256 * 1024; // in bytes, 256KB


#ifdef _WIN32
#define VSNPRINTF(buf, size, maxCount, format, args) _vsnprintf_s(buf, size, maxCount, format, args)
#else
#define VSNPRINTF(buf, size, maxCount, format, args) inm_vsnprintf_s(buf, size, format, args)
#endif



enum LOG_LEVEL {
	LL_ERROR = 0,
	LL_WARNING = 1,
	LL_INFO = 2,
	LL_DEBUG = 3
};

void inm_printf(const char* format, ...)
{

	std::string msg(MAX_OUTPUT_SIZE, 0);

	va_list a;
	va_start(a, format);

	VSNPRINTF(&msg[0], msg.size(), msg.size() - 1, format, a);

	if (0 == strlen(&msg[0]))
	{
		return;
	}

	va_end(a);

	boost::trim_left_if(msg, boost::is_any_of(TRIM_CHARS));

	struct tm today;
	time_t ltime;
	time(&ltime);

	int msecs = 0;

#if defined(WIN32) || defined(WIN64) 
	localtime_s(&today, &ltime);
#else
	localtime_r(&ltime, &today);
#endif

	std::string present;
	present.resize(TIME_STAMP_LENGTH);
	inm_sprintf_s(&present[0], present.capacity(), "(%02d-%02d-20%02d %02d:%02d:%02d:%llu): DEBUG ",
		today.tm_mon + 1,
		today.tm_mday,
		today.tm_year - 100,
		today.tm_hour,
		today.tm_min,
		today.tm_sec,
		msecs
	);

	std::cout << msg.c_str();
}

void inm_printf(short logLevl, const char* format, ...)
{
	va_list a;
	va_start(a, format);
	inm_printf(format, a);
	va_end(a);
}

Connection::Connection(boost::asio::io_service &io_service,
	boost::asio::ssl::context &context,
	boost::function<void(const std::string&, const char *, size_t)> req_handler)
	: m_socket(io_service, context),
	m_strand(io_service),
	req_handler(req_handler)
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
		char* tmp = SSL_CIPHER_get_version(sslCipher);
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
		inm_printf("Current cipher : %s\n", cipher.c_str());

		async_read_some();
	}
	else
	{
		inm_printf("client handshake failed: %d %s\n", error.value(), error.message().c_str());
		stop();
	}
}

/// \brief process the received data
void Connection::process(std::string &msg_data)
{
	// TBD
	//Process the data received from server
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
	SSL_CTX* sslCtx = m_context.native_handle();
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
		req_handler));

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
	inm_printf("Server is running...\n");
	thread_group.join_all();
}

/// \brief posts an async requet to stop the server
void SecureServer::close()
{
	inm_printf("Closing Server\n");
	m_io_service.post(boost::bind(&SecureServer::handle_stop_server, this));
}

/// \brief handler to stop the server async request
/// 
/// closes all the client connections
///
void SecureServer::handle_stop_server()
{
	m_io_service.stop();
}

/// \brief handler for stop accepting connections async request
void SecureServer::handle_stop_accepting_connections()
{
	m_acceptor.close();
}

std::string SecureServer::get_password() const
{
	return std::string();
}

/// \brief handler for accept
void SecureServer::handle_accept(connection_ptr new_connection,
	const boost::system::error_code& error)
{
	if (!error)
	{
		boost::lock_guard<boost::mutex> lock(m_mutex);

		new_connection->set_connect_time();
		new_connection->start();

		new_connection.reset(new Connection(m_io_service,
			m_context,
			req_handler));

		m_acceptor.async_accept(new_connection->socket(),
			boost::bind(&SecureServer::handle_accept,
				this,
				new_connection,
				boost::asio::placeholders::error));

	}
	else
	{
		inm_printf("SecureServer::handle_accept failed with error %d (%s)\n",
			error.value(),
			error.message().c_str());
	}
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

	m_socket.set_verify_mode(boost::asio::ssl::verify_peer);
	m_socket.set_verify_callback(boost::bind(&SecureClient::cert_verify_callback, this, _1, _2));
}

/// \brief connects to the server at give  port
///
/// \returns 
/// \li error_code returned by the socket connect or handshake on failure
/// \li error_code is set to 0 on success
/// 
boost::system::error_code SecureClient::connect(std::string &server,
	std::string &port)
{
	boost::system::error_code ec;

	boost::asio::ip::tcp::resolver resolver(m_io_service);
	boost::asio::ip::tcp::resolver::query query(server.c_str(), port.c_str());
	boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

	boost::asio::ip::tcp::endpoint endpoint = *iterator;
	while (iterator != boost::asio::ip::tcp::resolver::iterator())
	{
		m_socket.lowest_layer().connect(endpoint, ec);
		if (ec)
		{
			m_socket.lowest_layer().close();
			iterator++;
		}
		else
		{
			if (!setSocketTimeoutOptions(m_socket.lowest_layer().native_handle(), CLIENT_SOCKET_TIMEOUT))
				setSocketTimeoutForAsyncRequests(m_socket.lowest_layer().native_handle(), CLIENT_SOCKET_TIMEOUT);

			m_server_id.assign(server);
			m_socket.handshake(boost::asio::ssl::stream_base::client, ec);
			return ec;
		}
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
	inm_printf("Client running...\n");
	thread_group.join_all();

	return;
}


/// \brief post a async close
///
void SecureClient::close()
{
	inm_printf("Closing Client...\n");
	m_io_service.post(boost::bind(&SecureClient::handle_stop, this));
}

/// \brief close the client
///  
/// closes the client socket and stops the io service
///
void SecureClient::handle_stop()
{
	boost::system::error_code ec;
	//    m_socket.lowest_layer().cancel(ec);
	m_socket.lowest_layer().close(ec);
	m_io_service.stop();
}

/// \brief process the received bytes
///
/// if the received bytes contain a login reply, validate login reply
/// otherwise call the request handler to process the data
///
void SecureClient::process(std::string &msg_data)
{
	// TBD
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
	ss1 << strdata;

	try {
		if (!m_socket.lowest_layer().is_open())
		{
			inm_printf("SecureClient::send failed as socket is closed for %s\n", m_client_id.c_str());
			return -1;
		}

		boost::asio::write(m_socket,
			boost::asio::buffer(ss1.str().c_str(),
				ss1.str().length()));
	}
	catch (std::exception &e)
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
	/*X509* cert = X509_STORE_CTX_get_current_cert(vctx.native_handle());
	std::string fingerprint = securitylib::GenCert::extractFingerprint(cert);

	inm_printf(LL_DEBUG, "%s: fingerprint %s\n",
		__FUNCTION__,
		fingerprint.c_str());*/

	// TBD

	return true;
}
