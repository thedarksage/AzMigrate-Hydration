#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <tchar.h>
#include "InstallInVirVolBus.h"

#include <winioctl.h>

#define FILE_DEVICE_INMAGE_VV   0x000080001
#define IOCTL_INMAGE_LOAD_BUS_DRIVER                  CTL_CODE(FILE_DEVICE_INMAGE_VV, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define VVBUS_DOS_DEVICE_NAME   _T("\\\\.\\InmageVVBusEnum")

void InstallBusDriver();

int _tmain(int argc, TCHAR *argv[])
{
    if(argc < 2)
    {
        _tprintf(_T("Usage: InVirVolBus [Commands] \nCommands are\n"));
        _tprintf(_T("\t/install - Install bus driver\n\t/uninstall - uninstall bus driver\n"));
        return 0;
    }

    if(_tcsicmp(argv[1], CMD_INSTALL) == 0)
        InstallInvirVolBus();
    else if(_tcsicmp(argv[1], CMD_UNINSTALL) == 0)
        UninstallInvirVolBus();

    return 0;
}

void InstallBusDriver()
{
    TCHAR *FileName = VVBUS_DOS_DEVICE_NAME;
    HANDLE hFile = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                            (LPSECURITY_ATTRIBUTES) FILE_ATTRIBUTE_NORMAL, OPEN_EXISTING, 0, NULL);

    if(INVALID_HANDLE_VALUE == hFile)
    {
        _tprintf("Handle value invalid\n");
        return;
    }

    BOOLEAN bRet = DeviceIoControl(hFile, IOCTL_INMAGE_LOAD_BUS_DRIVER, NULL, 0, NULL, 0, NULL, NULL);
    if(!bRet)
    {
        _tprintf(_T("InstallBusDriver: Device Ioctl failed\n"));
        return;
    }
}


VOID InstallInvirVolBus()
{
    HDEVINFO DeviceInfoList;
    DWORD ConfigFlags = 0;
    TCHAR InstanceID[] = INVIRVOLBUS_INSTANCE_ID;
    TCHAR DeviceDescription[] = INVIRVOLBUS_CONTROL_DEVICE_DESC;
	TCHAR Class[] = INVIRVOLBUS_CLASS;
	TCHAR Driver[] = INVIRVOLBUS_DRIVER;
	TCHAR Mfg[] = INVIRVOLBUS_MFG;
	TCHAR Service[] = INVIRVOLBUS_SERVICE;
	TCHAR HardwareID[] = INVIRVOLBUS_HARDWARE_ID;
    
    SP_DEVINFO_DATA DeviceInfoData;
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    DeviceInfoList = SetupDiCreateDeviceInfoList(
            &InmageClassGUID,
            NULL
            );

    if(DeviceInfoList == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("Could not create Device Info List ErrorCode:%x\n"), GetLastError());
        return;
    }

    BOOL bRet = SetupDiCreateDeviceInfo(
                DeviceInfoList,
                InstanceID,
                &InmageClassGUID,
                DeviceDescription,
                NULL,
                0,
                &DeviceInfoData
                );

    if(!bRet)
    {
        if(ERROR_DEVINST_ALREADY_EXISTS == GetLastError())
            _tprintf(_T("Driver Already installed\n"));
        else
            _tprintf(_T("Could not create InstanceID ErrorCode:%x\n"), GetLastError());
        return;
    }

    if(!SetupDiSetDeviceRegistryProperty(DeviceInfoList,
							&DeviceInfoData,
							SPDRP_HARDWAREID,
							(LPBYTE)HardwareID,
							(DWORD)(_tcslen(HardwareID) + 2)* sizeof(TCHAR)))
    {       
        _tprintf(_T("Could not set device HardwareID\n"));
        return;
    }

    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DeviceInfoList, &DeviceInfoData))
    {
            _tprintf(_T("Could not register ErrorCode:%x\n"), GetLastError());
		    return ;
    }

    if(!SetupDiSetDeviceRegistryProperty(DeviceInfoList,
								&DeviceInfoData,
								SPDRP_SERVICE,
								(LPBYTE)Service,
								(DWORD)(_tcslen(Service) + 1)* sizeof(TCHAR)))
    {       
        _tprintf(_T("Could not set device Service\n"));
        return;
    }

    if(!SetupDiSetDeviceRegistryProperty(DeviceInfoList,
								&DeviceInfoData,
								SPDRP_MFG,
								(LPBYTE)Mfg,
								(DWORD)(_tcslen(Mfg) + 1)* sizeof(TCHAR)))
    {       
        _tprintf(_T("Could not set Mfg\n"));
        return;
    }

    if(!SetupDiSetDeviceRegistryProperty(DeviceInfoList,
								&DeviceInfoData,
								SPDRP_DRIVER,
								(LPBYTE)Driver,
								(DWORD)(_tcslen(Driver) + 1)* sizeof(TCHAR)))
    {       
        _tprintf(_T("Could not set Driver\n"));
        return;
    }

    if(!SetupDiSetDeviceRegistryProperty(DeviceInfoList,
								&DeviceInfoData,
								SPDRP_HARDWAREID,
								(LPBYTE)HardwareID,
								(DWORD)(_tcslen(HardwareID) + 2)* sizeof(TCHAR)))
    {       
        _tprintf(_T("Could not set device HardwareID\n"));
        return;
    }

    if(!SetupDiSetDeviceRegistryProperty(DeviceInfoList,
								&DeviceInfoData,
								SPDRP_CONFIGFLAGS,
                                (LPBYTE)&ConfigFlags,
								sizeof(DWORD)))
    {
        _tprintf(_T("Could not set device HardwareID\n"));
        return;
    }

	return;
}

VOID UninstallInvirVolBus()
{
    HDEVINFO DeviceInfoList;
    DWORD ConfigFlags = 0;
    TCHAR InstanceID[] = INVIRVOLBUS_INSTANCE_ID;
    TCHAR DeviceDescription[] = INVIRVOLBUS_CONTROL_DEVICE_DESC;
	TCHAR Class[] = INVIRVOLBUS_CLASS;
	TCHAR Driver[] = INVIRVOLBUS_DRIVER;
	TCHAR Mfg[] = INVIRVOLBUS_MFG;
	TCHAR Service[] = INVIRVOLBUS_SERVICE;
	TCHAR HardwareID[] = INVIRVOLBUS_HARDWARE_ID;
    
    SP_DEVINFO_DATA DeviceInfoData;
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    DeviceInfoList = SetupDiCreateDeviceInfoList(
            &InmageClassGUID,
            NULL
            );

    if(DeviceInfoList == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("Could not create Device Info List ErrorCode:%x\n"), GetLastError());
        return;
    }

    BOOL bRet = SetupDiOpenDeviceInfo(
            DeviceInfoList,
            InstanceID,
            NULL,
            0,
            &DeviceInfoData
            );

    if(!bRet)
    {
        _tprintf(_T("Could not open device ErrorCode:%x\n"), GetLastError());
        return;
    }

    if(!SetupDiRemoveDevice(DeviceInfoList, &DeviceInfoData))
        _tprintf(_T("Faliled to remove device:\n"));

	return;
}
