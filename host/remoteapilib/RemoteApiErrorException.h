///
///  \file RemoteApiErrorException.h
///
///  \brief an error exception used to throw errors in Remote API project for specific handling.
///

#ifndef REMOTEAPIERROREXCEPTION_H
#define REMOTEAPIERROREXCEPTION_H

#include "errorexception.h"
#include "RemoteApiErrorCode.h"

/// \brief for throwing exceptions for Remote API failures.
class RemoteApiErrorException : public ErrorException {
public:
	/// \brief constructor
	RemoteApiErrorException(RemoteApiErrorException const& ec)
		: ErrorException(ec.what())
	{
		m_RemoteApiErrorCode = ec.m_RemoteApiErrorCode;
		m_what = ec.what();
	}

	RemoteApiErrorException(RemoteApiErrorCode remoteApiErrorCode, std::string const& data = std::string())
		: ErrorException(data)
	{
		m_RemoteApiErrorCode = remoteApiErrorCode;
		m_what = data;
	}

	/// \brief get the remote api error code.
	RemoteApiErrorCode getRemoteApiErrorCode()
	{
		return m_RemoteApiErrorCode;
	}

	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	RemoteApiErrorException& operator<<(DATA_TYPE const& data) ///< additional data to add
	{
		m_stream << data;
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}


	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	RemoteApiErrorException& operator<<(DATA_TYPE& data) ///< additional data to add
	{
		m_stream << data;
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	RemoteApiErrorException& operator<<(std::wstring const& data) ///< additional data to add
	{
		std::locale loc;

		std::wstring::const_iterator it(data.begin());
		std::wstring::const_iterator endIt(data.end());
		for (/* empty */; it != endIt; ++it) {
			m_stream << std::use_facet<std::ctype<wchar_t> >(loc).narrow(*it, '.');
		}

		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	RemoteApiErrorException& operator<<(std::wstring& data) ///< additional data to add
	{
		std::locale loc;

		std::wstring::const_iterator it(data.begin());
		std::wstring::const_iterator endIt(data.end());
		for (/* empty */; it != endIt; ++it) {
			m_stream << std::use_facet<std::ctype<wchar_t> >(loc).narrow(*it, '.');
		}

		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wchar_t* data using iostream style syntax
	RemoteApiErrorException& operator<<(wchar_t const*& data) ///< additional data to add
	{
		std::locale loc;
		wchar_t const* wCh = data;
		for (/* empty */; L'\0' != *wCh; ++wCh) {
			m_stream << std::use_facet<std::ctype<wchar_t> >(loc).narrow(*wCh, '.');
		}
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wchar_t* data using iostream style syntax
	RemoteApiErrorException& operator<<(wchar_t*& data) ///< additional data to add
	{
		std::locale loc;
		wchar_t const* wCh = data;
		for (/* empty */; L'\0' != *wCh; ++wCh) {
			m_stream << std::use_facet<std::ctype<wchar_t> >(loc).narrow(*wCh, '.');
		}
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	RemoteApiErrorException& operator<<(wchar_t const& data) ///< additional data to add
	{
		std::locale loc;
		m_stream << std::use_facet<std::ctype<wchar_t> >(loc).narrow(data, '.');
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	RemoteApiErrorException& operator<<(wchar_t& data) ///< additional data to add
	{
		std::locale loc;
		m_stream << std::use_facet<std::ctype<wchar_t> >(loc).narrow(data, '.');
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief get the exception text.
	const char* what() const throw() {
		return m_what.c_str();
	}

	/// \brief destructor
	~RemoteApiErrorException() throw()
	{ }

protected:

private:
	RemoteApiErrorCode m_RemoteApiErrorCode;
	std::stringstream m_stream;
	std::string m_what;
};

#endif // REMOTEAPIERROREXCEPTION_H
