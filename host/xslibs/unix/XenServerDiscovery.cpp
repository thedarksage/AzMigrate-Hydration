#include <iostream>

#include "XenServerDiscovery.h"
#include "svutils.h"

void replaceHyphen(std::string &temp)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	size_t index = 0;
	while( ( (index = temp.find('-',index)) != std::string::npos ) && index < temp.length() )
	{							
		temp.insert(index, "-");								
		index=index+2;								
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool check_xapi_service()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	cmd  = "service xapi status | grep stop";

	std::stringstream results;

	if(!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	DebugPrintf(SV_LOG_INFO,"XAPI service result: %s\n",results.str().c_str());

	std::string result = results.str();

	if(!result.empty())
	{
		DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : XAPI service is stopped\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		rv = false;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool set_if_lvhdsr(const std::string sruuid, bool& lvhdsr)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if key value pair has ": "
	cmd  = "xe sr-list params=sm-config uuid=" + sruuid + " 2> /dev/null | grep \"use_vhd: true;\"";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	std::string result = results.str();

	if(result.empty())
	{
		DebugPrintf(SV_LOG_DEBUG,
				"Function %s @LINE %d in FILE %s :SR [%s] does not use VHD on LVM\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, sruuid.c_str() );

		lvhdsr = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return rv;
}

bool isValidLV(const std::string lvname, bool& validLV)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if key value pair has ": "
	cmd  = "lvdisplay " + lvname + " | grep " + lvname;
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	std::string result = results.str();

	if(result.empty())
	{
		DebugPrintf(SV_LOG_DEBUG,
				"Function %s @LINE %d in FILE %s :[%s] is not LVHD based virtual disk\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, lvname.c_str() );

		validLV = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return rv;
}

void printXenPoolInfo(ClusterInfo &xenPoolInfo, VMInfos_t &vminfos, VDIInfos_t &vdiinfos)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Pool UUID [%s]\n", 
			FUNCTION_NAME, LINE_NO, FILE_NAME, xenPoolInfo.id.c_str());
	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Pool Name [%s]\n", 
			FUNCTION_NAME, LINE_NO, FILE_NAME, xenPoolInfo.name.c_str());

	for( std::vector<ClusterGroupInfo>::const_iterator vmIter = xenPoolInfo.groups.begin(); 
			vmIter != xenPoolInfo.groups.end(); vmIter++ )
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VM UUID [%s]\n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME, vmIter->id.c_str());
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VM Name [%s]\n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME, vmIter->name.c_str());
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VM Resource Count VMUUID [%d]\n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME, vmIter->resources.size());
		for( std::vector<ClusterResourceInfo>::const_iterator vdIter = vmIter->resources.begin();
				vdIter != vmIter->resources.end(); vdIter++)
		{
			DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VDI UUID [%s]\n", 
					FUNCTION_NAME, LINE_NO, FILE_NAME, vdIter->id.c_str());
			DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VDI Name [%s]\n", 
					FUNCTION_NAME, LINE_NO, FILE_NAME, vdIter->name.c_str());

			for( std::vector<ClusterVolumeInfo>::const_iterator volIter = vdIter->volumes.begin();
					volIter != vdIter->volumes.end(); volIter++)
			{
				DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : LV ID [%s]\n", 
						FUNCTION_NAME, LINE_NO, FILE_NAME, volIter->deviceId.c_str());
				DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : LV Name [%s]\n", 
						FUNCTION_NAME, LINE_NO, FILE_NAME, volIter->deviceName.c_str());
			}
		}
	}
	VMInfos_t::const_iterator vminfoIter;
	VDIInfos_t::const_iterator vdiinfoIter;

	for(vminfoIter = vminfos.begin(); vminfoIter != vminfos.end(); vminfoIter++)
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VM UUID [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vminfoIter->uuid.c_str());
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VM Resident on [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vminfoIter->residenton.c_str());
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VM Power state [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vminfoIter->powerstate.c_str());
	}

	for(vdiinfoIter = vdiinfos.begin(); vdiinfoIter != vdiinfos.end(); vdiinfoIter++)
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VDI UUID [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vdiinfoIter->uuid.c_str());
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VDI description [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vdiinfoIter->vdi_description.c_str());
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VDI devicenumber [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vdiinfoIter->vdi_devicenumber.c_str());
        DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VDI availability [%d]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vdiinfoIter->vdi_availability);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool get_pool_param(const std::string pool_param, std::string& result)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if this is valid vm param or not
	cmd  = "xe pool-list params=" + pool_param + " | grep : | cut -d: -f2 | cut -c2-";        
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results)) 
	{
		DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
	{
		DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
		rv = false;
	}

	result = results.str();	
	trim(result);
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return rv;
}

bool get_vm_param(const std::string uuid, const std::string vm_param, std::string& vmname)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if this is valid pool param or not
	cmd  = "xe vm-list params=" + vm_param + " uuid=" + uuid + " | grep : | cut -d: -f2 | cut -c2-";	
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
		            "Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
		            FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	vmname = results.str();
	trim(vmname);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return rv;
}
bool get_host_param(const std::string uuid, const std::string host_param, std::string& value)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if this is valid host param or not
	cmd  = "xe host-list params=" + host_param + " uuid=" + uuid + " | grep : | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	value = results.str();
	trim(value);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return rv;
}
bool get_vbd_param(const std::string uuid, const std::string vbd_param, std::string& vdiuuid)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if this is valid vbd param or not
	cmd  = "xe vbd-list params=" + vbd_param + " uuid=" +  uuid + " | grep : | cut -d: -f2 | cut -c2-";	

	cmd += " 2> /dev/null ";

	std::stringstream results;

	sleep(1);

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
		            "Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
		            FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	vdiuuid = results.str();
	trim(vdiuuid);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return rv;
}

