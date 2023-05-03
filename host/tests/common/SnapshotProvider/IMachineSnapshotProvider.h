///
///  \file  IMachineSnapshotProvider.h
///
///  \brief contains IMachineSnapshotProvider interface
///

#ifndef IMACHINESNAPSHOTPROVIDER_H
#define IMACHINESNAPSHOTPROVIDER_H

#include "boost/shared_ptr.hpp"

#include "ISnapshotProvider.h"
#include "IMachine.h"

/// \brief IMachineSnapshotProvider interface
class IMachineSnapshotProvider : public ISnapshotProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<IMachineSnapshotProvider> Ptr; ///< Pointer type

	/// \brief Constructor
	IMachineSnapshotProvider(IMachine::Ptr pIMachine)        ///< Machine for which snapshot is provided
		: m_pIMachine(pIMachine)
	{
	}

	/// \brief Creates snapshot
	///
	/// \return snapshot
	/// \li \c throws std::exception
	virtual ISnapshot::Ptr CreateSnapshot(IConsistencyPoint::Ptr pIConsistencyPoint) = 0; ///< Consistency point for snapshot

	/// \brief Deletes a snapshot
	///
	/// throws std::exception
	virtual void DeleteSnapshot(ISnapshot::Ptr pISnapshot) = 0; ///< Snapshot to delete

	/// \brief destructor
	virtual ~IMachineSnapshotProvider() {}

protected:
	IMachine::Ptr m_pIMachine; ///< Machine
};

#endif /* IMACHINESNAPSHOTPROVIDER_H */
