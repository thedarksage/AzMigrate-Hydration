#ifndef EXPORT_DEVICE_H
#define EXPORT_DEVICE_H
#include "applicationsettings.h"


bool ExportDeviceUsingIscsi(const std::string& deviceName, DeviceExportParams& exportParams, std::stringstream& stream)  ;
bool ExportDeviceUsingCIFS(const std::string& deviceName, const DeviceExportParams& exportParams, std::stringstream& stream)  ;
bool AssignDriveLetterIfRequired(const std::string& targetName, const std::string& mountpoint) ;
bool ExportDeviceUsingNFS(const std::string& deviceName, const DeviceExportParams& exportParams, std::stringstream& stream) ;

#endif

