///
///  \file  IConsistencyProvider.h
///
///  \brief contains IConsistencyProvider interface
///

#ifndef ICONSISTENCYPROVIDER_H
#define ICONSISTENCYPROVIDER_H

#include "boost/shared_ptr.hpp"

#include "IConsistencyPoint.h"

/// \brief IConsistencyProvider interface
class IConsistencyProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<IConsistencyProvider> Ptr; ///< Pointer type

	/// \brief Creates consistency point
	///
	/// \return consistency point 
	/// \li \c throws std::exception
	virtual IConsistencyPoint::Ptr CreateConsistencyPoint(void) = 0;

	/// \brief destructor
	virtual ~IConsistencyProvider() {}
};

#endif /* ICONSISTENCYPROVIDER_H */