///
///  \file  CTargetSnapshotProvider.h
///
///  \brief contains CTargetSnapshotProvider class
///

#ifndef CTARGETSNAPSHOTPROVIDER_H
#define CTARGETSNAPSHOTPROVIDER_H

#include <map>
#include <fstream>

#include "boost/shared_ptr.hpp"
#include "boost/thread/thread.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/spirit/include/classic.hpp"
#include "boost/spirit/include/classic_core.hpp"
#include "boost/spirit/include/phoenix1_functions.hpp"
#include "boost/spirit/include/phoenix1_primitives.hpp"

#include "IMachineSnapshotProvider.h"
#include "CSnapshot.h"
#include "IConsistencyPoint.h"
#include "IProcessProvider.h"
#include "CTargetSnapshotStorage.h"
#include "ILogger.h"

#include "errorexception.h"

using namespace BOOST_SPIRIT_CLASSIC_NS;

/// \brief CTargetSnapshotProvider class
class CTargetSnapshotProvider : public IMachineSnapshotProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CTargetSnapshotProvider> Ptr; ///< Pointer type

	/// \brief source storage object to target snapshot storage object map
	typedef std::map<std::string, std::string> SourceToTargetStorageMap;

	/// \brief const iterator for SourceToTargetStorageMap
	typedef SourceToTargetStorageMap::const_iterator ConstSourceToTargetStorageMapIter;

	CTargetSnapshotProvider(IMachine::Ptr pIMachine, ///< Source Machine
		IProcessProvider::Ptr pIProcessProvider,     ///< Process provider to query for target snapshot
		const std::string &dirStore,                 ///< directory store to keep snapshot related files
		ILoggerPtr pILogger                          ///< logger provider
		)                        
		: IMachineSnapshotProvider(pIMachine),
		m_pIProcessProvider(pIProcessProvider),
		m_DirStore(dirStore),
		m_pILogger(pILogger)
	{
	}

	/// \brief Creates snapshot
	///
	/// \return snapshot
	/// \li \c throws std::exception
	ISnapshot::Ptr CreateSnapshot(IConsistencyPoint::Ptr pIConsistencyPoint) ///< Consistency point for snapshot
	{
		m_pILogger->LogInfo("ENTERED %s.\n", __FUNCTION__);

		// Prepare the snapshot file which will have mapping from source storage objects to snapshot storage objects
		std::string snapshotMapFile = m_DirStore;
		snapshotMapFile += "/TargetSnapshot/";

		boost::filesystem::path dirPath(snapshotMapFile);
		if (!boost::filesystem::exists(dirPath))
			boost::filesystem::create_directories(dirPath);

		snapshotMapFile += "SnapshotMapFile.txt";

		// Query target snapshots for given machine ID and consistency point ID
		// Retry based on below values:
		const unsigned RETRY_INTERVAL_IN_SECONDS = 120;
		const unsigned TIME_OUT_IN_SECONDS = 3600;
		unsigned secondsRetriedSoFar = 0;
		SourceToTargetStorageMap stmap;
		do {
			// The snapshotMapFile is overwritten for subsequent retry by HvrConsole.exe
			stmap = GetSourceToTargetStorageMap(m_pIMachine->GetID(), pIConsistencyPoint->GetID(), snapshotMapFile);
			if (stmap.empty()) {
				m_pILogger->LogInfo("Did not get target snapshot for machine ID %s, consistency point ID %s. Retrying after %u seconds...\n",
					m_pIMachine->GetID().c_str(), pIConsistencyPoint->GetID().c_str(), RETRY_INTERVAL_IN_SECONDS);
				boost::this_thread::sleep(boost::posix_time::seconds(RETRY_INTERVAL_IN_SECONDS));
				secondsRetriedSoFar += RETRY_INTERVAL_IN_SECONDS;
				if (secondsRetriedSoFar >= TIME_OUT_IN_SECONDS) {
					// Retries are over. Throw exception.
					throw ERROR_EXCEPTION << "Could not find target snapshot for machine ID " << m_pIMachine->GetID() << ", consistency point ID " << pIConsistencyPoint->GetID()
						<< ". Even after waiting for " << TIME_OUT_IN_SECONDS << " seconds, after consistency point creation.";
				}
			}
			else
				break;
		} while (true);

		// Get the target storage objects only for source storage objects
		IStorageObjects targetObjs;
		IStorageObjects mObjs = m_pIMachine->GetStorageObjects();
		IStoragePtr pMStorage;
		for (IStorageObjectsIter msit = mObjs.begin(); msit != mObjs.end(); msit++) {
			pMStorage = *msit;
            std::string msid(pMStorage->GetDeviceId());
			m_pILogger->LogInfo("Finding target snapshot blob for source storage %s.\n", msid.c_str());
			ConstSourceToTargetStorageMapIter it = stmap.find(msid);
			if (it == stmap.end())
				throw ERROR_EXCEPTION << "Could not find target snapshot storage object for source storage object ID " << msid;
			else {
				const std::string &tsid = it->second;
				// Create the blob object
				m_pILogger->LogInfo("Target snapshot storage ID is %s in target snapshot for source storage %s.\n", tsid.c_str(), msid.c_str());
				IStoragePtr pTStorage(new CTargetSnapshotStorage(tsid));
				targetObjs.push_back(pTStorage);
			}
		}

		m_pILogger->LogInfo("EXITED %s.\n", __FUNCTION__);

		ISnapshot::Ptr pSnapshot(new CSnapshot(targetObjs));
		return pSnapshot;
	}

	/// \brief Deletes a snapshot
	///
	/// throws std::exception
	void DeleteSnapshot(ISnapshot::Ptr pISnapshot) ///< Snapshot to delete
	{
		// Target blob snapshots need not be explicitly deleted
		throw ERROR_EXCEPTION << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << ": Not implemented";
	}

