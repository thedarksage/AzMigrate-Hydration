///
///  \file  CVacpAppMachineConsistencyProvider.h
///
///  \brief contains CVacpAppMachineConsistencyProvider class
///

#ifndef CVACPAPPMACHINECONSISTENCYPROVIDER_H
#define CVACPAPPMACHINECONSISTENCYPROVIDER_H

#include <string>
#include <stdexcept>

#include "IVacpMachineConsistencyProvider.h"
#include "VacpDefines.h"
#include "errorexception.h"

/// \brief CVacpAppMachineConsistencyProvider class
class CVacpAppMachineConsistencyProvider : public IVacpMachineConsistencyProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CVacpAppMachineConsistencyProvider> Ptr; ///< Pointer type

	/// \brief Constructor
	CVacpAppMachineConsistencyProvider(IMachine::Ptr pIMachine, ///< Machine
		ILoggerPtr pILogger)                                    ///< logger
		: IVacpMachineConsistencyProvider(pIMachine, pILogger)
	{
	}

	/// \brief Creates consistency point
	///
	/// \return consistency point 
	/// \li \c throws std::exception
	IConsistencyPoint::Ptr CreateConsistencyPoint(void)
	{
		// Give vacp app args
		// default system level is app consistent
		IProcessProvider::ProcessArguments_t args;
		args.push_back(SYSTEM_LEVEL_ARG); //system level

		return CreateVacpConsistencyPoint(args);
	}
};

#endif /* CVACPAPPMACHINECONSISTENCYPROVIDER_H */