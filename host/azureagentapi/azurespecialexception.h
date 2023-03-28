
///
///  \file azurespeciaexception.h
///
///  \brief an error exception used to throw errors when encountering azure side error which may need exponential backoff delay for next attempt
///
/// \note
/// \li \c not all io manipulators will work. in particular std::endl, std::flush. But those  should not be needed in this case
/// \li \c most formatting manipulators do work which might be usefull
/// (std::hex, std::dec, std::setw, std::setfill, std::left, std::right, others)
///

#ifndef AZURESPECIALEXCEPTION_H
#define AZURESPECIALEXCEPTION_H

#include <stdexcept>
#include <string>
#include <sstream>

#include <boost/throw_exception.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "atloc.h"
#include "errorexception.h"

/// \brief for throwing exceptions
class AzureSpecialException : public ErrorException {
public:
	/// \brief constructor
	///
	/// \param data to include with the exception
	explicit AzureSpecialException(std::string const& data, long errorCode)
		:ErrorException(data)
	{
		m_errorCode = errorCode;
	}

	AzureSpecialException(AzureSpecialException const& ec)
		: ErrorException(ec)
	{
		m_errorCode = ec.m_errorCode;
	}

	/// \brief destructor
	virtual ~AzureSpecialException() throw()
	{ }

	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	AzureSpecialException& operator<<(DATA_TYPE const& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}


	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	AzureSpecialException& operator<<(DATA_TYPE& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	AzureSpecialException& operator<<(std::wstring const& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	AzureSpecialException& operator<<(std::wstring& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t* data using iostream style syntax
	AzureSpecialException& operator<<(wchar_t const*& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t* data using iostream style syntax
	AzureSpecialException& operator<<(wchar_t *& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	AzureSpecialException& operator<<(wchar_t const& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	AzureSpecialException& operator<<(wchar_t& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	long ErrorCode() const { return m_errorCode; }

protected:

private:
	long m_errorCode;
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
/// throw AZURE_SPECIAL_EXCEPTION << "error occured: " << errno;
/// \endcode
#define AZURE_SPECIAL_EXCEPTION(errorCode)  AzureSpecialException(AT_LOC,errorCode)

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
/// BOOST_AZURE_SPECIAL_EXCEPTION("error occured: " << errno);
/// \endcode
#define BOOST_AZURE_SPECIAL_EXCEPTION(errorCode,xErrExpWhat) boost::throw_exception(AZURE_SPECIAL_EXCEPTION(errorCode) << xErrExpWhat)


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
/// THROW_AZURE_SPECIAL_EXCEPTION("error occured: " << errno);
/// \endcode
#define THROW_AZURE_SPECIAL_EXCEPTION(errorCode,xErrExpWhat) throw AzureSpecialException(std::string(), errorCode) << xErrExpWhat << '\t' << THROW_LOC_NO_TAB

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
/// BOOST_THROW_AZURE_SPECIAL_EXCEPTION("error occured: " << errno);
/// \endcode
#define BOOST_THROW_AZURE_SPECIAL_EXCEPTION(errorCode, boostErrExpWhat) boost::throw_exception(AzureSpecialException(std::string(), errorCode) << boostErrExpWhat << '\t' << THROW_LOC_NO_TAB)

#endif // ERROREXCEPTION_H