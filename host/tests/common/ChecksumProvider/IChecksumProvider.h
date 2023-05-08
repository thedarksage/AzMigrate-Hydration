///
///  \file  IChecksumProvider.h
///
///  \brief contains IChecksumProvider interface
///

#ifndef ICHECKSUMPROVIDER_H
#define ICHECKSUMPROVIDER_H

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

/// \brief IChecksumProvider interface
class IChecksumProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<IChecksumProvider> Ptr; ///< Pointer type

	/// \brief Checksum hash type
	typedef std::vector<unsigned char> ChecksumHash; ///< checksum hash type

	/// \brief Checksum hash pointer
	typedef boost::shared_ptr<ChecksumHash> ChecksumHashPtr; ///< Checksum hash pointer

	/// \brief Returns checksum for given offset and length
	/// 
	/// throws std::exception
	virtual ChecksumHashPtr GetChecksum(const unsigned long long &offset, ///< offset
		const unsigned &length                                            ///< length
		) = 0;

	/// \brief Places complete checksums, in units of chunkSize, into file
	///
	/// throws std::exception
	virtual void GetChecksumsInFile(const unsigned &chunkSize, ///< chunk size
		const std::string &file,                               ///< file into which checksums are placed
		const bool &formatHashAsText,                          ///< boolean requesting hash to be formatted as text, separated by newline for each chunk
		const bool &separateHashesByNewline                    ///< Applies only when formatHashAsText is true. Writes checksums in text form with each block's checksum separated by a newline
		) = 0;

	/// \brief Places checksums, in units of chunkSize, for given offset and length into a file
	///
	/// throws std::exception
	virtual void GetChecksumsInFile(const unsigned long long &offset, ///< offset
		const unsigned long long &length,                             ///< length
		const unsigned &chunkSize,                                    ///< chunk size
		const std::string &file,                                      ///< file into which checksums are placed
		const bool &formatHashAsText,                                 ///< boolean requesting hash to be formatted as text, separated by newline for each chunk
		const bool &separateHashesByNewline                           ///< Applies only when formatHashAsText is true. Writes checksums in text form with each block's checksum separated by a newline
		) = 0;

	/// \brief destructor
	virtual ~IChecksumProvider() {}
};

#endif /* ICHECKSUMPROVIDER_H */