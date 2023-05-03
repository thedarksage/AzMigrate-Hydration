///
///  \file  CStorageOfflineMachineSnapshotProvider.h
///
///  \brief contains CStorageOfflineMachineSnapshotProvider class
///

#ifndef CSTORAGEOFFLINEMACHINESNAPSHOTPROVIDER_H
#define CSTORAGEOFFLINEMACHINESNAPSHOTPROVIDER_H

#include "boost/shared_ptr.hpp"

#include "IMachineSnapshotProvider.h"
#include "CSnapshot.h"
#include "ILogger.h"

/// \brief CStorageOfflineMachineSnapshotProvider class
class CStorageOfflineMachineSnapshotProvider : public IMachineSnapshotProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CStorageOfflineMachineSnapshotProvider> Ptr; ///< Pointer type

	CStorageOfflineMachineSnapshotProvider(IMachine::Ptr pIMachine, ///< Machine
		ILoggerPtr pILogger)                                        ///< logger provider
		: IMachineSnapshotProvider(pIMachine),
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

		// Get the storage objects from machine
		IStorageObjects objs = m_pIMachine->GetStorageObjects();

		// Offline each of storage object
		IStoragePtr pStorage;
		bool isReadonly = true;
		for (IStorageObjectsIter it = objs.begin(); it != objs.end(); it++) {
			pStorage = *it;
			m_pILogger->LogInfo("Offlining storage object %S in read only mode.\n", pStorage->GetDeviceId());
			if (pStorage->Offline(isReadonly))
				m_pILogger->LogInfo("Offlined.\n");
			else {
				m_pILogger->Flush();
				// throw
				throw ERROR_EXCEPTION << "Failed to readonly offline storage object " << pStorage->GetDeviceId();
			}
		}

		m_pILogger->LogInfo("EXITED %s.\n", __FUNCTION__);
		m_pILogger->Flush();

		ISnapshot::Ptr pISnapshot(new CSnapshot(objs));
		return pISnapshot;
	}

	/// \brief Deletes a snapshot
	///
	/// throws std::exception
	void DeleteSnapshot(ISnapshot::Ptr pISnapshot) ///< Snapshot to delete
	{
		m_pILogger->LogInfo("ENTERED %s.\n", __FUNCTION__);

		// Get the storage objects from machine
		IStorageObjects objs = pISnapshot->GetStorageObjects();

		// Offline each of storage object
		IStoragePtr pStorage;
		bool isReadonly = false;
		for (IStorageObjectsIter it = objs.begin(); it != objs.end(); it++) {
			pStorage = *it;
			m_pILogger->LogInfo("Onlining storage object %S.\n", pStorage->GetDeviceId());
			if (pStorage->Online(isReadonly))
				m_pILogger->LogInfo("Onlined.\n");
			else {
				m_pILogger->Flush();
				// throw
				throw ERROR_EXCEPTION << "Failed to online storage object " << pStorage->GetDeviceId();
			}
		}

		m_pILogger->LogInfo("EXITED %s.\n", __FUNCTION__);
		m_pILogger->Flush();
	}

private:
	ILoggerPtr m_pILogger;             ///< logger
};

#endif /* CSTORAGEOFFLINEMACHINESNAPSHOTPROVIDER_H */
