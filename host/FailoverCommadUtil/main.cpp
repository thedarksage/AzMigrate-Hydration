#include <ace/Init_ACE.h>

#include <iostream>
#include "failovercommandutil.h"
#include "config/appconfigurator.h"
#include "logger.h"
#include "util/common/util.h"
#include <boost/lexical_cast.hpp>
#include "cdputil.h"
#include "controller.h"
#include "configurator2.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

using namespace std;

Configurator* TheConfigurator = 0; // singleton

bool SetupLocalLogging()
{
    bool rv = false;
    try 
    {

        LocalConfigurator localconfigurator;
        std::string sLogFileName = localconfigurator.getLogPathname()+ "failovercommandutil.log";
        SetLogFileName( sLogFileName.c_str() );
        SetLogLevel( localconfigurator.getLogLevel() );		
        SetLogHttpIpAddress(GetCxIpAddress().c_str());
        SetLogHttpPort( localconfigurator.getHttp().port );
        SetLogHostId( localconfigurator.getHostId().c_str() );
        SetLogRemoteLogLevel( localconfigurator.getRemoteLogLevel() );
		SetSerializeType( localconfigurator.getSerializerType() ) ;
		SetLogHttpsOption(localconfigurator.IsHttps());
        rv = true;

    }
    catch(...) 
    {
        std::cerr << "Local Logging initialization failed.\n";
        rv = false;
    }

    return rv;
}


void InitCDPQuitEvent()
{
    try
    {
        if(!CDPUtil::InitQuitEvent())
        {
            DebugPrintf( SV_LOG_ERROR, "AppService: Unable to Initialize QuitEvent\n");
        }
    }
    catch( std::string const& e ) {
        DebugPrintf( SV_LOG_FATAL,"FailoverCommandUtil: exception %s\n", e.c_str() );
    }
    catch( char const* e ) {
        DebugPrintf( SV_LOG_FATAL,"FailoverCommandUtil: exception %s\n", e );
    }
    catch( exception const& e ) {
        DebugPrintf( SV_LOG_FATAL,"FailoverCommandUtil: exception %s\n", e.what() );
    }
    catch(...) {
        DebugPrintf( SV_LOG_FATAL,"FailoverCommandUtil: exception...\n" );
    }
}
int main(int argc, char* argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    ACE::init() ;
	ACE_Thread_Manager tm;
	ControllerPtr controler = Controller::createInstance(&tm);
	if(controler.get() != NULL)
	{
		std::set<std::string> plcHolder;
		controler->init(plcHolder);
	}
	else
	{
		std::cout<< "FailoverCommandUtil failed to initialize controler object." << std::endl;
		return 1;
	}
    MaskRequiredSignals();
    SetupLocalLogging();
    InitCDPQuitEvent();
    FailoverCommandUtil failoverCmdObj;
    failoverCmdObj.GetApplicationSettings();
    failoverCmdObj.GetDisabledPolices();
    if(failoverCmdObj.GetNoOfPolicies() > 0)
    {
        failoverCmdObj.ShowDisabledPolices();
        std::cout<<"\nChoose policyId to do failover:";
        SV_ULONG policyId;
        std::cin>> policyId;
        if(failoverCmdObj.ValidatePolyId(policyId) == true)
        {
            if(failoverCmdObj.EnablePolicy(policyId) == SVS_OK)
            {
                failoverCmdObj.monitorFailoverExecution(policyId);
				failoverCmdObj.StartSVAgent();
            }
        }
        else
        {
            std::cout<<"policyId "<<policyId<<" is either not in the above list  or its dependent policy not completed "<<std::endl;
        }
    }
    else
    {
        std::cout<<"No Failover Policy in disable mode\n";
    }
    return 0;
}

