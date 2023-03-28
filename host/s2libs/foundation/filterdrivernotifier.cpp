///
///  \file  filterdrivernotifier.cpp
///
///  \brief contains FilterDriverNotifier implementation
///

#include <string>
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "filterdrivernotifier.h"
#include "portablehelpersmajor.h"

#ifdef SV_WINDOWS
#include <windows.h>
#include <winioctl.h>
#endif
#include "devicefilter.h"

#include "portable.h"
#include "logger.h"

FilterDriverNotifier::FilterDriverNotifier(const std::string &driverName)
: m_DriverName(driverName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with driver name %s\n", FUNCTION_NAME, driverName.c_str());
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

FilterDriverNotifier::~FilterDriverNotifier()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void FilterDriverNotifier::Notify(const NotificationType &notificationType)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with notification type %s\n", FUNCTION_NAME, GetStrNotificationType(notificationType).c_str());

	//Create the device stream if needed
	if (!m_pDeviceStream) {
		try {
			m_pDeviceStream.reset(new DeviceStream(Device(m_DriverName)));
			DebugPrintf(SV_LOG_DEBUG, "Created device stream.\n");
		}
		catch (std::bad_alloc &e) {
			throw FilterDriverNotifierEx(m_DriverName, SV_FAILURE, std::string("Memory allocation failed for creating driver stream object with exception: ") + e.what());
		}
	}

	//Open device stream in asynchrnous mode if needed
	if (!m_pDeviceStream->IsOpen()) {
		if (SV_SUCCESS == m_pDeviceStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_Asynchronous | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW))
			DebugPrintf(SV_LOG_DEBUG, "Opened driver %s in async mode.\n", m_DriverName.c_str());
		else
			throw FilterDriverNotifierEx(m_DriverName, m_pDeviceStream->GetErrorCode(), std::string("Failed to open driver"));
	}

	//Shutdown notify input
	SHUTDOWN_NOTIFY_INPUT sni = { 0 };
	sni.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING;
	sni.ulFlags |= SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILES;

	//Process start notify input
	PROCESS_START_NOTIFY_INPUT psni = { 0 };
	psni.ulFlags |= PROCESS_START_NOTIFY_INPUT_FLAGS_DATA_FILES_AWARE;

	//set ioctl input based on notification type
	void *pInput;
	long inputLen;
	unsigned long ioctlCode;
	if (E_SERVICE_SHUTDOWN_NOTIFICATION == notificationType) {
		ioctlCode = IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY;
		pInput = &sni;
		inputLen = sizeof sni;
	}
	else if (E_PROCESS_START_NOTIFICATION == notificationType) {
		ioctlCode = IOCTL_INMAGE_PROCESS_START_NOTIFY;
		pInput = &psni;
		inputLen = sizeof psni;
	}
	else {
		throw FilterDriverNotifierEx(m_DriverName, SV_FAILURE, std::string("Wrong notification type: ") + boost::lexical_cast<std::string>(notificationType));
	}

	//Issue ioctl
	if (SV_SUCCESS == m_pDeviceStream->IoControl(ioctlCode, pInput, inputLen, NULL, 0))
		DebugPrintf(SV_LOG_DEBUG, "notification issued.\n");
	else
		throw FilterDriverNotifierEx(m_DriverName, m_pDeviceStream->GetErrorCode(), std::string("Failed to issue notification ioctl"));

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

