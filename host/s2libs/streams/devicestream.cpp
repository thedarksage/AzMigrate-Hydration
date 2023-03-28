#include <stdio.h>
#include <string>

#include "portablehelpers.h"

#include "error.h"
#include "entity.h"

#include "genericfile.h"
#include "device.h"

#include "genericfile.h"
#include "genericstream.h"
#include "filestream.h"
#include "genericdevicestream.h"
#include "devicestream.h"
#include "ace/OS.h"

//using namespace inmage::foundation;

/*****************************************************************************************
*** Note: Please write any platform specific code in ${platform}/devicestream_port.cpp ***
*****************************************************************************************/

//namespace inmage {
DeviceStream::DeviceStream():m_bSync(true)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ResetErrorCode();
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

DeviceStream::DeviceStream(const std::string& sDeviceName)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ResetErrorCode();
	if ( !sDeviceName.empty() )
	{
		m_pGenericFile = new Device(sDeviceName);
//		Init();
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

DeviceStream::DeviceStream(const Device& device)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    ResetErrorCode();
	std::string sName = device.GetName();
	if ( !sName.empty() )
	{
		m_pGenericFile = new Device(device);
//		Init();
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}
/*
int DeviceStream::Init() 
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	int iStatus = SV_SUCCESS;
//	m_bInitialized = true;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return iStatus;
}
*/
DeviceStream::~DeviceStream()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	Close();

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

int DeviceStream::Close()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
    this->Clear();

    if ( IsOpen() ) 
    {	
	    if ( !m_bSync )
        {
            iStatus = CancelIO();
#ifdef SV_WINDOWS
			CloseHandle(m_IoEvent.hEvent);
#endif
        }
    }
    if ( NULL != m_pGenericFile )
    {
        if ( ACE_INVALID_HANDLE != m_pGenericFile->GetHandle())
        {
            ACE_OS::close(m_pGenericFile->GetHandle());
            m_pGenericFile->SetHandle(ACE_INVALID_HANDLE);
        }
        delete m_pGenericFile;
    }

//	m_bInitialized  = false;		
    m_bOpen         = false;
    m_pGenericFile = NULL;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return iStatus;
}

int DeviceStream::GetErrorCode(void)
{
    return m_iErrorCode;
}

void DeviceStream::SetErrorCode(int iErrorCode)
{
    m_iErrorCode = iErrorCode;
}

void DeviceStream::ResetErrorCode(void)
{
    SetErrorCode(SV_SUCCESS);
}

std::string DeviceStream::GetName(void)
{
	return m_pGenericFile ? m_pGenericFile->GetName() : std::string();
}

///////////////////////////////////////////////////////////////////////////////

//}

