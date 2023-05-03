#include "configvalueobj.h"
#include <ace/Configuration.h> 
#include <ace/Configuration_Import_Export.h>
#include <ace/Get_Opt.h>

#include "inmsafecapis.h"

ConfigValueObj ConfigValueObj::m_confValueObjInstance ;
InmProtectedAppType getAppliacationTypeFromString(std::string appName)
{
    InmProtectedAppType appType ;
    if(strcmpi(appName.c_str(), "fileserver") == 0)
    {
       appType = INM_APP_FILESERVER;
    }
    else if(strcmpi(appName.c_str(), "sql") == 0)
    {
        appType = INM_APP_MSSQL;
    }
    else if(strcmpi(appName.c_str(), "exchange") == 0)
    {
        appType = INM_APP_EXCHNAGE;
    }
    else if(strcmpi(appName.c_str(), "bes") == 0)
    {
        appType = INM_APP_BES;
    }
    else 
    {
        appType = INM_APP_NONE;
    }
    return appType;
}

bool setLongOption( ACE_Get_Opt& cmd_opts)
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ ) ;
	
	bool retVal = true ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	if( cmd_opts.long_option(ACE_TEXT("appname"),'a',ACE_Get_Opt::ARG_REQUIRED) == -1)
    {    
       return false ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retVal;
}

ConfigValueObj::ConfigValueObj()
:m_runOnconsole(false)
{
}
void ConfigValueObj::setRunOnconsole(bool value)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	m_runOnconsole = value;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ConfigValueObj::getRunOnconsole()
{
	return m_runOnconsole;
}

void ConfigValueObj::setApplication(InmProtectedAppType appType)
{
	m_applications.insert(appType);
}

std::set<InmProtectedAppType> ConfigValueObj::getApplications() 
{
	return m_applications;
}
ConfigValueObj& ConfigValueObj::getInstance()
{
    return m_confValueObjInstance;
}
bool ConfigValueObj::parse(ACE_TCHAR** argv, int argc)
{
	bool bRet = true;
    m_confValueObjInstance.setRunOnconsole(true);

    InmProtectedAppType appType;
	// PR#10815: Long Path support
	/*ACE_Get_Opt uses ACE_TCHAR. So 'argv' should be 'ACE_TCHAR**' */
	ACE_Get_Opt cmd_opts(argc, argv);
	if( setLongOption(cmd_opts) == false )
	{
		bRet = false ;
	}
	else
	{
		int option;
		ACE_TCHAR optionVal[ 100 ];
		while ((option = cmd_opts ()) != EOF)
		{
			switch (option) 
			{
				case 'a':
                    inm_wcscpy_s(optionVal, ARRAYSIZE(optionVal), cmd_opts.opt_arg());
					// PR#10815: Long Path support
                    appType = getAppliacationTypeFromString(ACE_TEXT_ALWAYS_CHAR(optionVal));
                    if(appType != INM_APP_NONE)
                    {
                        m_confValueObjInstance.setApplication(appType);
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "Invalid Application Name..\n") ;
                        bRet = false ;
                    }
					break ;
				default:
					DebugPrintf(SV_LOG_ERROR, "Invalid argument..\n") ;
					bRet = false ;
			 }
			 if(bRet == false )
			 {
				break;
			 }
		 }
	 }
	 if(bRet == false)
	 {
		usage();
	 }
	return bRet;
}


void usage()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ ) ;
    printf("AppWizard.exe --appname=fileserver --appname=exchange --appname=sql \n");
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}


bool ConfigValueObj::isApplicationSelected(InmProtectedAppType appType)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__ ) ;
    bool bRetValue = false;
    if( find(m_applications.begin(), m_applications.end(), appType) != m_applications.end() )
    {
        bRetValue = true;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return bRetValue;
}