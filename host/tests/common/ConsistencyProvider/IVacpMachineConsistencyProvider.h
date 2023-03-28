///
///  \file  IVacpMachineConsistencyProvider.h
///
///  \brief contains IVacpMachineConsistencyProvider interface
///

#ifndef IVACPMACHINECONSISTENCYPROVIDER_H
#define IVACPMACHINECONSISTENCYPROVIDER_H

#include "boost/shared_ptr.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/phoenix1_primitives.hpp>
#include <boost/spirit/include/phoenix1_functions.hpp>

#include "IMachineConsistencyProvider.h"
#include "VacpDefines.h"
#include "errorexception.h"
#include "CConsistencyPoint.h"

using namespace BOOST_SPIRIT_CLASSIC_NS;

/// \brief IVacpMachineConsistencyProvider interface
class IVacpMachineConsistencyProvider : public IMachineConsistencyProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<IVacpMachineConsistencyProvider> Ptr; ///< Pointer type

	/// \brief Constructor
	IVacpMachineConsistencyProvider(IMachine::Ptr pIMachine, ///< Machine for which consistency is provided
		ILoggerPtr pILogger                                  ///< logger
		)
		: IMachineConsistencyProvider(pIMachine),
		m_pILogger(pILogger)
	{
	}

	/// \brief Creates consistency point
	///
	/// \return consistency point 
	/// \li \c throws std::exception
	virtual IConsistencyPoint::Ptr CreateConsistencyPoint(void) = 0;

	/// \brief destructor
	virtual ~IVacpMachineConsistencyProvider() {}

protected:
	/// \brief Creates consistency point by forking vacp command with given arguments
	///
	/// \return consistency point 
	/// \li \c throws std::exception
	virtual IConsistencyPoint::Ptr CreateVacpConsistencyPoint(IProcessProvider::ProcessArguments_t &vacpArgs)
	{
		m_pILogger->LogInfo("ENTERED %s\n", __FUNCTION__);

		// Run the command
		int exitStatus = -1;
		std::string output, error;
		m_pIMachine->RunSyncProcess(VACP_PATH, vacpArgs, output, error, exitStatus);

		// Check exit status of vacp
		if (0 != exitStatus)
			throw ERROR_EXCEPTION << "vacp system level crash consistency command failed with exit status " << exitStatus;

		// Get tag GUID from vacp output
		// This looks for a line that has prefix string before GUID
		std::string guid;
		std::stringstream ssOut(output);
		while (!ssOut.eof()) {
			std::string line;
			std::getline(ssOut, line); //Get each line
     /*
			if (line.empty())
				break;
     */
			guid.clear();
			bool b = parse(line.begin(), line.end(),
				lexeme_d
				[
					str_p(GUID_PREFIX_IN_VACP_OUTPUT)
					>> (+anychar_p)[phoenix::var(guid) = phoenix::construct_<std::string>(phoenix::arg1, phoenix::arg2)]
				] >> end_p,
				space_p).full; //Matches GUID prefix and stores GUID into guid variable
					if (b && !guid.empty())
						break;
		}

		// Trim spaces around GUID
		boost::algorithm::trim(guid);

		m_pILogger->LogInfo("GUID: %s\n", guid.c_str());
		// throw error if guid is not found.
		if (guid.empty())
			throw ERROR_EXCEPTION << "Vacp consistency command did not give any GUID. Check log for output of vacp command.";

		m_pILogger->LogInfo("EXITED %s\n", __FUNCTION__);

		// return the consistency point object
		// new itself throws std::bad_alloc which is derived from std::exception.
		IConsistencyPoint::Ptr pIConsistencyPoint(new CConsistencyPoint(guid));
		return pIConsistencyPoint;
	}

protected:
	ILoggerPtr m_pILogger; ///< logger
};

#endif /* IVACPMACHINECONSISTENCYPROVIDER_H */