bool get_vbd_userdevice(const std::string vmuuid, const std::string vdiuuid, const std::string vbd_param, std::string& value)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;
	
	//Validate if this is valid vbd param or not
	cmd  = "xe vbd-list params=" + vbd_param + " vm-uuid=" +  vmuuid + " vdi-uuid=" + vdiuuid + " | grep : | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	DebugPrintf(SV_LOG_INFO,"Function %s @LINE %d in FILE %s : cmd %s \n",
			FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());

	std::stringstream results;
	
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	value = "0";
	value += results.str();
	trim(value);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return rv;
}

bool get_vdi_writable(const std::string uuid, bool& isWritable)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if this is valid pool param or not
	cmd  = "xe vdi-list params=read-only uuid=" + uuid;
	cmd += " | grep : | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	std::string readonly_property;
	results >> readonly_property;
	trim(readonly_property);
	if(readonly_property.compare("false") == 0)
		isWritable = true; //Writable device
	else
		isWritable = false;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool get_vdi_param(const std::string uuid, const std::string vdi_param, std::string& sruuid)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if this is valid vbd param or not
	cmd  = "xe vdi-list params=" + vdi_param + " uuid=";
	cmd += uuid + " | grep : | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	sleep(1);

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	sruuid = results.str();
	trim(sruuid);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return rv;
}

bool get_sr_shared(const std::string& sruuid, bool& issharedsr)
{ 
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if this is valid pool param or not
	cmd  = "xe sr-list params=shared uuid=" + sruuid;
	cmd += " | grep : | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	std::string sharedproperty;
	results >> sharedproperty;
	trim(sharedproperty);

	//cout << "sharedproperty = " << sharedproperty << endl;
	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [sr attribute] shared: [%s]\n",
			FUNCTION_NAME, LINE_NO, FILE_NAME, sharedproperty.c_str());

	if(sharedproperty.compare("true") == 0)
		issharedsr = true; // Shared SR
	else
		issharedsr = false;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool get_sr_is_lvm(const std::string sruuid, bool &islvmsr)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	std::string cmd;

	//Validate if this is valid pool param or not
	cmd  = "xe sr-list params=type uuid=" + sruuid + " | grep : | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	std::string srtype;
	results >> srtype;
	trim(srtype);

	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [sr attribute][uuid:%s] type: [%s]\n",
			FUNCTION_NAME, LINE_NO, FILE_NAME, sruuid.c_str(), srtype.c_str());

	if(srtype.compare("lvm") == 0 ||
			srtype.compare("lvmoiscsi") == 0 ||
			srtype.compare("lvmohba") == 0)
	{
		islvmsr = true; // Shared: local sr empty
	}
	else
	{
		islvmsr = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool get_all_pooled_vms(std::list<std::string>& poolvms)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	//Get All VBDs of bootable disks
	bool rv = true;
	std::string cmd;

	cmd = "xe vbd-list device=hda params=vm-uuid | grep :  | cut -d: -f2 | cut -c2-";
	cmd+= " 2> /dev/null ";

	std::stringstream windowsvms;
	if (!executePipe(cmd, windowsvms))
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		return false; //rv = false;
	}
	if(XAPI_LOST_CONNECTION == windowsvms.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        return false;
	}

	std::string vmstring;
	while(!windowsvms.eof())
	{
		windowsvms >> vmstring;
		trim(vmstring);
		if(!vmstring.empty())
		{
			poolvms.push_back(vmstring);
			DebugPrintf(SV_LOG_INFO,
				"Function %s @LINE %d in FILE %s : Discovered the VM :[%s] \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vmstring.c_str());
		}
	}
	
	cmd.clear();
	cmd = "xe vbd-list bootable=true params=uuid | grep :  | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
	 			FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		return false; //rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        return false;
    }

	while(!results.eof())
	{
		//Traverse one by one VBD
		//Get VDI for this VBD
		std::string vbd, vdi, sruuid, pooled;
		results >> vbd;
		trim(vbd);
		if(!vbd.empty())
		{
			//cout << "vbd = " << vbd << endl;
			DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [vbd attribute][uuid:%s] \n",
			  		FUNCTION_NAME, LINE_NO, FILE_NAME, vbd.c_str());

			if(get_vbd_param(vbd, "vdi-uuid", vdi))
			{
				//Is this VDI read-only or NOT
				bool isWritable = false;
				if(get_vdi_writable(vdi, isWritable))
				{
					DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [vdi attribute][uuid:%s] is Writable: [%d]\n",
							FUNCTION_NAME, LINE_NO, FILE_NAME, vdi.c_str(), isWritable);
					if(isWritable)
					{		
						//Get SR of VDI 
						if(get_vdi_param(vdi, "sr-uuid", sruuid))
						{		
							//cout << "\t\t\tSR = " << sruuid << endl;
                           	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [vdi attribute][uuid:%s] sr-uuid: [%s]\n",
									FUNCTION_NAME, LINE_NO, FILE_NAME, vdi.c_str(), sruuid.c_str());
         							
							bool issharedsr = false;
							if(get_sr_shared(sruuid, issharedsr))
							{
								DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [sr attribute][uuid:%s] is shared SR: [%d]\n",
										FUNCTION_NAME, LINE_NO, FILE_NAME, sruuid.c_str(), issharedsr);
         
								if(issharedsr)
								{									
									//Get VM of this VBD 
									if(get_vbd_param(vbd, "vm-uuid", pooled))
									{
										DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Discovered pool VM [uuid:%s]\n",
												FUNCTION_NAME, LINE_NO, FILE_NAME, pooled.c_str());
										poolvms.push_back(pooled);
									}
									else
									{
										rv = false;
									}
								}
								else
								{
									//Not Shared SR\n";
								}
							}
							else
							{
								rv = false;
							}
						}
						else
						{
							//Failed to get sr-uuid
							rv = false;
						}
					}
					else
					{
						//Read-only VDI
					}
				}
				else
				{
					rv = false;
				}
			}//Get VDI process VDI
			else
			{
				rv = false;
			}
		}
	}// All VDIs

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool get_vm_vbd_list(const std::string vmuuid, std::list<std::string>& vbdlist)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	//Get All VBDs of bootable disks
	bool rv = true;

	std::string vbd;
	std::string cmd;

	cmd = "xe vbd-list params=uuid vm-uuid=" + vmuuid + " | grep :  | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}
	
	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        return false;
    }

	while(!results.eof())
	{
		vbd.clear();
		results >> vbd;
		trim(vbd);
		if(!vbd.empty())
			vbdlist.push_back(vbd);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;	
}

