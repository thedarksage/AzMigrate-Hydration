///
///  \file  IStorageDIValidatorChecksumProviderFactory.h
///
///  \brief contains  interface
///

#ifndef ISTORAGEDIVALIDATORCHECKSUMPROVIDERFACTORY_H
#define ISTORAGEDIVALIDATORCHECKSUMPROVIDERFACTORY_H

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "IStorage.h"
#include "IChecksumProvider.h"

/// \brief IStorageDIValidatorChecksumProviderFactory interface
class IStorageDIValidatorChecksumProviderFactory
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<IStorageDIValidatorChecksumProviderFactory> Ptr; ///< Pointer type

	/// \brief Returns source storage's checksum provider
	///
	/// throws std::exception
	virtual IChecksumProvider::Ptr GetSourceStorageChecksumProvider(IStoragePtr pIStorage) = 0; ///< storage

	/// \brief Returns target storage's checksum provider
	///
	/// throws std::exception
	virtual IChecksumProvider::Ptr GetTargetStorageChecksumProvider(IStoragePtr pIStorage) = 0; ///< storage

	/// \brief destructor
	virtual ~IStorageDIValidatorChecksumProviderFactory() {}
};

#endif /* ISTORAGEDIVALIDATORCHECKSUMPROVIDERFACTORY_H */