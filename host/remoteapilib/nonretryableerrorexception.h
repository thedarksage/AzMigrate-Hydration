
///
///  \file nonretryableerrorexception.h
///
///  \brief a non-retryable error exception used to throw errors
///
/// \note
/// \li \c not all io manipulators will work. in particular std::endl, std::flush. But those  should not be needed in this case
/// \li \c most formatting manipulators do work which might be usefull
/// (std::hex, std::dec, std::setw, std::setfill, std::left, std::right, others)
///

#ifndef NON_RETRYABLE_ERROR_EXCEPTION_H
#define NON_RETRYABLE_ERROR_EXCEPTION_H

#include <stdexcept>
#include <string>
#include <sstream>

#include <boost/throw_exception.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "atloc.h"

/// \brief for throwing exceptions
class NonRetryableErrorException : public std::exception {
public:
	/// \brief constructor
	///
	/// \param data to include with the exception
	explicit NonRetryableErrorException(std::string const& data = std::string())
	{
		m_what = data;
	}

	NonRetryableErrorException(NonRetryableErrorException const& ec)
		: std::exception(ec)
	{
			m_what = ec.m_what;
	}

	/// \brief destructor
	~NonRetryableErrorException() throw()
	{ }

	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	NonRetryableErrorException& operator<<(DATA_TYPE const& data) ///< additional data to add
	{
		m_stream << data;
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}


	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	NonRetryableErrorException& operator<<(DATA_TYPE& data) ///< additional data to add
	{
		m_stream << data;
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	NonRetryableErrorException& operator<<(std::wstring const& data) ///< additional data to add
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
	NonRetryableErrorException& operator<<(std::wstring& data) ///< additional data to add
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
	NonRetryableErrorException& operator<<(wchar_t const*& data) ///< additional data to add
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
	NonRetryableErrorException& operator<<(wchar_t *& data) ///< additional data to add
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
	NonRetryableErrorException& operator<<(wchar_t const& data) ///< additional data to add
	{
		std::locale loc;
		m_stream << std::use_facet<std::ctype<wchar_t> >(loc).narrow(data, '.');
		m_what += m_stream.str();
		m_stream.str(std::string());
		m_stream.clear();
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	NonRetryableErrorException& operator<<(wchar_t& data) ///< additional data to add
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

protected:

private:
	std::stringstream m_stream;
	std::string m_what;
};

/// \brief helps in creating and throwing an NonRetryableErrorException
/// that has various type of data to include with the exception
///
/// \note
/// \li need to use throw keyword as it is not part of the macro
/// \li places the throw location (AT_LOC) before the exception message
/// \li exception messgage does not get put inside parentheses
/// 
/// e.g.
/// \code
/// throw NON_RETRYABLE_ERROR_EXCEPTION << "error occured: " << errno;
/// \endcode
#define NON_RETRYABLE_ERROR_EXCEPTION  NonRetryableErrorException(AT_LOC)

/// \brief helps in creating and throwing an NonRetryableErrorException using boost::throw_exception
/// that has various type of data to include with the exception
///
/// \note
/// \li throw keyword is part of the macro, so do not add it
/// \li places the throw location (THROW_LOC) before the exception message
/// \li exception messgage must be inside parentheses
///
/// e.g.
/// \code
/// BOOST_NON_RETRYABLE_ERROR_EXCEPTION("error occured: " << errno);
/// \endcode
#define BOOST_NON_RETRYABLE_ERROR_EXCEPTION(xErrExpWhat) boost::throw_exception(NON_RETRYABLE_ERROR_EXCEPTION << xErrExpWhat)


/// \brief helps in creating and throwing an NonRetryableErrorException
/// that has various type of data to include with the exception
///
/// \note
/// \li throw keyword is part of the macro, so do not add it
/// \li places the throw location (THROW_LOC) after the exception message
/// \li exception messgage must be inside parentheses
/// 
/// e.g.
/// \code
/// THROW_NON_RETRYABLE_ERROR_EXCEPTION("error occured: " << errno);
/// \endcode
#define THROW_NON_RETRYABLE_ERROR_EXCEPTION(xErrExpWhat) throw NonRetryableErrorException() << xErrExpWhat << '\t' << THROW_LOC_NO_TAB

/// \brief helps in creating and throwing an NonRetryableErrorException using boost::throw_exception
/// that has various type of data to include with the exception
///
/// \note
/// \li throw keyword is part of the macro, so do not add it
/// \li places the throw location (THROW_LOC) after the exception message
/// \li exception messgage must be inside parentheses
///
/// e.g.
/// \code
/// BOOST_THROW_NON_RETRYABLE_ERROR_EXCEPTION("error occured: " << errno);
/// \endcode
#define BOOST_THROW_NON_RETRYABLE_ERROR_EXCEPTION(boostErrExpWhat) boost::throw_exception(NonRetryableErrorException() << boostErrExpWhat << '\t' << THROW_LOC_NO_TAB)

#endif // NON_RETRYABLE_ERROR_EXCEPTION_H
