
///
///  \file WmiErrorException.h
///
///  \brief an error exception used to throw errors in wmi api failures
///
/// \note
/// \li \c not all io manipulators will work. in particular std::endl, std::flush. But those  should not be needed in this case
/// \li \c most formatting manipulators do work which might be usefull
/// (std::hex, std::dec, std::setw, std::setfill, std::left, std::right, others)
///

#ifndef WMIERROREXCEPTION_H
#define WMIERROREXCEPTION_H

#include "errorexception.h"
#include "WmiErrorCode.h"

/// \brief for throwing exceptions for wmi api failures
class WmiErrorException : public ErrorException {
public:
	/// \brief constructor
	WmiErrorException(WmiErrorException const& ec)
		: ErrorException(ec.what())
	{
		m_wmiErrorCode = ec.m_wmiErrorCode;
		m_what = ec.what();
	}

	WmiErrorException(WmiErrorCode wmiErrorCode, std::string const& data = std::string())
		: ErrorException(data)
	{
		m_wmiErrorCode = wmiErrorCode;
		m_what = data;
	}

	/// \brief get the wmi error code
	WmiErrorCode getWmiErrorCode()
	{
		return m_wmiErrorCode;
	}

	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	WmiErrorException& operator<<(DATA_TYPE const& data) ///< additional data to add
	{
		m_stream << data;
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}


	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	WmiErrorException& operator<<(DATA_TYPE& data) ///< additional data to add
	{
		m_stream << data;
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	WmiErrorException& operator<<(std::wstring const& data) ///< additional data to add
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
	WmiErrorException& operator<<(std::wstring& data) ///< additional data to add
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
	WmiErrorException& operator<<(wchar_t const*& data) ///< additional data to add
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
	WmiErrorException& operator<<(wchar_t *& data) ///< additional data to add
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
	WmiErrorException& operator<<(wchar_t const& data) ///< additional data to add
	{
		std::locale loc;
		m_stream << std::use_facet<std::ctype<wchar_t> >(loc).narrow(data, '.');
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	WmiErrorException& operator<<(wchar_t& data) ///< additional data to add
	{
		std::locale loc;
		m_stream << std::use_facet<std::ctype<wchar_t> >(loc).narrow(data, '.');
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief get the exception text
	const char* what() const throw() {
		return m_what.c_str();
	}

	/// \brief destructor
	~WmiErrorException() throw()
	{ }

protected:

private:
	WmiErrorCode m_wmiErrorCode;
	std::stringstream m_stream;
	std::string m_what;
};

#endif // WMIERROREXCEPTION_H