#ifdef SV_WINDOWS
/*
* FUNCTION NAME : FilterDriverNotifier::ConfigureDataPagedPool
*
* DESCRIPTION
*       Configures driver with specified data paged pool in MB.
*       It creates an in parameter with flags DRIVER_CONFIG_DATA_POOL_SIZE and
*               ulDataPoolSize as provided input.
*       It also creates an output config parameter.
*       It sends IOCTL_INMAGE_SET_DRIVER_CONFIGURATION with these 2 parameters
*           to filter driver.
*       If return status is failure or, driver config output error is set
*           an exception is thrown.
*       Logs success in case of success
*
* INPUT PARAMETERS : Data paged Pool to be used by filter driver in MB
*
* OUTPUT PARAMETERS : none
*
* return value : None.
*
*/
void FilterDriverNotifier::ConfigureDataPagedPool(SV_ULONG ulDataPoolSizeInMB)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);
    DRIVER_CONFIG   DriverInConfig = { 0 };
    DRIVER_CONFIG   DriverOutConfig = { 0 };

    if (!m_pDeviceStream) {
        try {
            m_pDeviceStream.reset(new DeviceStream(Device(m_DriverName)));
            DebugPrintf(SV_LOG_DEBUG, "Created device stream.\n");
        }
        catch (std::bad_alloc &e) {
            throw FilterDriverNotifierEx(m_DriverName, SV_FAILURE, std::string("Memory allocation failed for creating driver stream object with exception: ") + e.what());
        }
    }
    if (!m_pDeviceStream->IsOpen()) {
        if (SV_SUCCESS == m_pDeviceStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_Asynchronous | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW))
            DebugPrintf(SV_LOG_DEBUG, "Opened driver %s in async mode.\n", m_DriverName.c_str());
        else
            throw FilterDriverNotifierEx(m_DriverName, m_pDeviceStream->GetErrorCode(), std::string("Failed to open driver"));
    }
    DriverInConfig.ulFlags1 = DRIVER_CONFIG_DATA_POOL_SIZE;
    DriverInConfig.ulDataPoolSize = ulDataPoolSizeInMB;

    DebugPrintf(SV_LOG_INFO, "Data Paged Pool Configuration = %lu MB\n", ulDataPoolSizeInMB);
    if (SV_SUCCESS == m_pDeviceStream->IoControl(IOCTL_INMAGE_SET_DRIVER_CONFIGURATION, &DriverInConfig, sizeof(DriverInConfig), &DriverOutConfig, sizeof(DriverOutConfig))) {
        if (TEST_FLAG(DriverOutConfig.ulError, DRIVER_CONFIG_DATA_POOL_SIZE)) {
            std::stringstream ssError;
            ssError << "Error Data Page Pool" << ulDataPoolSizeInMB << "cannot be lesser than Max Lock Data Block" << DriverOutConfig.ulMaxLockedDataBlocks;
            throw FilterDriverNotifierEx(m_DriverName, m_pDeviceStream->GetErrorCode(), ssError.str());
        }
        DebugPrintf(SV_LOG_DEBUG, "DataPagedPool Configured.\n");
    }
    else{
        std::stringstream ssError;
        ssError << "Failed to configure Data Paged Pool Size = " << ulDataPoolSizeInMB;
        throw FilterDriverNotifierEx(m_DriverName, m_pDeviceStream->GetErrorCode(), ssError.str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
#endif
DeviceStream * FilterDriverNotifier::GetDeviceStream(void)
{
	return m_pDeviceStream.get();
}

std::string FilterDriverNotifier::GetStrNotificationType(const NotificationType &type)
{
	static const char * const StrNotificationTypes[] = { "service shutdown notification", "process start notification" };
	return (type < E_UNKNOWN_NOTIFICATION_TYPE) && (type >= E_SERVICE_SHUTDOWN_NOTIFICATION) ? StrNotificationTypes[type] : "unknown";
}

FilterDriverNotifier::Exception::Exception(const char *fileName, const int &lineNo, const std::string &driverName, const int &code, const std::string &message)
: m_Code(code),
m_Message(std::string("FILE: ") + fileName +
std::string(", LINE: ") + boost::lexical_cast<std::string>(lineNo) +
std::string(", DRIVER NAME: ") + driverName +
std::string(", MESSAGE: ") + message +
std::string(", CODE: ") + boost::lexical_cast<std::string>(code))
{
}

FilterDriverNotifier::Exception::~Exception() throw()
{
}

const char* FilterDriverNotifier::Exception::what() const throw()
{
	return m_Message.c_str();
}

int FilterDriverNotifier::Exception::code() const throw()
{
	return m_Code;
}
