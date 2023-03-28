
///
///  \file azuremissingexception.h
///
///  \brief an error exception used to throw errors when encourtering a missing diff file on MT
///
/// \note
/// \li \c not all io manipulators will work. in particular std::endl, std::flush. But those  should not be needed in this case
/// \li \c most formatting manipulators do work which might be usefull
/// (std::hex, std::dec, std::setw, std::setfill, std::left, std::right, others)
///

#ifndef AZUREMISSINGFILEEXCEPTION_H
#define AZUREMISSINGFILEEXCEPTION_H

#include <stdexcept>
#include <string>
#include <sstream>

#include <boost/throw_exception.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "atloc.h"
#include "errorexception.h"

/// \brief for throwing exceptions
class AzureMissingFileException : public ErrorException {
public:
	/// \brief constructor
	///
	/// \param data to include with the exception
	explicit AzureMissingFileException(std::string const& data = std::string())
		:ErrorException(data)
	{
	}

	AzureMissingFileException(AzureMissingFileException const& ec)
		: ErrorException(ec)
	{
	}

	/// \brief destructor
	virtual ~AzureMissingFileException() throw()
	{ }

	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	AzureMissingFileException& operator<<(DATA_TYPE const& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}


	/// \brief used to add data using iostream style syntax
	template<typename DATA_TYPE>
	AzureMissingFileException& operator<<(DATA_TYPE& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	AzureMissingFileException& operator<<(std::wstring const& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wstring data using iostream style syntax
	AzureMissingFileException& operator<<(std::wstring& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t* data using iostream style syntax
	AzureMissingFileException& operator<<(wchar_t const*& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t* data using iostream style syntax
	AzureMissingFileException& operator<<(wchar_t *& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	AzureMissingFileException& operator<<(wchar_t const& data) ///< additional data to add
	{
		ErrorException::operator<<(data);
		return *this;
	}

	/// \brief overload used to add wchar_t data using iostream style syntax
	AzureMissingFileException& operator<<(wchar_t& data) ///< additional data to add
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
/// throw AZURE_MISSINGFILE_EXCEPTION << "error occured: " << errno;
/// \endcode
#define AZURE_MISSINGFILE_EXCEPTION  AzureMissingFileException(AT_LOC)

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
/// BOOST_AZURE_MISSINGFILE_EXCEPTION("error occured: " << errno);
/// \endcode
#define BOOST_AZURE_MISSINGFILE_EXCEPTION(xErrExpWhat) boost::throw_exception(AZURE_MISSINGFILE_EXCEPTION << xErrExpWhat)


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
/// THROW_AZURE_MISSINGFILE_EXCEPTION("error occured: " << errno);
/// \endcode
#define THROW_AZURE_MISSINGFILE_EXCEPTION(xErrExpWhat) throw AzureMissingFileException() << xErrExpWhat << '\t' << THROW_LOC_NO_TAB

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
/// BOOST_THROW_AZURE_MISSINGFILE_EXCEPTION("error occured: " << errno);
/// \endcode
#define BOOST_THROW_AZURE_MISSINGFILE_EXCEPTION(boostErrExpWhat) boost::throw_exception(AzureMissingFileException() << boostErrExpWhat << '\t' << THROW_LOC_NO_TAB)

#endif // ERROREXCEPTION_H
