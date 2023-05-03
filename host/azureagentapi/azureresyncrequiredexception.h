
///
///  \file azureresyncrequiredexception.h
///
///  \brief an error exception used to throw errors
///
/// \note
/// \li \c not all io manipulators will work. in particular std::endl, std::flush. But those  should not be needed in this case
/// \li \c most formatting manipulators do work which might be usefull
/// (std::hex, std::dec, std::setw, std::setfill, std::left, std::right, others)
///

#ifndef AZURERESYNCREQUIREDEXCEPTION_H
#define AZURERESYNCREQUIREDEXCEPTION_H

#include <stdexcept>
#include <string>
#include <sstream>

#include <boost/throw_exception.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "atloc.h"
#include "errorexception.h"

/// \brief for throwing exceptions
class AzureResyncRequiredException : public ErrorException {
public:
	/// \brief constructor
	///
	/// \param data to include with the exception
	explicit AzureResyncRequiredException(std::string const& data = std::string())
		:ErrorException(data)
	{

	}

	AzureResyncRequiredException(AzureResyncRequiredException const& ec)
		: ErrorException(ec)
	{
	}

	/// \brief destructor
	virtual ~AzureResyncRequiredException() throw()
	{ }

	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	AzureResyncRequiredException& operator<<(DATA_TYPE const& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}


	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	AzureResyncRequiredException& operator<<(DATA_TYPE& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	AzureResyncRequiredException& operator<<(std::wstring const& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	AzureResyncRequiredException& operator<<(std::wstring& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t* data using iostream style syntax
	AzureResyncRequiredException& operator<<(wchar_t const*& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t* data using iostream style syntax
	AzureResyncRequiredException& operator<<(wchar_t *& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	AzureResyncRequiredException& operator<<(wchar_t const& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	AzureResyncRequiredException& operator<<(wchar_t& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}


protected:

private:
};

/// \brief helps in creating and throwing an ErrorException
/// that has various type of data to include with the exception
///
/// \note
/// \li need to use throw keyword as it is not part of the macro
/// \li places the throw location (AT_LOC) before the exception message
/// \li exception messgage does not get put inside parentheses
/// 
/// e.g.
/// \code
/// throw AZURE_RESYNCREQUIRED_EXCEPTION << "error occured: " << errno;
/// \endcode
#define AZURE_RESYNCREQUIRED_EXCEPTION  AzureResyncRequiredException(AT_LOC)

/// \brief helps in creating and throwing an ErrorException using boost::throw_exception
/// that has various type of data to include with the exception
///
/// \note
/// \li throw keyword is part of the macro, so do not add it
/// \li places the throw location (THROW_LOC) before the exception message
/// \li exception messgage must be inside parentheses
///
/// e.g.
/// \code
/// BOOST_AZURE_RESYNCREQUIRED_EXCEPTION("error occured: " << errno);
/// \endcode
#define BOOST_AZURE_RESYNCREQUIRED_EXCEPTION(xErrExpWhat) boost::throw_exception(AZURE_RESYNCREQUIRED_EXCEPTION << xErrExpWhat)


/// \brief helps in creating and throwing an ErrorException
/// that has various type of data to include with the exception
///
/// \note
/// \li throw keyword is part of the macro, so do not add it
/// \li places the throw location (THROW_LOC) after the exception message
/// \li exception messgage must be inside parentheses
/// 
/// e.g.
/// \code
/// THROW_AZURE_RESYNCREQUIRED_EXCEPTION("error occured: " << errno);
/// \endcode
#define THROW_AZURE_RESYNCREQUIRED_EXCEPTION(xErrExpWhat) throw AzureResyncRequiredException() << xErrExpWhat << '\t' << THROW_LOC_NO_TAB

/// \brief helps in creating and throwing an ErrorException using boost::throw_exception
/// that has various type of data to include with the exception
///
/// \note
/// \li throw keyword is part of the macro, so do not add it
/// \li places the throw location (THROW_LOC) after the exception message
/// \li exception messgage must be inside parentheses
///
/// e.g.
/// \code
/// BOOST_THROW_AZURE_RESYNCREQUIRED_EXCEPTION("error occured: " << errno);
/// \endcode
#define BOOST_THROW_AZURE_RESYNCREQUIRED_EXCEPTION(boostErrExpWhat) boost::throw_exception(AzureResyncRequiredException() << boostErrExpWhat << '\t' << THROW_LOC_NO_TAB)

#endif // ERROREXCEPTION_H
