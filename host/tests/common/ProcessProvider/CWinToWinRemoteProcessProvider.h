///
///  \file  CWinToWinRemoteProcessProvider.h
///
///  \brief contains CWinToWinRemoteProcessProvider class
///

#ifndef CWINTOWINREMOTEPROCESSPROVIDER_H
#define CWINTOWINREMOTEPROCESSPROVIDER_H

#include <string>

#include "boost/shared_ptr.hpp"

#include "IProcessProvider.h"
#include "CLocalProcessProvider.h"
#include "ILogger.h"

#include "errorexception.h"

/// \brief CWinToWinRemoteProcessProvider class
class CWinToWinRemoteProcessProvider : public IProcessProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CWinToWinRemoteProcessProvider> Ptr; ///< Pointer type

	CWinToWinRemoteProcessProvider(const std::string &ip,  ///< ip
		const std::string &username,                       ///< user name
		const std::string &password,                       ///< password
		CLocalProcessProvider::Ptr pCLocalProcessProvider, ///< Local process provider
		ILoggerPtr pILogger                                ///< logger provider
		) : 
		m_IP(ip),
		m_Username(username),
		m_Password(password),
		m_pCLocalProcessProvider(pCLocalProcessProvider),
		m_pILogger(pILogger)
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
		//
		// Note: PSExec has to be forked from cmd /c with input redirected to <NUL,
		//       otherwise the process waits indefinitely to read from console.
		const std::string CMD = "C:\\Windows\\System32\\cmd.exe";

		const std::string PS_EXEC = "C:\\PSTools\\PSExec.exe";

		ProcessArguments_t finalArgs;
		std::string arg;

		// Form arguments
		// cmd /c
		finalArgs.push_back("/c");

		finalArgs.push_back(PS_EXEC);

		// eula acceptance
		finalArgs.push_back("-accepteula");

		//IP
		arg = "\\\\";
		arg += m_IP;
		finalArgs.push_back(arg);

		//User name
		finalArgs.push_back("-u");
		finalArgs.push_back(m_Username);

		//Passwd
		finalArgs.push_back("-p");
		finalArgs.push_back(m_Password);

		//Working directory
		boost::filesystem::path filePath(name);
		boost::filesystem::path dirPath(filePath.parent_path());
		finalArgs.push_back("-w");
		finalArgs.push_back(dirPath.string());

		//Command
		finalArgs.push_back(name);

		//Command's args
		for (ConstProcessArgumentsIter_t it = args.begin(); it != args.end(); it++)
			finalArgs.push_back(*it);

		finalArgs.push_back("<NUL");

		m_pCLocalProcessProvider->RunSyncProcess(CMD, finalArgs, output, error, exitStatus);

		m_pILogger->LogInfo("EXITED %s\n", __FUNCTION__);
		m_pILogger->Flush();
	}

	virtual ~CWinToWinRemoteProcessProvider() {}

private:
	std::string m_IP;                              ///< IP

	std::string m_Username;                        ///< Username
	 
	std::string m_Password;                        ///< Password

	CLocalProcessProvider::Ptr m_pCLocalProcessProvider; ///< Local process provider

	ILoggerPtr m_pILogger;                         ///< Logger
};

#endif /* CWINTOWINREMOTEPROCESSPROVIDER_H */