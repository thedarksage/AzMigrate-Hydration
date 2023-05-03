///
///  \file  CIStorageDIValidatorChecksumProviderFactory.h
///
///  \brief contains CIStorageDIValidatorChecksumProviderFactory  class
///

#ifndef CISTORAGEDIVALIDATORCHECKSUMPROVIDERFACTORY_H
#define CISTORAGEDIVALIDATORCHECKSUMPROVIDERFACTORY_H

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "IStorageDIValidatorChecksumProviderFactory.h"
#include "IProcessProvider.h"
#include "CIStorageMD5ChecksumProvider.h"
#include "CTargetSnapshotStorageMD5ChecksumProvider.h"

/// \brief CIStorageDIValidatorChecksumProviderFactory class
class CIStorageDIValidatorChecksumProviderFactory : public IStorageDIValidatorChecksumProviderFactory
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CIStorageDIValidatorChecksumProviderFactory> Ptr; ///< Pointer type

	/// \brief Constructor
	CIStorageDIValidatorChecksumProviderFactory(IProcessProvider::Ptr pIProcessProvider) ///< Process provider
		: m_pIProcessProvider(pIProcessProvider)
	{
	}

	/// \brief Returns source storage's checksum provider
	///
	/// throws std::exception
	IChecksumProvider::Ptr GetSourceStorageChecksumProvider(IStoragePtr pIStorage) ///< storage
	{
		IChecksumProvider::Ptr pChecksumProvider(new CIStorageMD5ChecksumProvider(pIStorage));
		return pChecksumProvider;
	}

	/// \brief Returns target storage's checksum provider
	///
	/// throws std::exception
	IChecksumProvider::Ptr GetTargetStorageChecksumProvider(IStoragePtr pIStorage) ///< storage
	{
		IChecksumProvider::Ptr pChecksumProvider(new CTargetSnapshotStorageMD5ChecksumProvider(pIStorage, m_pIProcessProvider));
		return pChecksumProvider;
	}

private:
	IProcessProvider::Ptr m_pIProcessProvider; ///< Process provider
};

#endif /* CISTORAGEDIVALIDATORCHECKSUMPROVIDERFACTORY_H */
