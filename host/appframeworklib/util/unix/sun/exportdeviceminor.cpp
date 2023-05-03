#include <fstream>
#include <ace/Configuration.h>
#include "util/exportdevice.h"
#include <ace/Configuration_Import_Export.h>
#include "appcommand.h"
#include "localconfigurator.h"
#include "controller.h"
#include "util/common/util.h"
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>

bool ExportDeviceUsingIscsi(const std::string& deviceName, DeviceExportParams& exportParams, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	std::string initiatorIPs, targetName, Tid ;
	std::string  mountpoint, sessionId, fstype,targetIp ;
	
	std::string operation ;
	

	if( exportParams.m_Exportparams.find("InitiatorIPs") != exportParams.m_Exportparams.end() )
	{
		initiatorIPs = exportParams.m_Exportparams.find("InitiatorIPs")->second ;
	}

	if( exportParams.m_Exportparams.find("TargetName") != exportParams.m_Exportparams.end() )
	{
		targetName = exportParams.m_Exportparams.find("TargetName")->second ;
	}

	if( exportParams.m_Exportparams.find("Tid") != exportParams.m_Exportparams.end() )
	{
		Tid = exportParams.m_Exportparams.find("Tid")->second ;
	}
	
	if( exportParams.m_Exportparams.find("Operation") != exportParams.m_Exportparams.end() )
	{
		operation = exportParams.m_Exportparams.find("Operation")->second ;
	}

	if( exportParams.m_Exportparams.find("MountPoint") != exportParams.m_Exportparams.end() )
	{
		mountpoint = exportParams.m_Exportparams.find("MountPoint")->second ;
	}

	if( exportParams.m_Exportparams.find("SessionId") != exportParams.m_Exportparams.end() )
	{
		sessionId = exportParams.m_Exportparams.find("SessionId")->second ;
	}

	if( exportParams.m_Exportparams.find("FSType") != exportParams.m_Exportparams.end() )
	{
		fstype = exportParams.m_Exportparams.find("FSType")->second ;
	}

	if( exportParams.m_Exportparams.find("TargetIP") != exportParams.m_Exportparams.end() )
	{
		targetIp = exportParams.m_Exportparams.find("TargetIP")->second ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Initator IPs %s\n", initiatorIPs.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "target name %s\n", targetName.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "tid %s\n", Tid.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "operation %s\n", operation.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "mount point %s\n", mountpoint.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "session id %s\n", sessionId.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "fstype %s\n", fstype.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "targetip %s\n", targetIp.c_str()) ;
	if( operation.compare("Export") == 0 )
	{
		if( targetName.compare("") == 0 ||
			Tid.compare("") == 0 ||
			deviceName.compare("") == 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "One of the Non-optional parameters doesn't have proper value\n") ;
		}
	}
	else if( operation.compare("Login") == 0 )
	{
		std::string deviecNameAfterLogin ;
		if( targetName.compare("") == 0 ||
			targetIp.compare("") == 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "One of the Non-optional parameters doesn't have proper value\n") ;
		}
	}
	else if(operation.compare("Disconnect") == 0 )
	{
		if( Tid.compare("") == 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "One of the Non-optional parameters doesn't have proper value\n") ;	
		}
	}
	else if(operation.compare("Logout") == 0 )
	{
		if( targetName.compare("") == 0 ||
			targetIp.compare("") == 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "One of the Non-optional parameters doesn't have proper value\n") ;
		}
	}

    // XXX - for now
    stream << "iSCSI target export or initiator discovery not supported on Solaris";
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool ExportDeviceUsingCIFS(const std::string& deviceName, const DeviceExportParams& exportParams, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	std::string path, shareName ;
	bool exportOp ;

	bool  validParams = false ;
	if( exportParams.m_Exportparams.find("LocalPathName") != exportParams.m_Exportparams.end() )
	{
		path = exportParams.m_Exportparams.find("LocalPathName")->second ;	
		DebugPrintf(SV_LOG_DEBUG, "Local path Name %s\n", path.c_str()) ;
		if( exportParams.m_Exportparams.find("ShareName") != exportParams.m_Exportparams.end() )
		{
			shareName = exportParams.m_Exportparams.find("ShareName")->second ;
			DebugPrintf(SV_LOG_DEBUG, "Share Name %s\n", shareName.c_str()) ;
			if( exportParams.m_Exportparams.find("Operation") != exportParams.m_Exportparams.end() )
			{
				std::string operation = exportParams.m_Exportparams.find("Operation")->second ;
				if( operation.compare("Export") == 0 )
				{
					exportOp = true ;		
				}
				else
				{
					exportOp = false ;
				}
				validParams = true ;
			}	
		}
		else
		{
			stream << "ShareName is not found in the ExportParams. The device " << deviceName << " cannot be exported using cifs" ;
		}
	}
	else
	{
		stream << "LocalPath is not found in the ExportParams. The device " << deviceName << " cannot be exported using cifs" ;
	}
	if( validParams )
	{
        // XXX - for now
        stream << "CIFS export not supported on Solaris.";
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool ExportDeviceUsingNFS(const std::string& deviceName, const DeviceExportParams& exportParams, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	std::string path, shareName ;
	bool exportOp ;

	bool  validParams = false ;
	if( exportParams.m_Exportparams.find("LocalPathName") != exportParams.m_Exportparams.end() )
	{
		path = exportParams.m_Exportparams.find("LocalPathName")->second ;	
		DebugPrintf(SV_LOG_DEBUG, "Local path Name %s\n", path.c_str()) ;
		if( exportParams.m_Exportparams.find("Operation") != exportParams.m_Exportparams.end() )
		{
			std::string operation = exportParams.m_Exportparams.find("Operation")->second ;
			if( operation.compare("Export") == 0 )
			{
				exportOp = true ;		
			}
			else
			{
				exportOp = false ;
			}
			validParams = true ;
		}	
	}
	else
	{
		stream << "LocalPath is not found in the ExportParams. The device " << deviceName << " cannot be exported using cifs" ;
	}

	if( validParams )
	{
        // XXX - for now
        stream << "NFS export not supported on Solaris.";
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}