bool get_pooledvdi_on_pooledvm(const std::string vmuuid, std::vector<std::string>& pvdilist)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	ClusterResourceInfo resinfo;
	ClusterVolumeInfo volinfo;

	//Get all vbd's of this vm
	std::list<std::string> vbdlist;
	std::string vdi, sruuid;
	bool isWritable, issharedsr, islvmsr;

	if(get_vm_vbd_list(vmuuid, vbdlist))
	{
		//Get vdi for the vbd
		std::list<std::string>::const_iterator vbdIter;
		vbdIter = vbdlist.begin();
		for(;vbdIter != vbdlist.end(); vbdIter++)
		{
			//cout << "\tvbdIter = " << *vbdIter << endl;
	        DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [vbd attribute][uuid:%s] \n",
					FUNCTION_NAME, LINE_NO, FILE_NAME, vbdIter->c_str());

			vdi.clear();
			if(get_vbd_param(*vbdIter, "vdi-uuid", vdi))
			{
				isWritable = false;
				//Is this VDI read-only or NOT
				if(get_vdi_writable(vdi, isWritable))
				{
					DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [vdi attribute][uuid:%s] is Writable: [%d]\n",
							FUNCTION_NAME, LINE_NO, FILE_NAME, vdi.c_str(), isWritable);

					if(isWritable)
					{
						sruuid.clear();			
						if(get_vdi_param(vdi, "sr-uuid", sruuid))
						{
							//Is shared SR 
							issharedsr = false;
							if(get_sr_shared(sruuid, issharedsr))
							{
								DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [sr attribute][uuid:%s] is shared SR: [%d]\n",
										FUNCTION_NAME, LINE_NO, FILE_NAME, sruuid.c_str(), issharedsr);

								if(issharedsr)
								{
									//is lvm SR
									islvmsr = false;
									if(get_sr_is_lvm(sruuid, islvmsr))
									{
										DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [sr attribute][uuid:%s] is LVM based SR: [%d]\n",
												FUNCTION_NAME, LINE_NO, FILE_NAME, sruuid.c_str(), islvmsr);

										if(islvmsr)
										{
											// VDI should on shared SR only (only shared vdi info reported to CX
											// To display in CX UI under pool
											// VDI should be on LVM based SR

											// This VDI is a shared VDI attached to pooled VM
											pvdilist.push_back(vdi);
										}
										else
										{
											//Not lvm based SR
										}
									}
									else
									{
										rv = false;
									}
								}
								else
								{
									//Not shared SR
								}
							}//Is shared SR
							else
							{
								rv = false;
							}
						}//Get SR					
						else
						{
							//Failed to get sr-uuid
							rv = false;						
						}			
					}
					else
					{
						//Read-only VDI;
					}
				}//Get VDI of current VBD
				else
				{
					rv = false;
				}
			}
			else
			{
				rv = false;
			}

		}//Each VBD
	}//Retrieved list of VBD's of pooled VM
	else
	{
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool get_all_vms(ClusterGroupInfos_t& vmlist)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	//Get Pooled VMs
	std::list<std::string> poolvms;
	if(!get_all_pooled_vms(poolvms))
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s : Failed to get pooled VMs \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME); 
		return false;//rv = false;
	}

	ClusterGroupInfo vminfo;
	std::string vmuuid, vmname;		
	std::list<std::string>::const_iterator pvmIter = poolvms.begin();

	for(;pvmIter != poolvms.end(); pvmIter++)
	{
		ClusterResourceInfos_t resources;
		std::vector<std::string> pvdlist;
		std::vector<std::string>::const_iterator vdIter;
		std::string vdiName, sruuid, devName, vgPart, lvPart;

		DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : Processing the pooled VM : %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, pvmIter->c_str());

		get_vm_param(*pvmIter, "name-label", vmname);		
		vminfo.id = *pvmIter;
		vminfo.name = vmname;

		DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : pooled VM UUID: %s\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vminfo.id.c_str());

		DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : pooled VM Name: %s\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vminfo.name.c_str());

		if(get_pooledvdi_on_pooledvm(*pvmIter, pvdlist))
		{
			DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : Number of pooled VDIs of pooled VM [%s:%s]: %d\n",
					FUNCTION_NAME, LINE_NO, FILE_NAME, vminfo.name.c_str(), vminfo.id.c_str(), pvdlist.size());

			vdIter = pvdlist.begin();
			for(; vdIter != pvdlist.end(); vdIter++)
			{
				ClusterResourceInfo resInfo;
				ClusterVolumeInfo volInfo;
				resInfo.id = *vdIter;
	
				//Get VDI Name & report
				vdiName.clear();				
				if(get_vdi_param(*vdIter, "name-label", vdiName))
				{
					resInfo.name = vdiName;
				}
				else
				{
					rv = false;
					break; //continue
				}

				DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : resource id [of VM [%s:%s]]: %s\n",
						FUNCTION_NAME, LINE_NO, FILE_NAME, vminfo.name.c_str(), vminfo.id.c_str(), resInfo.id.c_str());

				DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : resource name [of VM [%s:%s]]: %s\n",
						FUNCTION_NAME, LINE_NO, FILE_NAME, vminfo.name.c_str(), vminfo.id.c_str(), resInfo.name.c_str());

				//Get /dev/mapper device name for the virtual disk
				//VG_XenStorage--8ef1a9e7--e358--ef50--379b--6b0e4392f565-LV--cf6506c7--e2bc--41e6--8c45--31606025e81f

				if(!get_devmapper_path(*vdIter, devName))
				{
					//Failed to get dev mapper device name for the virtual disk. Proceed to next virtual disk entry
					DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s : Failed to get dev mapper device name for the virtual disk [%s]. Procesing next virtual disk.\n", LINE_NO, FILE_NAME, vdIter->c_str());
					continue;
				}
				
				volInfo.deviceName = devName;
				volInfo.deviceId = devName;

				resInfo.volumes.push_back(volInfo);						

				resources.push_back(resInfo);
			}
		}
		else
		{
			rv = false;
			break; //continue
		}

		vminfo.resources = resources;
		vmlist.push_back(vminfo);		
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

