#ifndef INMAGE_PI_DEFS_H
#define INMAGE_PI_DEFS_H

#include "pushInstallationSettings.h"

enum PUSH_JOB_STATUS
{
	INSTALL_JOB_PENDING = 1,
	INSTALL_JOB_PROGRESS = 2,
	INSTALL_JOB_COMPLETED = 3,
	INSTALL_JOB_FAILED = -3,
	INSTALL_UPGRADE_BUILD_NOT_AVAILABLE = -7,
	UNINSTALL_JOB_PENDING = 4,
	UNINSTALL_JOB_PROGRESS = 5,
	UNINSTALL_JOB_COMPLETED = 6,
	UNINSTALL_JOB_FAILED = -6,
	DELETE_JOB_PENDING = 8,
	DELETE_JOB_COMPLETED = 9,
	INSTALL_JOB_COMPLETED_WITH_WARNINGS = -10
};

struct InstallDetails
{
	std::string m_installerPath;
	std::string m_domainName;
	std::string m_deploymentDir;
	std::string m_serviceUserName;
	std::string m_servicePasswd;
	bool m_installUnderSystemAcc;
	PUSH_JOB_STATUS m_status;
	INSTALL_TASK m_task;
};


#endif
