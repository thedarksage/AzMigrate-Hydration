///
///  \file  CSnapshot.h
///
///  \brief contains CSnapshot class
///

#ifndef CSNAPSHOT_H
#define CSNAPSHOT_H

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "ISnapshot.h"

/// \brief CSnapshot class
class CSnapshot : public ISnapshot
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CSnapshot> Ptr; ///< Pointer type

	/// \Constructor
	CSnapshot(const IStorageObjects  &objs) ///< snapshot storage objects 
		: m_StorageObjects(objs)
	{
	}

	/// \brief Returns storage objects
	virtual IStorageObjects GetStorageObjects(void)
	{
		return m_StorageObjects;
	}

	/// \brief destructor
	virtual ~CSnapshot() {}

protected:
	IStorageObjects m_StorageObjects; ///< storage objects
};

#endif /* CSNAPSHOT_H */
