///
///  \file  IStorage.h
///
///  \brief contains IStorage interface
///

#ifndef ISTORAGE_H
#define ISTORAGE_H

#include <vector>

#include "boost/shared_ptr.hpp"

#include "CDiskDevice.h"

/// \brief IStorage
typedef IBlockDevice IBlockStorage;

/// \brief IStorage Ptr type
typedef boost::shared_ptr<IBlockStorage> IStoragePtr;

/// \brief Collection of storage objects
typedef std::vector<IStoragePtr> IStorageObjects;

/// \brief Iterator for storage objects
typedef IStorageObjects::iterator IStorageObjectsIter;

#endif /* ISTORAGE_H */
