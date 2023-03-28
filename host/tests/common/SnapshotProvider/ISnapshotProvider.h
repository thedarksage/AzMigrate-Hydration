///
///  \file  ISnapshotProvider.h
///
///  \brief contains ISnapshotProvider interface
///

#ifndef ISNAPSHOTPROVIDER_H
#define ISNAPSHOTPROVIDER_H

#include "boost/shared_ptr.hpp"

#include "ISnapshot.h"
#include "IConsistencyPoint.h"

/// \brief ISnapshotProvider interface
class ISnapshotProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<ISnapshotProvider> Ptr; ///< Pointer type

	/// \brief Creates snapshot
	///
	/// \return snapshot
	/// \li \c throws std::exception
	virtual ISnapshot::Ptr CreateSnapshot(IConsistencyPoint::Ptr pIConsistencyPoint) = 0; ///< Consistency point for snapshot

	/// \brief Deletes a snapshot
	///
	/// throws std::exception
	virtual void DeleteSnapshot(ISnapshot::Ptr pISnapshot) = 0; ///< snapshot to delete

	/// \brief destructor
	virtual ~ISnapshotProvider() {}
};

#endif /* ISNAPSHOTPROVIDER_H */