private:
	SourceToTargetStorageMap GetSourceToTargetStorageMap(const std::string &machineID, ///< machine ID
		const std::string &consistencyPointID,                                         ///< consistency point ID
		const std::string &snapshotMapFile                                             ///< snapshot map file      
		)
	{
		// Fork command and parse to get snapshot storage IDs
		SourceToTargetStorageMap m;
		const std::string CMD = "C:\\HvrConsole\\HvrConsole.exe";
		const std::string GET_MARKER_SAS_URI_OPTION = "/GetMarkerSnapshotSasUri";
		const std::string VMID_OPTION = "/VmId:";
		const std::string MARKER_ID_OPTION = "/MarkerId:";
		const std::string OUTPUT_FILE_OPTION = "/OutputFile:";

		// vmid argument
		std::string vmIdArg = VMID_OPTION;
		vmIdArg += machineID;

		// marker id argument
		std::string markerIdArg = MARKER_ID_OPTION;
		markerIdArg += consistencyPointID;

		// output file argument
		std::string outputFileArg = OUTPUT_FILE_OPTION;
		outputFileArg += snapshotMapFile;

		// prepare args
		IProcessProvider::ProcessArguments_t args;
		args.push_back(GET_MARKER_SAS_URI_OPTION);
		args.push_back(vmIdArg);
		args.push_back(markerIdArg);
		args.push_back(outputFileArg);

		std::string output, error;
		int exitStatus;
		// already this throws
		m_pIProcessProvider->RunSyncProcess(CMD, args, output, error, exitStatus);
		if (exitStatus != 0) {
			m_pILogger->LogInfo("%s command exit status is not zero.\n", CMD.c_str());
			return m;
		}

		const char SEP = ',';
		// Do not check exit status but rely on output file
		std::ifstream is;
		is.open(snapshotMapFile.c_str());
		if (!is.is_open()) {
			m_pILogger->LogInfo("File %s did not open. It may not be created yet.\n", snapshotMapFile.c_str());
			return m;
		}

		while (!is.eof()) {
			std::string line;
			std::getline(is, line); //Get each line
			if (line.empty())
				break;
			std::string s, t;
			// format is source storage, target snapshot
			bool parsed = parse(
				line.begin(),
				line.end(),
				lexeme_d
				[
					(+(~ch_p(SEP)))[phoenix::var(s) = phoenix::construct_<std::string>(phoenix::arg1, phoenix::arg2)]
					>> SEP >>
					(+anychar_p)[phoenix::var(t) = phoenix::construct_<std::string>(phoenix::arg1, phoenix::arg2)]
				] >> end_p,
				space_p).full;
			if (parsed) {
				m.insert(std::make_pair(s, t));
				m_pILogger->LogInfo("Source %s has target snapshot %s.\n", s.c_str(), t.c_str());
			}
		}

		return m;
	}

private:
	IProcessProvider::Ptr m_pIProcessProvider;   ///< Process provider to query for target snapshot

	std::string m_DirStore;                      ///< directory store to keep snapshot related files

	ILoggerPtr m_pILogger;                       ///< logger
};

#endif /* CTARGETSNAPSHOTPROVIDER_H */
