///
///  \file  CConsistencyPoint.h
///
///  \brief contains CConsistencyPoint class
///

#ifndef CCONSISTENCYPOINT_H
#define CCONSISTENCYPOINT_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "IConsistencyPoint.h"

/// \brief CConsistencyPoint class
class CConsistencyPoint : public IConsistencyPoint
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CConsistencyPoint> Ptr; ///< Pointer type

	/// \brief Constructor
	CConsistencyPoint(const std::string &id) ///< id
		: m_ID(id)
	{
	}

	/// \brief Returns ID
	std::string GetID(void) { return m_ID;  }

private:
	std::string m_ID; ///< id
};

#endif /* CCONSISTENCYPOINT_H */