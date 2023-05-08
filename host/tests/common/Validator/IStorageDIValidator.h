///
///  \file  IStorageDIValidator.h
///
///  \brief contains  IStorageDIValidator interface
///

#ifndef ISTORAGEDIVALIDATOR_H
#define ISTORAGEDIVALIDATOR_H

#include "boost/shared_ptr.hpp"

#include "IStorage.h"

/// \brief IStorageDIValidator interface
class IStorageDIValidator
{
public:
    /// \brief Pointer type
    typedef boost::shared_ptr<IStorageDIValidator> Ptr;

	/// \brief validates
	///
	/// \return
	/// \li \c true if validation was successful
	/// \li \c false if validation failed
	virtual bool Validate(IStoragePtr pSourceIStorage, ///< source snapshot
                          IStoragePtr pTargetIStorage  ///< target snapshot
				         ) = 0;
};

#endif /* ISTORAGEDIVALIDATOR_H */
