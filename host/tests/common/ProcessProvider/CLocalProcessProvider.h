///
///  \file  CLocalProcessProvider.h
///
///  \brief contains CLocalProcessProvider class
///

#ifndef CLOCALPROCESSPROVIDER_H
#define CLOCALPROCESSPROVIDER_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "IProcessProvider.h"
#include "ILogger.h"

#include "errorexception.h"
#include "inmcommand.h"

/// \brief CLocalProcessProvider class
class CLocalProcessProvider : public IProcessProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CLocalProcessProvider> Ptr; ///< Pointer type

	CLocalProcessProvider(ILoggerPtr pILogger)     ///< logger provider
		: m_pILogger(pILogger)
	{
	}

	/// \brief Runs a synchronous process. The function does not return until process execution completes.
	///
	/// throws std::exception
	virtual void RunSyncProcess(const std::string &name, ///< name
		const ProcessArguments_t &args,                  ///< args
		std::string &output,                             ///< out: stdout
		std::string &error,                              ///< out: stderr
		int &exitStatus                                  ///< out: exit status
		)
	{
		m_pILogger->LogInfo("ENTERED %s: process name: %s\n", __FUNCTION__, name.c_str());

		// Form the command line from process name and arguments

		// Form process name
		std::string quotedName = "\""; // quoted name is needed to avoid interpretation of first token (separated by space) in process name as a process 
		quotedName += name;
		quotedName += "\"";

		std::string cmdWithArgs = quotedName;
		// Attach arguments
		for (ConstProcessArgumentsIter_t it = args.begin(); it != args.end(); it++) {
			m_pILogger->LogInfo("Argument: %s\n", it->c_str());
			cmdWithArgs += " ";
			cmdWithArgs += *it;
		}
		
		// For debugging record cmdWithArgs
		m_pILogger->LogInfo("Running command line: %s\n", cmdWithArgs.c_str());

		// Run the command
		InmCommand ic(cmdWithArgs);
		InmCommand::statusType status = ic.Run();
		if (status != InmCommand::completed)
			throw ERROR_EXCEPTION << "Failed to run " << cmdWithArgs << " with status " << status;

		output = ic.StdOut();
		error = ic.StdErr();
		exitStatus = ic.ExitCode();

		m_pILogger->LogInfo("exitStatus = %d\n========== Output: ==========\n%s\n==========Error:===========\n%s\n", exitStatus, output.c_str(), error.c_str());

		m_pILogger->LogInfo("EXITED %s\n", __FUNCTION__);
		m_pILogger->Flush();
	}

	virtual ~CLocalProcessProvider() {}

private:
	ILoggerPtr m_pILogger; ///< Logger
};

#endif /* CLOCALPROCESSPROVIDER_H */