int getXenPoolInfo(ClusterInfo &clusInfo, VMInfos_t &vmInfoList, VDIInfos_t &vdiInfoList)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	int rv = 0; //Success

	ClusterGroupInfos_t vmlist;

	if(get_pool_param("uuid", clusInfo.id))
	{
		trim(clusInfo.id);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s : [get_pool_param] Failed to retrieve pool uuid\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return -1;
	}

	if(get_pool_param("name-label", clusInfo.name))
	{
		trim(clusInfo.name);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s : [get_pool_param] Failed to retrieve pool name\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return -1;
	}

	if(get_all_vms(vmlist))
	{
		DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : [get_all_vms] Successfully disovered all VMs in pool \n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		clusInfo.groups = vmlist;
		//printXenPoolInfo(clusInfo);

		for( std::vector<ClusterGroupInfo>::const_iterator vmIter = clusInfo.groups.begin(); vmIter != clusInfo.groups.end(); vmIter++ )
		{
			struct VMInfo vminfo;
			vminfo.uuid = vmIter->id;
			DebugPrintf(SV_LOG_INFO,"Function %s @LINE %d in FILE %s : vminfo.uuid [%s]\n",
					FUNCTION_NAME, LINE_NO, FILE_NAME, vmIter->id.c_str());

			//Fill the VM uuid in vmInfoList
			std::string residenthost, hostname, powerstate;

			if(!get_vm_param(vmIter->id, "resident-on", residenthost))
			{
				DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : [get_vm_param] Failed to get resdient host uuid of VM [%s]\n",
						FUNCTION_NAME, LINE_NO, FILE_NAME,vmIter->id.c_str());
			}
			else
			{
				hostname.clear();
				if(!get_host_param(residenthost, "name-label", hostname))
				{
					DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s : [get_host_param] Failed to get name of resdient host [%s]\n",
							FUNCTION_NAME, LINE_NO, FILE_NAME,residenthost.c_str());
				}
				else
				{
					//Fill the resident host name in the vmInfoList
					vminfo.residenton = hostname;
					DebugPrintf(SV_LOG_INFO,"Function %s @LINE %d in FILE %s : vminfo.residenton [%s]\n",
							FUNCTION_NAME, LINE_NO, FILE_NAME, hostname.c_str());
				}
			}
			if(!get_vm_param(vmIter->id, "power-state", powerstate))
			{
				DebugPrintf(SV_LOG_ERROR,
						"Function %s @LINE %d in FILE %s : [get_vm_param] Failed to get power-state of VM [%s}\n",
						FUNCTION_NAME, LINE_NO, FILE_NAME, vmIter->id.c_str());
			}
			else
			{
				//Fill the power-state of the VM
				vminfo.powerstate = powerstate;
				DebugPrintf(SV_LOG_INFO,
						"Function %s @LINE %d in FILE %s : vminfo.powerstate [%s]\n",
						FUNCTION_NAME, LINE_NO, FILE_NAME, vminfo.powerstate.c_str());
			}
			vmInfoList.push_back(vminfo);

			std::string vdi_description, vdi_devicenumber, devMapperPath;
			for( std::vector<ClusterResourceInfo>::const_iterator vdIter = vmIter->resources.begin();
					vdIter != vmIter->resources.end(); vdIter++)
			{
				struct VDIInfo vdiinfo;
				//Fill the VD uuid in vmInfoList vdIter->id

				vdiinfo.uuid = vdIter->id;
				DebugPrintf(SV_LOG_INFO,
						"Function %s @LINE %d in FILE %s : vdiinfo.uuid [%s]\n",
						FUNCTION_NAME, LINE_NO, FILE_NAME, vdiinfo.uuid.c_str());

				if(!get_vdi_param(vdIter->id, "name-description", vdi_description))
				{
					DebugPrintf(SV_LOG_ERROR,
							"Function %s @LINE %d in FILE %s : [get_vdi_param] Failed to get name-description of VDI [%s]\n",
							FUNCTION_NAME, LINE_NO, FILE_NAME,vdIter->id.c_str());
				}
				else
				{
					//Fill the name-description in the vdiInfoList vdi_description
					if(vdi_description.empty())
						vdi_description = "N/A";
					vdiinfo.vdi_description = vdi_description;

					DebugPrintf(SV_LOG_INFO,
							"Function %s @LINE %d in FILE %s : vdiinfo.vdi_description [%s]\n",
							FUNCTION_NAME, LINE_NO, FILE_NAME, vdiinfo.vdi_description.c_str());
				}
				if(!get_vbd_userdevice(vmIter->id, vdIter->id, "userdevice", vdi_devicenumber))
				{
					DebugPrintf(SV_LOG_ERROR,
							"Function %s @LINE %d in FILE %s : [get_vbd_userdevice] Failed to get vbd userdevice for VM [%s] VDI [%s] \n",
							FUNCTION_NAME, LINE_NO, FILE_NAME, vmIter->id.c_str(), vdIter->id.c_str());
				}
				else
				{
					//Fill the userdevice number of VD vdi_devicenumber
					vdiinfo.vdi_devicenumber = "Device-";
					vdi_devicenumber.assign(vdi_devicenumber, 1, 2);
					vdiinfo.vdi_devicenumber += vdi_devicenumber;
					DebugPrintf(SV_LOG_INFO,
							"Function %s @LINE %d in FILE %s : vdiinfo.vdi_devicenumber [%s]\n",
							FUNCTION_NAME, LINE_NO, FILE_NAME, vdiinfo.vdi_devicenumber.c_str());
				}
				if(get_devmapper_path(vdIter->id, devMapperPath))
				{
					bool avail = false;
					if(is_lvavailable(devMapperPath, avail))
					{
						vdiinfo.vdi_availability = avail;
						DebugPrintf(SV_LOG_INFO,
								"Function %s @LINE %d in FILE %s : vdiinfo.vdi_availability [%d]\n",
								FUNCTION_NAME, LINE_NO, FILE_NAME, vdiinfo.vdi_availability);
					}
				}
				vdiInfoList.push_back(vdiinfo);
			}
		}

		rv = 0;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, 
				"Function %s @LINE %d in FILE %s : [get_all_vms] Failed to retrieve all vms in pool\n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		rv = -1;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool getAllLogicalVolumes(std::list<std::string>& lvdevices)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	//Get All LVs
	bool rv = true;
	std::string cmd;

	cmd = "vgdisplay -v 2>/dev/null | grep -e 'LV Name'";

	std::stringstream lvs;
	if (!executePipe(cmd, lvs))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		return false; //rv = false;
	}

	std::string devicename, lvtok, nametok;
	char line[128];
	while(!lvs.eof() && lvs.good())
	{
		lvs >> lvtok >> nametok >> devicename;
//		lvs.getline(line, sizeof(line));
		trim(devicename);
		lvdevices.push_back(devicename);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

int getAvailableVDIs(VDIInfos_t &vdiInfoList)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	std::list<std::string> lvdevices;
	if(getAllLogicalVolumes(lvdevices))
	{
		bool avail;
		size_t vgleft, lvleft, vduuidstart;
		std::string lvname, devMapperPath, vgPart, lvPart;
		for( std::list<std::string>::const_iterator lvIter = lvdevices.begin();lvIter != lvdevices.end(); lvIter++)
		{
			avail = false;
			struct VDIInfo vdiinfo;
			lvname = *lvIter;

			//Get devmapper path for each LV
			vgleft = 0;
	       	lvleft = 0;
			vduuidstart = 0;
			vgPart.clear();
			lvPart.clear();

			vgleft = lvname.find_first_of('/',1);
			lvleft = lvname.find_last_of('/');
			vduuidstart = lvname.find('-', lvleft);

			//Extract uuid from devicemapper path
			vdiinfo.uuid = lvname.substr(vduuidstart+1);

			vgPart = lvname.substr(vgleft+1, lvleft-vgleft-1);  
			replaceHyphen(vgPart);

			lvPart = lvname.substr(lvleft+1);
			replaceHyphen(lvPart);

			devMapperPath = DEV_MAPPER_PATH + vgPart + "-" + lvPart;			
			if(is_lvavailable(devMapperPath, avail))
			{
				vdiinfo.vdi_availability = avail;
				DebugPrintf(SV_LOG_INFO,
						"Function %s @LINE %d in FILE %s : vdiinfo.vdi_availability [%d]\n",
						FUNCTION_NAME, LINE_NO, FILE_NAME, vdiinfo.vdi_availability);
			}

			if(avail)
			{
				vdiInfoList.push_back(vdiinfo);
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);	
	return 0;
}

int isXenPoolMember(bool& isMember, PoolInfo& pInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	std::string pooluuid, poolname;

	if(get_pool_param("uuid", pooluuid))
	{
		trim(pooluuid);
		if( !pooluuid.empty())
		{
			DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : [get_pool_param] pool uuid [%s]\n",
					FUNCTION_NAME, LINE_NO, FILE_NAME, pooluuid.c_str());
			pInfo.uuid = pooluuid;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s : [get_pool_param] Failed to retrieve pool uuid\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return -1;
	}

	if(get_pool_param("name-label", poolname))
	{
		trim(poolname);
		if( !poolname.empty())
		{
			DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : [get_pool_param] pool name [%s]\n",
					FUNCTION_NAME, LINE_NO, FILE_NAME, poolname.c_str());
			pInfo.name = poolname;
			isMember = true;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : [get_pool_param] NOT a pool member\n",	
					FUNCTION_NAME, LINE_NO, FILE_NAME);		
			isMember = false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s : [get_pool_param] Failed to retrieve pool name\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return -1;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return 0; //Success
}

int isPoolMaster(bool& isMaster)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	std::string hostuuid, masteruuid;

	if(get_pool_param("master", masteruuid))
	{
		trim(masteruuid);
		if( !masteruuid.empty())
		{
			DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : [get_pool_param] master uuid [%s]\n",
					FUNCTION_NAME, LINE_NO, FILE_NAME, masteruuid.c_str());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s : [get_pool_param] Failed to retrieve pool uuid\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return -1;
	}

	std::string hostname, thishost;
	
	if(!get_hostname(hostname))
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Failed in get_hostname\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return -1;
	}
	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Local host name: [%s]\n",
			FUNCTION_NAME, LINE_NO, FILE_NAME, hostname.c_str());
	
	if(!get_this_host(hostname, hostuuid))
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Failed in get_this_host\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return -1;
	}

	if( !hostuuid.empty())
	{
		DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : [get_host_param] host uuid [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, hostuuid.c_str());
		if(hostuuid.compare(masteruuid) == 0)
		{
			DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : This is pool master with uuid [%s]\n",
					FUNCTION_NAME, LINE_NO, FILE_NAME, hostuuid.c_str());
			isMaster = true;
		}
	}	
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return 0; //Success
}
/*
bool get_uuid_device(const std::string uuid, std::string& devMapperDeviceName)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	//Get SR uuid
	//Construct lv name from SR uuid & VDI uuid
	//Validate the lv name to decide the lvname prefix whether VHD- or LV-
	//Construct dev mapper device name from VDI uuid & SR uuid

	std::string sruuid;

	if( !get_vdi_param(uuid, "sr-uuid", sruuid) )
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Failed to retrive SR uuid of VDI [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, uuid.c_str());
		rv = false;
	}
	else
	{
		bool bVHD;
		string lvdevice, vgName, lvName;

		lvdevice = "/dev/";
		vgName = VG_NAME_PREFIX + sruuid;
		lvName = VHD_NAME_PREFIX + uuid;
		lvdevice += vgName + "/" + lvName;

		bVHD = true;
		if(isValidLV(lvdevice, bVHD))
		{
			if(!bVHD)
			{
				lvName = LV_NAME_PREFIX + uuid;
			}
		}

		replaceHyphen(vgName);
		replaceHyphen(lvName);

		//By default consider VHD virtual disk
		devMapperDeviceName = DEV_MAPPER_PATH + vgName + "-" + lvName;

		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Dev mapper device corresponding to VDI [%s] <=> [%s}\n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME, uuid.c_str(), devMapperDeviceName.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}
*/
bool get_device_uuid(const std::string device, std::string& uuid)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	size_t pos = 0;
	
	if( device.find("VG_XenStorage", 0) == std::string::npos )
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Non XenServer storage entity : %s\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, device.c_str());
		rv = false;
	}
	else if( (pos = device.find("-LV-", 0)) != std::string::npos )
	{
		pos += strlen("-LV--");
		uuid.assign(device, pos, device.length());		
		std::string::iterator it = unique(uuid.begin(), uuid.end(), IsRepeatingHyphen);
		uuid.erase(it, uuid.end());
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : device [%s] : vdi-uuid [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, device.c_str(), uuid.c_str());
		rv = true;
	}
	else if( (pos = device.find("-VHD-", 0)) != std::string::npos )
	{
		pos += strlen("-VHD--");
		uuid.assign(device, pos, device.length());
		std::string::iterator it = unique(uuid.begin(), uuid.end(), IsRepeatingHyphen);
		uuid.erase(it, uuid.end());
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : device [%s] : vdi-uuid [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, device.c_str(), uuid.c_str());
		rv = true;
	}

	else
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Non dev mapper device : %s\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, device.c_str()); 
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool is_shared_vdi(const std::string vdiuuid, bool &issharedvdi)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	std::string sruuid;
	//Get SR of VDI
	if(get_vdi_param(vdiuuid, "sr-uuid", sruuid))
	{
		//Is on shared SR
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : VDI [%s] attribute [sr-uuid]:[%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vdiuuid.c_str(), sruuid.c_str());

		bool issharedsr = false;
		if(get_sr_shared(sruuid, issharedsr))
		{
			if(issharedsr)
			{
				issharedvdi = true;
			}
			else
			{
				issharedvdi = false;
				DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : sr [%s] is not shared\n",
						FUNCTION_NAME, LINE_NO, FILE_NAME, sruuid.c_str());
			}
		}
		else
		{
			rv = false;
		}
	}
	else
	{
		//Failed to get sr-uuid
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}
bool get_hostname(std::string& hostname)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	
	std::string cmd;
	
	cmd = "hostname";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		return false; //rv = false;
	}

	hostname = results.str();
	trim(hostname);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	
	if(hostname.empty())
		return false;

	return rv;
}

