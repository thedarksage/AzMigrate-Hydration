///
///  \file  IConsistencyPoint.h
///
///  \brief contains IConsistencyPoint interface
///

#ifndef ICONSISTENCYPOINT_H
#define ICONSISTENCYPOINT_H

#include <string>

#include "boost/shared_ptr.hpp"

/// \brief IConsistencyPoint interface
class IConsistencyPoint
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<IConsistencyPoint> Ptr; ///< Pointer type

	/// \brief Returns ID
	virtual std::string GetID(void) = 0;

	/// \brief destructor
	virtual ~IConsistencyPoint() {}
};

#endif /* ICONSISTENCYPOINT_H */