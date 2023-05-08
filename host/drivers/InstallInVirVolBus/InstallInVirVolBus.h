#ifndef _INSTALL_INVIRVOL_BUS_H_
#define _INSTALL_INVIRVOL_BUS_H_

// {42899BF8-F95F-41c2-8009-9588DC64526F}
static const GUID InmageClassGUID = 
{ 0x42899bf8, 0xf95f, 0x41c2, { 0x80, 0x9, 0x95, 0x88, 0xdc, 0x64, 0x52, 0x6f } };

#define INVIRVOLBUS_INSTANCE_ID _T("Root\\Inmage\\0000")
#define INVIRVOLBUS_CONTROL_DEVICE_DESC _T("Inmage Control Device")
#define INVIRVOLBUS_CLASS _T("Inmage")
#define INVIRVOLBUS_DRIVER _T("{42899BF8-F95F-41c2-8009-9588DC64526F}\\0000")
#define INVIRVOLBUS_MFG _T("Inmage")
#define INVIRVOLBUS_SERVICE _T("InVirVolBus")
#define INVIRVOLBUS_HARDWARE_ID _T("ROOT\\INMAGE\0")

#define CMD_INSTALL _T("/install")
#define CMD_UNINSTALL _T("/uninstall")

VOID InstallInvirVolBus();
VOID UninstallInvirVolBus();

#define DRIVER_ALREADY_INSTALLED 0xe0000207
#endif
