#ifndef ICAT_EXCEPTION_H
#define ICAT_EXCEPTION_H
#include "defs.h"
#include <string>
#include <curl/curl.h>

class icatException: public std::exception
{
	std::string m_str ;
public:
	icatException(const std::string&) ;
	const char* what() ;
	virtual ~icatException() throw(){ } 
} ;
class icatThreadException: public icatException
{
public:
	icatThreadException(const std::string&) ;
	~icatThreadException()  throw() {}
} ;

class icatOutOfMemoryException : public icatException
{
public:
	icatOutOfMemoryException(const std::string&) ;
	~icatOutOfMemoryException()  throw() {}
} ;
class icatTransportException: public icatException
{
	ICAT_TRANSPORT_PROTOCOL m_proto ;
public:
	icatTransportException(ICAT_TRANSPORT_PROTOCOL=TRANS_PROTO_NONE) ;
	icatTransportException(const std::string&, ICAT_TRANSPORT_PROTOCOL=TRANS_PROTO_NONE) ;
	ICAT_TRANSPORT_PROTOCOL getProtocol() ;
	~icatTransportException()  throw() {}
} ;

class icatHttpTransportException : public icatTransportException
{
	CURLcode m_code ;
	int m_responseCode ;
public:
	icatHttpTransportException(CURLcode code, int responseCode) ;
	icatHttpTransportException(const std::string&) ;
	int getResponseCode() { return m_responseCode ; }
	CURLcode getCode() { return m_code ; }
    ~icatHttpTransportException()throw(){}
} ;

class icatUnsupportedOperException: public icatException
{
	ICAT_OPERATION m_operation ;
public:
	icatUnsupportedOperException(ICAT_OPERATION operation=ICAT_OPER_NONE) ;
	ICAT_OPERATION getOperation() ;
	~icatUnsupportedOperException() throw() {}
} ;

class icatParseException: public icatException
{
public:
	icatParseException(const std::string&) ;
	~icatParseException() throw() {}
} ;

class icatFileNotFoundException : public icatException
{
	std::string m_fileName ;
public:
	icatFileNotFoundException(const std::string&) ;
	std::string getFileName() ;
	~icatFileNotFoundException() throw() {}
} ;

class icatFileIOException : public icatException
{
	std::string m_fileName ;
public:
	icatFileIOException(const std::string&) ;
	std::string getFileName() ;
	~icatFileIOException() throw() {}
} ;

class ParseException: public icatException
{
    std::string m_errorstring ;
    
public:
    std::string inputType;
	ParseException() ;
	ParseException(const std::string&) ;
	~ParseException() throw() {}
};

class InvalidInput: public ParseException
{
    std::string m_inval;
public:    
    
    InvalidInput(const std::string&);
    std::string getInvalidInput();
	~InvalidInput() throw(){} 
};

#endif

