///
///  \file  CVacpCrashMachineConsistencyProvider.h
///
///  \brief contains CVacpCrashMachineConsistencyProvider class
///

#ifndef CVACPCRASHMACHINECONSISTENCYPROVIDER_H
#define CVACPCRASHMACHINECONSISTENCYPROVIDER_H

#include <string>
#include <stdexcept>

#include "IVacpMachineConsistencyProvider.h"
#include "VacpDefines.h"
#include "errorexception.h"

/// \brief CVacpCrashMachineConsistencyProvider class
class CVacpCrashMachineConsistencyProvider : public IVacpMachineConsistencyProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CVacpCrashMachineConsistencyProvider> Ptr; ///< Pointer type

	/// \brief Constructor
	CVacpCrashMachineConsistencyProvider(IMachine::Ptr pIMachine, ///< Machine
		ILoggerPtr pILogger)                                      ///< logger
		: IVacpMachineConsistencyProvider(pIMachine, pILogger)
	{
	}

	/// \brief Creates consistency point
	///
	/// \return consistency point 
	/// \li \c throws std::exception
	IConsistencyPoint::Ptr CreateConsistencyPoint(void)
	{
		// Give vacp cc args
		IProcessProvider::ProcessArguments_t args;
		args.push_back(SYSTEM_LEVEL_ARG); //system level
		args.push_back(CC_ARG);           //crash consistent

		return CreateVacpConsistencyPoint(args);
	}
};

#endif /* CVACPCRASHMACHINECONSISTENCYPROVIDER_H */