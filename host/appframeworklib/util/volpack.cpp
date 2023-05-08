#include "volpack.h"
#include "appscheduler.h"
#include "appcommand.h"
#include "util/common/util.h"
#include <boost/lexical_cast.hpp>
#include "config/applocalconfigurator.h"
#include <boost/algorithm/string.hpp>
#include "controller.h"

std::string VolPack::getCdpcliPath()
{
	 DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	 std::string strCdpcliAbsolutePath ;
     AppLocalConfigurator localconfigurator ;
	 strCdpcliAbsolutePath += localconfigurator.getInstallPath();
#ifdef SV_WINDOWS
	 strCdpcliAbsolutePath += ACE_DIRECTORY_SEPARATOR_CHAR;
	 strCdpcliAbsolutePath += "cdpcli.exe";
#else
	strCdpcliAbsolutePath += "bin" ;
    strCdpcliAbsolutePath += ACE_DIRECTORY_SEPARATOR_CHAR ;
    strCdpcliAbsolutePath += "cdpcli";
#endif
	 DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	 return strCdpcliAbsolutePath;
}
bool VolPack::volpackProvision(std::string& outputFileName,SV_ULONG &exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRetValue = false;
    bool compressVolpack = false;
	SV_ULONGLONG mntPtSize;
    AppLocalConfigurator appLocalConfigurator ;
	exitCode = 1 ;
	std::string strCommnadToExecute ;
	std::string strCommandToCreateSparseFile;
	std::string strCommandToMountSparseFile;
	std::string strSparseFileName;
	std::string strOutPutString;
    compressVolpack = appLocalConfigurator.VirtualVolumeCompressionEnabled();
	strCommnadToExecute += std::string("\"");
	strCommnadToExecute += std::string(getCdpcliPath());
	strCommnadToExecute += std::string("\"");
	strCommnadToExecute += std::string(" ");
	strCommnadToExecute += std::string(" --virtualvolume");
	
	//STEP 1. Create the SparseFile 
	strCommandToCreateSparseFile = strCommnadToExecute + std::string(" --op=createsparsefile");

	strCommandToCreateSparseFile += std::string("  --filepath=\"");
	strSparseFileName = m_MountPointName + std::string("_sparsefile");
	strCommandToCreateSparseFile += strSparseFileName + "\"" ;
	strCommandToCreateSparseFile += std::string(" --size=");
	try
	{
		mntPtSize = m_volPackSize;
		//Convert the bytes into MB.
		mntPtSize = mntPtSize/(1024*1024);
        if(  mntPtSize % (1024 * 1024 ) != 0 )
        {
            mntPtSize += 1 ;
        }
		m_volPackSize = mntPtSize;
	}
	catch( const boost::bad_lexical_cast & exObj)
	{
		DebugPrintf(SV_LOG_ERROR,"Exception caught. bad cast .Type %s",exObj.what());
		exitCode = 1;
		m_stdOut = exObj.what();
		return false;
	}

	strCommandToCreateSparseFile += boost::lexical_cast<std::string>(m_volPackSize);
    if( compressVolpack )
    {
	    strCommandToCreateSparseFile += std::string("  --compression=yes");
    }
    else
    {
	    strCommandToCreateSparseFile += std::string("  --compression=no");
    }
    AppCommand objAppCommand(strCommandToCreateSparseFile,m_uTimeOut,outputFileName,true);
	bool bActive = true;
	void *h = 0;
#ifdef SV_WINDOWS
	h = Controller::getInstance()->getDuplicateUserHandle();
#endif
	if(objAppCommand.Run(exitCode,m_strStdOut, bActive , "", h ) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to spawn cdpcli.exe for command %s\n",strCommandToCreateSparseFile.c_str());
	}
	else
	{
		if(exitCode ==0)
		{
			DebugPrintf(SV_LOG_INFO,"Successfully created the SparseFile \n");
			bRetValue = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to create SparseFile. Error Code :%d\n",exitCode);
		}
	}
#ifdef SV_WINDOWS
	if(h)
	{
		CloseHandle(h);
	}
#endif
	//STEP 2. Mount the sparse filepath
	if(	bRetValue)
	{
		strCommandToMountSparseFile += strCommnadToExecute;
#ifdef SV_WINDOWS
		strCommandToMountSparseFile += std::string(" --op=mount ");
#else
        strCommandToMountSparseFile += std::string(" --op=createvolume ");
#endif
		strCommandToMountSparseFile += std::string(" --filepath=\"");
		strCommandToMountSparseFile += strSparseFileName + "\"" ;
#ifdef SV_WINDOWS
		strCommandToMountSparseFile += std::string("  --drivename=\"");
		AppLocalConfigurator configurator ;
        std::string mountPointName ;
        std::string::size_type pos = m_MountPointName.find( "\\\\" ) ;
        if( pos == std::string::npos || pos > 0 ) 
		    mountPointName = m_MountPointName + std::string("_virtualvolume");
        else
        {
            mountPointName = configurator.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A ;
            mountPointName +=   "volpacks"  ;
            pos = m_MountPointName.find_last_of( ACE_DIRECTORY_SEPARATOR_STR_A ) ;
            if( pos != std::string::npos )
            {
                mountPointName += m_MountPointName.substr( pos ) + "_virtualvolume" ;
            }
        }
	
        std::string drivename = mountPointName.substr(0, mountPointName.find_first_of("\\") + 1 ) ;
        std::string relativepath = mountPointName.substr(mountPointName.find_first_of("\\") + 1 ) ;
        boost::algorithm::to_upper(drivename) ;
        boost::algorithm::to_lower(relativepath) ;
        mountPointName = drivename + relativepath ;
		strCommandToMountSparseFile += mountPointName + "\"" ;
		m_finalMountPoint = mountPointName;
#endif
		AppCommand objAppCommand(strCommandToMountSparseFile,m_uTimeOut,outputFileName,true);
		m_strStdOut.clear();
		bool bActive = true;
		ACE_OS::sleep (10);
		void *h = 0;
#ifdef SV_WINDOWS
		h = Controller::getInstance()->getDuplicateUserHandle();
#endif
		if(objAppCommand.Run(exitCode,m_strStdOut,bActive, "", h ) != SVS_OK)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to mount the sparsefile %s\n",strCommandToMountSparseFile.c_str());
			bRetValue = false;
		}
		else
		{
			bRetValue = false;
			strOutPutString = m_strStdOut;
			if(exitCode == 0)
			{
				DebugPrintf(SV_LOG_INFO,"Successfully mounted SparseFile. \n");
                            bRetValue = true;
                        }
                        else
                        {
                DebugPrintf(SV_LOG_ERROR,"Failed to mount the sparseFile. Error Code :%d\n",exitCode);
            }
        }
#ifdef SV_WINDOWS
		if(h)
		{
			CloseHandle(h);
		}
#endif
	}
	m_stdOut = strOutPutString;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRetValue;
}

