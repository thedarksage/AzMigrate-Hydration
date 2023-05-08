///
///  \file  IMachineConsistencyProvider.h
///
///  \brief contains IMachineConsistencyProvider interface
///

#ifndef IMACHINECONSISTENCYPROVIDER_H
#define IMACHINECONSISTENCYPROVIDER_H

#include "boost/shared_ptr.hpp"

#include "IConsistencyProvider.h"
#include "IMachine.h"

/// \brief IMachineConsistencyProvider interface
class IMachineConsistencyProvider : public IConsistencyProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<IMachineConsistencyProvider> Ptr; ///< Pointer type

	/// \brief Constructor
	IMachineConsistencyProvider(IMachine::Ptr pIMachine)        ///< Machine for which consistency is provided
		: m_pIMachine(pIMachine)
	{
	}

	/// \brief Creates consistency point
	///
	/// \return consistency point 
	/// \li \c throws std::exception
	virtual IConsistencyPoint::Ptr CreateConsistencyPoint(void) = 0;

	/// \brief destructor
	virtual ~IMachineConsistencyProvider() {}

protected:
	IMachine::Ptr m_pIMachine; ///< Machine
};

#endif /* IMACHINECONSISTENCYPROVIDER_H */