#include <windows.h>
#include <tchar.h>
#include "drvstatus.h"
#include "InstallInVirVol.h"
#include "ServiceControl.h"

DRSTATUS InstallInVirVolDriver(INSTALL_INVIRVOL_DATA &InstallData)
{
	DRSTATUS Status = DRSTATUS_SUCCESS;
	
	Status = SCInstallService(
				InstallData.DriverName,
				InstallData.DriverName,
				InstallData.PathAndFileName,
				NULL,
				NULL);

	return Status;
}

DRSTATUS UninstallInVirVolDriver(UNINSTALL_INVIRVOL_DATA &UninstallData)
{
	DRSTATUS Status = DRSTATUS_SUCCESS;
	
	Status = SCUninstallDriver(UninstallData.DriverName);

	return Status;
}

DRSTATUS StartInVirVolDriver(START_INVIRVOL_DATA &StartData)
{
	DRSTATUS Status = DRSTATUS_SUCCESS;
	
	Status = SCStartDriver(StartData.DriverName);

	return Status;
}

DRSTATUS StopInVirVolDriver(STOP_INVIRVOL_DATA &StopData)
{
	DRSTATUS Status = DRSTATUS_SUCCESS;

	Status = SCStopDriver(StopData.DriverName);

	return Status;
}
