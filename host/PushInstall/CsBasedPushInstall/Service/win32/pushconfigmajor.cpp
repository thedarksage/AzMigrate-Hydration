///
/// \file pushconfigmajor.cpp
///
/// \brief
///

#include <string>

#include <Windows.h>
#include <atlbase.h>

#include "errorexception.h"
#include "errorexceptionmajor.h"
#include "converterrortostringminor.h"
#include "CsPushConfig.h"

namespace PI {

	std::string CsPushConfig::configPath()
	{
		char szPathname[MAX_PATH] = "";

		try {
			CRegKey reg;
			ULONG cch = sizeof(szPathname);

			LONG rc = reg.Open(HKEY_LOCAL_MACHINE, "SOFTWARE\\SV Systems\\PushInstaller", KEY_READ);
			if (ERROR_SUCCESS != rc) {
				throw ERROR_EXCEPTION << "missing registry key HKLM\\SOFTWARE\\SV Systems\\PushInstaller";
			}

			rc = reg.QueryStringValue("InstallDirectory", szPathname, &cch);
			if (ERROR_SUCCESS != rc) {
				std::string err;
				convertErrorToString(lastKnownError(), err);
				throw ERROR_EXCEPTION << "Query registry key HKLM\\SOFTWARE\\SV Systems\\PushInstaller\\InstallDirectory failed with error "
					<< lastKnownError() << "(" << err << ")";
			}

			if (strcmp(szPathname, "") == 0) {
				throw ERROR_EXCEPTION << "Query registry key HKLM\\SOFTWARE\\SV Systems\\PushInstaller\\InstallDirectory is set to null";
			}
		}
		catch (...) {
			return "C:\\PushInstallSvc\\pushinstaller.conf";
		}

		return std::string(szPathname) + "\\pushinstaller.conf";
	}
}