///
///  \file  ISnapshot.h
///
///  \brief contains ISnapshot interface
///

#ifndef ISNAPSHOT_H
#define ISNAPSHOT_H

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "IStorage.h"

/// \brief ISnapshot interface
class ISnapshot
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<ISnapshot> Ptr; ///< Pointer type

	/// \brief Returns storage objects
	virtual IStorageObjects GetStorageObjects(void) = 0;

	/// \brief destructor
	virtual ~ISnapshot() {}
};

#endif /* ISNAPSHOT_H */