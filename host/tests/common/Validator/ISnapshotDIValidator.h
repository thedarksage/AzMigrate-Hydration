///
///  \file  ISnapshotDIValidator.h
///
///  \brief contains  ISnapshotDIValidator interface
///

#ifndef ISNAPSHOTDIVALIDATOR_H
#define ISNAPSHOTDIVALIDATOR_H

#include "boost/shared_ptr.hpp"

#include "ISnapshot.h"

/// \brief ISnapshotDIValidator interface
class ISnapshotDIValidator
{
public:
	/// \brief Pointer type
    typedef boost::shared_ptr<ISnapshotDIValidator> Ptr;

	/// \brief validates
	///
	/// \return
	/// \li \c true if validation was successful
	/// \li \c false if validation failed
	virtual bool Validate(ISnapshot::Ptr pSourceISnapshot, ///< source snapshot
		                  ISnapshot::Ptr pTargetISnapshot  ///< target snapshot
				          ) = 0;
};

#endif /* ISNAPSHOTDIVALIDATOR_H */