bool get_this_host(const std::string hostname, std::string& thishost)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	std::string cmd;

	cmd = " xe host-list params=uuid name-label=" + hostname;
	cmd += " | grep :  | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		return false; //rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        return false;
    }

	thishost = results.str();
	trim(thishost);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	if(thishost.empty())
		return false;
	return rv;
}

bool get_all_vms_on_host(const std::string hostuuid, std::list<std::string>& allvms)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	std::string cmd;

	cmd = "xe vm-list params=uuid resident-on=" + hostuuid;
	cmd += " is-control-domain=false is-a-template=false | grep :  | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;
	std::string vm;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		return false; //rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        return false;
    }

	while(!results.eof())
	{
		vm.clear();
		results >> vm;
		trim(vm);
		if(!vm.empty())
			allvms.push_back(vm);
	}

	std::stringstream haltedvms;
	cmd.clear();

	cmd = "xe vm-list params=uuid resident-on=\"<not in database>\" " ;
	cmd += " is-control-domain=false is-a-template=false | grep :  | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	if (!executePipe(cmd, haltedvms))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		return false; //rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	while(!haltedvms.eof())
	{
		vm.clear();	
		haltedvms >> vm;
		trim(vm);
		if(!vm.empty())
			allvms.push_back(vm);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool get_all_vms_on_localhost(std::list<std::string>& allvms)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	std::string hostname, thishost;

	if(!get_hostname(hostname))
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Failed in get_hostname\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return false;
	}
	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Local host name: [%s]\n",
			FUNCTION_NAME, LINE_NO, FILE_NAME, hostname.c_str());


	if(!get_this_host(hostname, thishost))
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Failed in get_this_host\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return false;
	}

	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Current host: [%s] \n",
			FUNCTION_NAME, LINE_NO, FILE_NAME, thishost.c_str());

	if(!get_all_vms_on_host(thishost, allvms))
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Failed in get_all_vms_on_host\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME);
		return false;
	}
	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Current host: [%s] Resident VM count: [%d]\n",
			FUNCTION_NAME, LINE_NO, FILE_NAME, thishost.c_str(), allvms.size());


	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool get_globalvmlist(std::list<std::string>& globallist)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	std::string cmd;

	cmd = "xe vm-list params=uuid";
	cmd += " is-control-domain=false is-a-template=false | grep :  | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;
	std::string vm;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		return false; //rv = false;
	}

	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        rv = false;
    }

	while(!results.eof())
	{
		vm.clear();
		results >> vm;
		trim(vm);
		if(!vm.empty())
			globallist.push_back(vm);
	}  

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}
bool get_globallist(std::string par , std::list<std::string>& globallist){


	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	std::string cmd;

	cmd = "xe ";
	cmd += par;
	cmd += "-list params=uuid";
	cmd += " is-control-domain=false is-a-template=false | grep :  | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;
	std::string pp;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		return false; //rv = false;
	}
	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        return false;
    }

	while(!results.eof())
	{
		pp.clear();
		results >> pp;
		trim(pp);
		if(!pp.empty())
			globallist.push_back(pp);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

// set owner as true for shared & non_owned
int is_xsowner(const std::string device, bool& owner)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	int rv = 0;
	owner = false; //Non-owner

	std::string uuid; //VDI uuid
	if(!get_device_uuid(device, uuid))
	{
		owner = true;
		return 0; //should take bool variable as output parameter; This return value should only be used for exit codes
	}
	else if(uuid.empty())
	{
		owner = true;
		return 0;
	}

	DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [vdi attribute][uuid:%s] \n",
			FUNCTION_NAME, LINE_NO, FILE_NAME, uuid.c_str());         
	
	bool issharedvdi = false;	
	if(!is_shared_vdi(uuid, issharedvdi))
	{
		return -1;
	}
	else
	{
		if(issharedvdi)
		{
			//cout << "\nShared...\n";
			DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [vdi attribute][uuid:%s] is shared: [%d]\n",
					FUNCTION_NAME, LINE_NO, FILE_NAME, uuid.c_str(), issharedvdi);

			//Shared VDI
			//Now check if it is not owned by this host
			//Get all VMs on this host
			std::list<std::string> allvms;
			if(!get_all_vms_on_localhost(allvms))
			{
				return -1;
			}
			else
			{
				if(allvms.size() == 0)
				{	
					//No VMs running on this host to own this
					owner = false;
					return 0;
				}
				//Get all VDIs of each VM
				std::list<std::string>::const_iterator vmIter;				
				for(vmIter = allvms.begin(); vmIter != allvms.end(); vmIter++)
				{
					//cout << "vmIter = " << *vmIter << endl;
					DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [vm attribute][uuid:%s]\n",
							FUNCTION_NAME, LINE_NO, FILE_NAME, vmIter->c_str());

					std::vector<std::string> vdilist;
					if(!get_pooledvdi_on_pooledvm(*vmIter, vdilist))
					{
						return -1;
					}
					else
					{
						std::vector<std::string>::const_iterator vdIter;
						for(vdIter = vdilist.begin(); vdIter != vdilist.end(); vdIter++)
						{	//compare uuid of each VDI with uuid
							//Match => owner = false;
							DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : [vdi of local VM][uuid:%s] == device [uuid:%s]\n",                                                               FUNCTION_NAME, LINE_NO, FILE_NAME, vdIter->c_str(), uuid.c_str());

							if( vdIter->compare(uuid) == 0 )
							{
								DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n non owned & shared VDI [%s]", FUNCTION_NAME, uuid.c_str());
								owner = true;
								return 0;
							}							
						}
					}
				}
			}
		}
		else
		{
			owner = true;
			return 0;
		}
		rv = 0;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return 0;
}
bool get_devmapper_path(const std::string vdiuuid, std::string& devMapperPath)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	//Get SR uuid
	//Construct lv name from SR uuid & VDI uuid
	//Validate the lv name to decide the lvname prefix whether VHD- or LV-
	//Construct dev mapper device name from VDI uuid & SR uuid

	std::string sruuid;

	if( !get_vdi_param(vdiuuid, "sr-uuid", sruuid) )
	{
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Failed to retrive SR uuid of VDI [%s]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, vdiuuid.c_str());
		rv = false;
	}
	else
	{
		bool bVHD;
		std::string lvdevice, vgName, lvName;

		lvdevice = "/dev/";
		vgName = VG_NAME_PREFIX + sruuid;
		lvName = VHD_NAME_PREFIX + vdiuuid;
		lvdevice += vgName + "/" + lvName;

		bVHD = true;
		if(isValidLV(lvdevice, bVHD))
		{
			if(!bVHD)
			{
				lvName = LV_NAME_PREFIX + vdiuuid;
			}
		}

		replaceHyphen(vgName);
		replaceHyphen(lvName);

		//By default consider VHD virtual disk
		devMapperPath = DEV_MAPPER_PATH + vgName + "-" + lvName;

		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : Dev mapper device corresponding to VDI [%s] <=> [%s}\n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME, vdiuuid.c_str(), devMapperPath.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool get_motion_task(std::list<std::string>& tasklist)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	std::string task;
	std::string cmd;

	cmd = "xe task-list name-label=Async.VM.pool_migrate status=pending params=uuid";
	cmd += " | grep :  | cut -d: -f2 | cut -c2-";
	cmd += " 2> /dev/null ";

	std::stringstream results;

	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR,
				"Function %s @LINE %d in FILE %s :failed to execute cmd %s \n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, cmd.c_str());
		rv = false;
	}
	if(XAPI_LOST_CONNECTION == results.str())
    {
        DebugPrintf(SV_LOG_ERROR,"Function %s @LINE %d in FILE %s : XAPI service is not running\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        return false;
    }

	while(!results.eof())
	{
		task.clear();
		results >> task;
		trim(task);
		DebugPrintf(SV_LOG_DEBUG,"%s: task = %s\n", FUNCTION_NAME, task.c_str());
		if(!task.empty())
			tasklist.push_back(task);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool is_lvavailable(const std::string lv, bool& avail)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	struct stat st;
	avail = false;

	if(stat(lv.c_str(), &st) < 0)
	{
		if(errno == ENOENT || errno == ENOTDIR)
		{
			DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : LV NOT Available [%s]\n",
					FUNCTION_NAME, LINE_NO, FILE_NAME, lv.c_str());
		}
		else if(errno == ENAMETOOLONG )
		{
			 //std::cout << "File name is too long \n";
			 DebugPrintf(SV_LOG_ERROR,
					 "Function %s @LINE %d in FILE %s :File name is too long: %s\n",
					 FUNCTION_NAME, LINE_NO, FILE_NAME, lv.c_str());

			return false;
		}
		DebugPrintf(SV_LOG_DEBUG, "Function %s @LINE %d in FILE %s : Errno = %d\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, errno);
	}
	else
	{
		avail = true;
		DebugPrintf(SV_LOG_INFO, "Function %s @LINE %d in FILE %s : LV Status [%d]\n",
				FUNCTION_NAME, LINE_NO, FILE_NAME, avail);  
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return true;
}
