///
///  \file  CISnapshotDIValidator.h
///
///  \brief contains  CISnapshotDIValidator class
///

#ifndef CISNAPSHOTDIVALIDATOR_H
#define CISNAPSHOTDIVALIDATOR_H

#include <string>
#include <sstream>
#include <vector>
#include <list>

#include "boost/ref.hpp"
#include "boost/bind.hpp"
#include "boost/thread.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"

#include "ISnapshotDIValidator.h"
#include "ISnapshot.h"
#include "ILogger.h"
#include "IStorageDIValidatorChecksumProviderFactory.h"
#include "CIStorageDIValidator.h"

/// \brief CISnapshotDIValidator class
class CISnapshotDIValidator : public ISnapshotDIValidator
{
public:
    /// \brief Pointer type
    typedef boost::shared_ptr<CISnapshotDIValidator> Ptr;

	///< Constructor
	CISnapshotDIValidator(
		IStorageDIValidatorChecksumProviderFactory::Ptr pIChecksumFactory, ///< checksum factory
		const std::string &dirStore,                                       ///< base directory for storing source and target checksums in respective folders.
		const unsigned &chunkSize,                                         ///< checksum chunkSize
		ILoggerPtr pILogger                                                ///< logger
		) :
		m_pIChecksumFactory(pIChecksumFactory),
		m_DirStore(dirStore),
		m_ChunkSize(chunkSize),
		m_pILogger(pILogger)
	{
	}

	/// \brief validates
	///
	/// \return
	/// \li \c true if validation was successful
	/// \li \c false if validation failed
	bool Validate(ISnapshot::Ptr pSourceISnapshot, ///< source snapshot
		          ISnapshot::Ptr pTargetISnapshot  ///< target snapshot
				  )
	{
		m_pILogger->LogInfo("ENTERED %s.\n", __FUNCTION__);
		bool isValid = false;

		IStorageObjects iSourceStorageObjects = pSourceISnapshot->GetStorageObjects();
		IStorageObjects iTargetStorageObjects = pTargetISnapshot->GetStorageObjects();

		if (iSourceStorageObjects.size() == 0)
			throw ERROR_EXCEPTION << "Source snapshot has zero storage objects.";

		if (iTargetStorageObjects.size() == 0)
			throw ERROR_EXCEPTION << "Target snapshot has zero storage objects.";

		// If number of storage objects do not match, return false
		if (iSourceStorageObjects.size() != iTargetStorageObjects.size()) {
			std::stringstream ss;
			ss << "Number of source storage objects " << iSourceStorageObjects.size()
				<< ", number of target storage objects " << iTargetStorageObjects.size();
			throw ERROR_EXCEPTION << "Source and target snapshots have unequal number of storage objects. " << ss.str();
		}

		// Form the source and target checksum files' directories
		std::string sourceCSDir, targetCSDir;
		sourceCSDir = m_DirStore;

		sourceCSDir += "/checksum/"; // '/' Works for windows too as per MSDN for file I/O functions
		targetCSDir = sourceCSDir;

		sourceCSDir += "source";
		targetCSDir += "target";

		boost::filesystem::path sourceCSDirPath(sourceCSDir);
		if (!boost::filesystem::exists(sourceCSDirPath))
			boost::filesystem::create_directories(sourceCSDirPath);

		boost::filesystem::path targetCSDirPath(targetCSDir);
		if (!boost::filesystem::exists(targetCSDirPath))
			boost::filesystem::create_directories(targetCSDirPath);
		
		// For all storage object pairs except first one, fork the validator thread. 
		// Validation for first storage object pair is handled by this thread.
		std::list<bool> statuses; // Using list instead of vector because vector<bool> is bitarray
		std::vector<std::string> errorMessages;
		std::vector <boost::shared_ptr<boost::thread> > ts;

		IStorageObjectsIter sIt = iSourceStorageObjects.begin()+1;
		IStorageObjectsIter tgIt = iTargetStorageObjects.begin()+1;
		for ( /* empty */; sIt != iSourceStorageObjects.end(); sIt++, tgIt++) {
			std::list<bool>::iterator bSt = statuses.insert(statuses.end(), false);
			std::vector<std::string>::iterator sErr = errorMessages.insert(errorMessages.end(), std::string());
			bool &status = *bSt;
			std::string &errMsg = *sErr;
			boost::shared_ptr<boost::thread> pT(new boost::thread(
				boost::bind(&CISnapshotDIValidator::GetIStorageDIValidityStatus, this, // call this' GetTargetChecksums in another thread
				*sIt, *tgIt, boost::cref(sourceCSDir), boost::cref(targetCSDir), boost::ref(status), boost::ref(errMsg)))); //arguments
			ts.push_back(pT);
		}

		// Do validation for first storage object pair
		IStoragePtr s = *(iSourceStorageObjects.begin());
		IStoragePtr t = *(iTargetStorageObjects.begin());
		CIStorageDIValidator v(m_pIChecksumFactory, sourceCSDir, targetCSDir, m_ChunkSize, m_pILogger);
		bool isFirstPairValid = v.Validate(s, t);
				
		// Join threads
		std::vector <boost::shared_ptr<boost::thread> >::iterator it = ts.begin();
		for ( /* empty */ ; it != ts.end(); it++) {
			boost::shared_ptr<boost::thread> &t = *it;
			t->join();
		}

		isValid = isFirstPairValid;

		if (isValid) {
			// Look at other statuses as first status was successful
			std::list<bool>::iterator bIt = statuses.begin();
			std::vector<std::string>::iterator eIt = errorMessages.begin();
			for ( /* empty */; bIt != statuses.end(); bIt++, eIt++) {
				const bool &b = *bIt;
				if (!b) {
					isValid = false;
					break;
				}
			}
		}

		m_pILogger->LogInfo("EXITED %s.\n", __FUNCTION__);
		return isValid;
	}

private:
	/// \brief DI validity status for given source and target storage
	///
	/// \note: This method executes in a thread and hence should not throw any exceptions.
	void GetIStorageDIValidityStatus(IStoragePtr pISourceStorage, ///< source storage
		IStoragePtr pITargetStorage,                              ///< target storage
		const std::string &sourceCSDir,                           ///< source checksum directory
		const std::string &targetCSDir,                           ///< target checksum directory
		bool &status,                                             ///< status: true / false
		std::string &errMsg                                       ///< error message in case of failure
		)
	{
		status = false;

		try {
			CIStorageDIValidator v(m_pIChecksumFactory, sourceCSDir, targetCSDir, m_ChunkSize, m_pILogger);
			status = v.Validate(pISourceStorage, pITargetStorage);
		}
		catch (std::exception &e) {
			errMsg = e.what();
		}
	}

private:
	IStorageDIValidatorChecksumProviderFactory::Ptr m_pIChecksumFactory; /// checksum factory

	std::string m_DirStore;                                              ///< checksums Dir

	unsigned m_ChunkSize;                                                ///< checksum chunk size 

	ILoggerPtr m_pILogger;                                                 ///< logger
};

#endif /* CISNAPSHOTDIVALIDATOR_H */
