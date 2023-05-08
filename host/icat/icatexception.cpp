#include "icatexception.h"

icatException::icatException(const std::string& str)
{
	m_str = str ;
}
const char* icatException::what()
{
    return m_str.c_str() ;
}


icatThreadException::	icatThreadException(const std::string& str) 
: icatException(str)
{

}

icatOutOfMemoryException ::icatOutOfMemoryException (const	std::string& str)
:icatException(str)
{
}
icatTransportException::icatTransportException(ICAT_TRANSPORT_PROTOCOL proto)
:icatException("")
{
	m_proto = proto ;
}
icatTransportException::icatTransportException(const std::string& error, ICAT_TRANSPORT_PROTOCOL proto)
: icatException(error)
{
	m_proto = proto ;
}
ICAT_TRANSPORT_PROTOCOL icatTransportException::getProtocol()
{
	return m_proto ;
}


icatHttpTransportException::icatHttpTransportException(CURLcode code, int responseCode)
: icatTransportException(TRANS_PROTO_HTTP) 
{
	m_code = code ;
	m_responseCode =  responseCode ;
}

icatHttpTransportException::icatHttpTransportException(const std::string& error)
: icatTransportException(error, TRANS_PROTO_HTTP)
{
}
icatUnsupportedOperException::icatUnsupportedOperException(ICAT_OPERATION operation):icatException("" ) 
{
	m_operation = operation ;
}

ICAT_OPERATION icatUnsupportedOperException::getOperation()
{
	return m_operation ;
}

icatFileNotFoundException::icatFileNotFoundException(const std::string& fileName):icatException("" ) 
{
	m_fileName = fileName ;
}

std::string icatFileNotFoundException::getFileName()
{
	return m_fileName ;
}

icatFileIOException::icatFileIOException(const std::string& fileName):icatException("" ) 
{
	m_fileName = fileName ;
}

std::string icatFileIOException::getFileName()
{
	return m_fileName ;
}

icatParseException::icatParseException(const std::string& str)
:icatException(str)
{

}
ParseException::ParseException()
:icatException("Could not parse given Input")
{
}
ParseException::ParseException(const std::string& str)
: icatException(str)
{
}

InvalidInput::InvalidInput(const std::string& str)
:ParseException( str )
{
}
std::string InvalidInput::getInvalidInput()
{
    return what();
}