bool VolPack::volpackDeletion(std::string& outputFileName,SV_ULONG& exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false;
	bool bActive = true;
	exitCode = 1 ;
    std::string cmdString;
    cmdString += std::string("\"");
	cmdString += std::string(getCdpcliPath());
	cmdString += std::string("\"");
	cmdString += std::string(" ");
	cmdString += std::string(" --virtualvolume");
    cmdString += std::string(" --op=unmount ");
	cmdString += std::string("  --drivename=");
	cmdString += m_MountPointName;

    DebugPrintf(SV_LOG_INFO,"\n The volpack deletion command to execute : %s\n",cmdString.c_str());
    AppCommand objAppCommand(cmdString,m_uTimeOut,outputFileName,true);
	void *h = 0;
#ifdef SV_WINDOWS
	h = Controller::getInstance()->getDuplicateUserHandle();
#endif
	if(objAppCommand.Run(exitCode,m_stdOut, bActive , "", h ) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to spawn cdpcli.exe for command %s\n",cmdString.c_str());
	}
	else
	{
		if(exitCode ==0)
		{
			DebugPrintf(SV_LOG_INFO,"Successfully deleted the volpack \n");
			bRet = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to delete volpack. Error Code :%ld\n",exitCode);
		}
	}
#ifdef SV_WINDOWS
	if(h)
	{
		CloseHandle(h);
	}
#endif
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

bool VolPack::volpackResize(std::string& outputFileName,SV_ULONG& exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false;
	SV_ULONGLONG mntPtSize = 0;
	exitCode = 1 ;
    std::string cmdString;
    cmdString += std::string("\"");
	cmdString += std::string(getCdpcliPath());
	cmdString += std::string("\"");
	cmdString += std::string(" ");
	cmdString += std::string(" --virtualvolume");
    cmdString += std::string(" --op=resize ");
	cmdString += std::string("  --devicename=");
	cmdString += m_MountPointName;
	cmdString += std::string("  --size=");
	try
    {
        mntPtSize = m_volPackSize;
        //Convert the bytes into MB.
        mntPtSize = mntPtSize/(1024*1024);
        if(  mntPtSize % (1024 * 1024 ) != 0 )
        {
            mntPtSize += 1 ;
        }
        m_volPackSize = mntPtSize;
    }
    catch( const boost::bad_lexical_cast & exObj)
    {
        DebugPrintf(SV_LOG_ERROR,"Exception caught. bad cast .Type %s",exObj.what());
        exitCode = 1;
        m_stdOut = exObj.what();
        return false;
    }

    cmdString += boost::lexical_cast<std::string>(m_volPackSize);


    DebugPrintf(SV_LOG_INFO,"\n The volpack resize command to execute : %s\n",cmdString.c_str());
    AppCommand objAppCommand(cmdString,m_uTimeOut,outputFileName,true);
	void *h = 0;
#ifdef SV_WINDOWS
	h = Controller::getInstance()->getDuplicateUserHandle();
#endif
	if(objAppCommand.Run(exitCode,m_stdOut, Controller::getInstance()->m_bActive, "", h) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to spawn cdpcli.exe for command %s\n",cmdString.c_str());
	}
	else
	{
		if(exitCode ==0)
		{
			DebugPrintf(SV_LOG_INFO,"Successfully resize the volpack \n");
			bRet = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to resize volpack. Error Code :%ld\n",exitCode);
		}
	}
#ifdef SV_WINDOWS
	if(h)
	{
		CloseHandle(h);
	}
#endif
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}
