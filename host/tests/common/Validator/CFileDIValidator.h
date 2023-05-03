///
///  \file  CFileDIValidator.h
///
///  \brief contains CFileDIValidator class
///

#ifndef CFILEDIVALIDATOR_H
#define CFILEDIVALIDATOR_H

#include <cstdio>
#include <string>
#include <cerrno>

#include "boost/shared_ptr.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/filesystem/path.hpp"

#include "IFileDIValidator.h"
#include "errorexception.h"
#include "scopeguard.h"
#include "ILogger.h"
#include "CLocalProcessProvider.h"
#include "FileValidatorDefinesMajor.h"

/// \brief CFileDIValidator class
class CFileDIValidator : public IFileDIValidator
{
public:
    /// \brief Pointer type
    typedef boost::shared_ptr<CFileDIValidator> Ptr;

	CFileDIValidator(ILoggerPtr pILogger) ///< logger
		: m_pILogger(pILogger),
		m_CLocalProcessProvider(pILogger)
	{
	}

	bool Validate(const std::string &sourceFile, ///< source file
		          const std::string &targetFile  ///< target file
                  )
	{
	
		m_pILogger->LogInfo("ENTERED %s.\n", __FUNCTION__);

		boost::filesystem::path sPath(sourceFile);
		boost::filesystem::path tPath(targetFile);

		std::string sourceFileArg = "\"";
		sourceFileArg += sPath.string();
		sourceFileArg += "\"";

		std::string targetFileArg = "\"";
        targetFileArg += tPath.string();
		targetFileArg += "\"";
		
		// files to compare
		IProcessProvider::ProcessArguments_t args;
		args.push_back(sourceFileArg);
		args.push_back(targetFileArg);

		// Run the command
		int exitStatus = -1;
		std::string output, error;
		m_CLocalProcessProvider.RunSyncProcess(FILE_COMPARE_CMD, args, output, error, exitStatus);

		// Check exit status of vacp
		bool isValid = (0 == exitStatus);
		if (isValid)
			m_pILogger->LogInfo("Files %s, %s match\n", sourceFile.c_str(), targetFile.c_str());
		else
			m_pILogger->LogInfo("Files %s, %s match failed\n", sourceFile.c_str(), targetFile.c_str());

		m_pILogger->LogInfo("EXITED %s.\n", __FUNCTION__);
		return isValid;
	}

private:
	ILoggerPtr m_pILogger;                         ///< logger

	CLocalProcessProvider m_CLocalProcessProvider; ///< local process provider
};

#endif /* CFILEDIVALIDATOR_H */
