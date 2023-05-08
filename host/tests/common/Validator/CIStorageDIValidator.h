///
///  \file  CIStorageDIValidator.h
///
///  \brief contains  CIStorageDIValidator class
///

#ifndef CISTORAGEDIVALIDATOR_H
#define CISTORAGEDIVALIDATOR_H

#include <string>
#include <sstream>
#include <ctime>

#include "boost/shared_ptr.hpp"
#include "boost/thread.hpp"
#include "boost/bind.hpp"
#include "boost/ref.hpp"

#include "IStorageDIValidator.h"
#include "IStorage.h"
#include "ILogger.h"
#include "IStorageDIValidatorChecksumProviderFactory.h"
#include "CFileDIValidator.h"

/// \brief CIStorageDIValidator class
class CIStorageDIValidator : public IStorageDIValidator
{
public:
    /// \brief Pointer type
    typedef boost::shared_ptr<CIStorageDIValidator> Ptr;

	///< Constructor
	CIStorageDIValidator(
		IStorageDIValidatorChecksumProviderFactory::Ptr pIChecksumFactory, ///< checksum factory
		const std::string &sourceCSDir,                                    ///< source checksums directory
		const std::string &targetCSDir,                                    ///< target checksums directory
		const unsigned &chunkSize,                                         ///< checksum chunkSize
		ILoggerPtr pILogger                                                ///< logger
		) :
		m_pIChecksumFactory(pIChecksumFactory),
		m_SourceCSDir(sourceCSDir),
		m_TargetCSDir(targetCSDir),
		m_ChunkSize(chunkSize),
		m_pILogger(pILogger)
	{
	}

	/// \brief validates
	///
	/// \return
	/// \li \c true if validation was successful
	/// \li \c false if validation failed
	bool Validate(IStoragePtr pSourceIStorage, ///< source snapshot
                  IStoragePtr pTargetIStorage  ///< target snapshot
				 )
                  
	{
		m_pILogger->LogInfo("ENTERED %s.\n", __FUNCTION__);

		// Prepare source and target checksum files
		std::string sourceCSFile = m_SourceCSDir;
		std::string targetCSFile = m_TargetCSDir;

		sourceCSFile += "/"; // '/' Works for windows too as per MSDN for file I/O functions
		targetCSFile += "/";

        std::string sSourceDeviceID(pSourceIStorage->GetDeviceId());

		// basenames
		sourceCSFile += sSourceDeviceID;
		targetCSFile += sSourceDeviceID; //store target's checksum as source basename

		sourceCSFile += ".txt";
		targetCSFile += ".txt";
		
		// Fork a thread for target checksum
		bool tStatus = false;
		std::string tErrMsg;
		boost::shared_ptr<boost::thread> pT(new boost::thread(
			boost::bind(&CIStorageDIValidator::GetTargetChecksums, this, // call this' GetTargetChecksums in another thread
			pTargetIStorage, pSourceIStorage, boost::cref(targetCSFile), boost::ref(tStatus), boost::ref(tErrMsg)))); //arguments

		// Get source checksum
		bool formatHashAsText = true;
		bool separateHashesByNewline = true;
		IChecksumProvider::Ptr sourceCSProvider = m_pIChecksumFactory->GetSourceStorageChecksumProvider(pSourceIStorage);
		sourceCSProvider->GetChecksumsInFile(m_ChunkSize, sourceCSFile, formatHashAsText, separateHashesByNewline);

		// Join target checksum thread
		pT->join();

		if (!tStatus)
			throw ERROR_EXCEPTION << "Failed to get checksums for target " << pTargetIStorage->GetDeviceId() << " with error message " << tErrMsg;

		// Compare files
		CFileDIValidator fdi(m_pILogger);
		bool isValid = fdi.Validate(sourceCSFile, targetCSFile);
		
		m_pILogger->LogInfo("EXITED %s.\n", __FUNCTION__);
		return isValid;
	}

private:
	/// \brief Gets target checksums
	///
	/// \note: This method executes in a thread and hence should not throw any exceptions.
	void GetTargetChecksums(IStoragePtr pTargetIStorage, ///< target storage
		IStoragePtr pSourceIStorage,                     ///< source storage
        const std::string &file,                         ///< checksums file
		bool &status,                                    ///< status: true / false
		std::string &errMsg                              ///< error message in case of failure
		)
	{
		status = false;
		bool formatHashAsText = true;
		bool separateHashesByNewline = true;
		try {
			// Get target checksums
			IChecksumProvider::Ptr targetCSProvider = m_pIChecksumFactory->GetTargetStorageChecksumProvider(pTargetIStorage);
			targetCSProvider->GetChecksumsInFile(0, pSourceIStorage->GetDeviceSize(), m_ChunkSize, file, formatHashAsText, separateHashesByNewline);
			status = true;
		}
		catch (std::exception &e) {
			errMsg = e.what();
		}
	}

private:
	IStorageDIValidatorChecksumProviderFactory::Ptr m_pIChecksumFactory; /// checksum factory

	std::string m_SourceCSDir;                                           ///< source checksums Dir

	std::string m_TargetCSDir;                                           ///< target checksums Dir

	unsigned m_ChunkSize;                                                ///< checksum chunk size 

	ILoggerPtr m_pILogger;                                                 ///< logger
};

#endif /* CISTORAGEDIVALIDATOR_H */
