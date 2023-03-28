#include "volproxyutil.h"
#include <boost/algorithm/string/replace.hpp>
#include "inmsdkutil.h"

bool RunCDPCLI( const std::vector<std::string>& cdpcliArgs, CDPCli::cdpProgressCallback_t cdpProgressCallback,
                                                  void * cdpProgressCallbackContext )
{
    char ** argv ;
    int argc = cdpcliArgs.size() ;
    std::string command ;
    argv = new char*[cdpcliArgs.size()  ] ;
    
    for( int nIndex = 0 ; nIndex < (cdpcliArgs.size() ); nIndex++ )
    {
        argv[nIndex] = (char*)cdpcliArgs[nIndex].c_str() ;
        command += (char*)cdpcliArgs[nIndex].c_str() ; 
        command += " " ;
    }

    CDPCli cli(argc, (char**) argv);
    cli.skip_log_initialization() ;
    cli.skip_quitevent_initialization() ;
    cli.skip_platformdeps_initialization() ;
    cli.skip_tal_initialization() ;
    if( cdpProgressCallbackContext != NULL )
        cli.set_progress_callback( cdpProgressCallback, cdpProgressCallbackContext ) ;
    DebugPrintf( SV_LOG_DEBUG, "CDPCLI Command: %s\n", command.c_str() ) ;
    bool bReturn = cli.run() ;

    delete [] argv ;
    return bReturn ;
}

bool mountVirtualVolume(const std::string& volpackName,
                        const std::string& SrcVolume, 
                        std::string& volpackMountPath,
						bool& alreadyMounted)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool bret = false;
    LocalConfigurator localConfigurator ;
    std::string FormattedvolpackName = volpackName ;
    int counter = localConfigurator.getVirtualVolumesId();
    bool volPackMounted = false ;
    std::string spareFilePath = volpackName ;
    boost::algorithm::replace_all(spareFilePath, "_virtualvolume", "_sparsefile") ;	
	bool isUncPath = spareFilePath[0] == '\\' && spareFilePath[1] == '\\' ? true : false ;
	while(counter != 0) {
        char regData[26];
		inm_sprintf_s(regData, ARRAYSIZE(regData), "VirVolume%d", counter);

        std::string data = localConfigurator.getVirtualVolumesPath(regData);

        std::string sparsefilename;
        std::string volume;

        if (ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
        {
            FormatVolumeNameForCxReporting(sparsefilename);
            FormatVolumeNameForCxReporting(spareFilePath ) ;
            if( InmStrCmp<NoCaseCharCmp>(sparsefilename, spareFilePath) == 0 )
            {
                DebugPrintf(SV_LOG_DEBUG, "Volume already mounted..\n") ;
                volpackMountPath = volume ;
                volPackMounted = true ;
                bret = true ;
                break;
            }
        }        
        counter--;
    }
	if( isUncPath )
	{
		spareFilePath = "\\\\" + spareFilePath ;
	}
    if(!volPackMounted)
	{
        std::vector<std::string> cdpcliArgs ;
        cdpcliArgs.push_back( "cdpcli" ) ;
        cdpcliArgs.push_back( "--virtualvolume" ) ;
 #ifdef SV_WINDOWS
        cdpcliArgs.push_back( "--op=mount" ) ;        
#else
        cdpcliArgs.push_back( "--op=createvolume");
#endif        
        cdpcliArgs.push_back( "--filepath=" + spareFilePath ) ;
 #ifdef SV_WINDOWS
        GetMountNameFromPath(volpackName, volpackMountPath) ;
        cdpcliArgs.push_back( "--drivename=" + volpackMountPath ) ;
#else
        volpackMountPath = spareFilePath ;
#endif
        bret = RunCDPCLI( cdpcliArgs ) ;
        if( bret == false )
        {
            DebugPrintf(SV_LOG_DEBUG,"volpack mount failed for volpack:%s",volpackName.c_str());
        }      
		alreadyMounted = false ;
	}
	else
	{
		alreadyMounted = true ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return bret;
}

bool getlocalpath(ExportedRepoDetails& exprepoobj,std::string& localPath) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool bret = false;
	CIFSShareMap_t::const_iterator cifsIter = exprepoobj.CIFS.begin();
    if( cifsIter == exprepoobj.CIFS.end() )
    {
        localPath = getConfigStorePath() ;
        bret = true ;
    }
	while(cifsIter != exprepoobj.CIFS.end())
	{			
        std::string SharePath ;		
        getSharedPathAndLocalPath( cifsIter->second.TargetIP, cifsIter->second.ShareName, SharePath, localPath ) ;
        if(InmStrCmp<NoCaseCharCmp>(cifsIter->second.ReturnCode.c_str(), "0" ) == 0 ) //.Status.c_str(),"Export Sucess")==0)
		{
			if(mountRepository(SharePath,localPath))
			{
				bret = true;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Error Failed to mount the sharepath:%s locally ",localPath.c_str());
			}
		}
		cifsIter++;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return bret;
}