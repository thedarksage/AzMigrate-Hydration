///
///  \file  CTargetSnapshotStorageMD5ChecksumProvider.h
///
///  \brief contains CTargetSnapshotStorageMD5ChecksumProvider class
///

#ifndef CTARGETSNAPSHOTSTORAGEMD5CHECKSUMPROVIDER_H
#define CTARGETSNAPSHOTSTORAGEMD5CHECKSUMPROVIDER_H

#include <string>
#include <sstream>
#include <fstream>

#include "boost/shared_ptr.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/thread/thread.hpp"
#include "boost/spirit/include/classic.hpp"
#include "boost/spirit/include/classic_core.hpp"
#include "boost/spirit/include/phoenix1_functions.hpp"
#include "boost/spirit/include/phoenix1_primitives.hpp"

#include "IChecksumProvider.h"
#include "ILogger.h"
#include "CTargetSnapshotStorage.h"
#include "IProcessProvider.h"
#include "errorexception.h"

using namespace BOOST_SPIRIT_CLASSIC_NS;

/// \brief CTargetSnapshotStorageMD5ChecksumProvider Class
class CTargetSnapshotStorageMD5ChecksumProvider : public IChecksumProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CTargetSnapshotStorageMD5ChecksumProvider> Ptr; ///< Pointer type

	/// \brief Constructor
	CTargetSnapshotStorageMD5ChecksumProvider(IStoragePtr pCTargetSnapshotStorage, ///< storage
		IProcessProvider::Ptr pIProcessProvider                                    ///< Process provider to query for target snapshot storage's checksum
		) :
		m_pCTargetSnapshotStorage(pCTargetSnapshotStorage),
		m_pIProcessProvider(pIProcessProvider)
	{
	}

	/// \brief Returns checksum for given offset and length
	/// 
	/// throws std::exception
	ChecksumHashPtr GetChecksum(const unsigned long long &offset, ///< offset
		const unsigned &length                                    ///< length
		)
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

	/// \brief Places complete checksums, in units of chunkSize, into file
	///
	/// throws std::exception
	void GetChecksumsInFile(const unsigned &chunkSize, ///< chunk size
		const std::string &file,                       ///< file into which checksums are placed
		const bool &formatHashAsText,                  ///< boolean requesting hash to be formatted as text, separated by newline for each chunk
		const bool &separateHashesByNewline            ///< Applies only when formatHashAsText is true. Writes checksums in text form with each block's checksum separated by a newline
		)
	{
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

	/// \brief Places checksums, in units of chunkSize, for given offset and length into a file
	///
	/// throws std::exception
	void GetChecksumsInFile(const unsigned long long &offset, ///< offset
		const unsigned long long &length,                     ///< length
		const unsigned &chunkSize,                            ///< chunk size
		const std::string &file,                              ///< file into which checksums are placed
		const bool &formatHashAsText,                         ///< boolean requesting hash to be formatted as text, separated by newline for each chunk
		const bool &separateHashesByNewline                   ///< Applies only when formatHashAsText is true. Writes checksums in text form with each block's checksum separated by a newline
		)
	{
		// Check parameters
		if (!formatHashAsText)
			throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Non text hash format is not supported"; // no support for non text hash format

		if (!separateHashesByNewline)
			throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": No support for text has format without newline hash separator"; //newline separator is a must

		// Fork command to get checksums to file
		const std::string CMD = "C:\\HvrConsole\\HvrConsole.exe";
		const std::string GET_CHECKSUM_OPTION = "/GetChecksum";
		const std::string BLOB_URI_OPTION = "/BlobUri:";
		const std::string SAS_TOKEN_ARG = "/SasToken:";
		const std::string DATA_LENGTH_ARG = "/DataLength:";
		const std::string BLOCK_SIZE_OPTION = "/BlockSize:";
		const std::string OUTPUT_FILE_OPTION = "/OutputFile:";

        std::string sTargetStorageID(m_pCTargetSnapshotStorage->GetDeviceId());

		// break the total target storage ID into BlobUri and SasToken
		// The target storage ID is of form BlobUri?SasToken
		std::string blobUri, sasToken;
		const char SEP = '?';
		bool parsed = parse(
			sTargetStorageID.begin(),
			sTargetStorageID.end(),
			lexeme_d
			[
				(+(~ch_p(SEP)))[phoenix::var(blobUri) = phoenix::construct_<std::string>(phoenix::arg1, phoenix::arg2)]
				>> SEP >>
				(+anychar_p)[phoenix::var(sasToken) = phoenix::construct_<std::string>(phoenix::arg1, phoenix::arg2)]
			] >> end_p,
			space_p).full;

		if (!parsed)
			throw ERROR_EXCEPTION << "Target storage ID " << sTargetStorageID << " does not have blobUri, sasToken";

		if (blobUri.empty())
			throw ERROR_EXCEPTION << "Target storage ID " << sTargetStorageID << " does not have blobUri";

		if (sasToken.empty())
			throw ERROR_EXCEPTION << "Target storage ID " << sTargetStorageID << " does not have sasToken";

		// blob uri argument
		std::string blobUriArg = BLOB_URI_OPTION;
		blobUriArg += '\"';
		blobUriArg += blobUri;
		blobUriArg += '\"';

		// sas token argument
		std::string sasTokenArg = SAS_TOKEN_ARG;
		sasTokenArg += '\"';
		sasTokenArg += sasToken;
		sasTokenArg += '\"';

		// data length argument
		std::string dataLengthArg = DATA_LENGTH_ARG;
		dataLengthArg += boost::lexical_cast<std::string>(length);

		// block size argument (This is in terms of MB)
		if (chunkSize % 1048576)
			throw ERROR_EXCEPTION << "ChunkSize " << chunkSize << " has to be multiple of 1MB";

		std::string blockSizeArg = BLOCK_SIZE_OPTION;
		unsigned argChunkSize = (chunkSize / 1048576);
		blockSizeArg += boost::lexical_cast<std::string>(argChunkSize);

		// output file argument
		std::string outputFileArg = OUTPUT_FILE_OPTION;
		outputFileArg += file;

		// prepare args
		IProcessProvider::ProcessArguments_t args;
		args.push_back(GET_CHECKSUM_OPTION);
		args.push_back(blobUriArg);
		args.push_back(sasTokenArg);
		args.push_back(dataLengthArg);
		args.push_back(blockSizeArg);
		args.push_back(outputFileArg);

		// get the checksums
		const unsigned MAX_RETRIES = 3;
		unsigned retryIteration = 0;
		do {
			std::string output, error;
			int exitStatus;
			
			// Fork and check the exit status
			m_pIProcessProvider->RunSyncProcess(CMD, args, output, error, exitStatus);
			if (exitStatus == 0)
				break;
			
			boost::this_thread::sleep(boost::posix_time::seconds(1));

			retryIteration++;

		} while (retryIteration < MAX_RETRIES);
		
		if (retryIteration == MAX_RETRIES) {
			throw ERROR_EXCEPTION << "Failed to get checksum from " << CMD
				<< ", GET_CHECKSUM_OPTION " << GET_CHECKSUM_OPTION
				<< ", blobUriArg " << blobUriArg
				<< ", sasTokenArg " << sasTokenArg
				<< ", dataLengthArg " << dataLengthArg
				<< ", blockSizeArg " << blockSizeArg
				<< ", outputFileArg " << outputFileArg;
		}
	}

private:
	IStoragePtr m_pCTargetSnapshotStorage;     ///< storage

	IProcessProvider::Ptr m_pIProcessProvider; ///< Process provider
};

#endif /* CTARGETSNAPSHOTSTORAGEMD5CHECKSUMPROVIDER_H */
