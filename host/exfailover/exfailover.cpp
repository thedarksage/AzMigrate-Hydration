#include "ADInterface.h"
#include "defs.h"
#include "Ex2010Failover.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

using namespace std;

#define OPTION_DRYRUN           "-dryrun"
#define OPTION_FILTER           "-f"
#define OPTION_REFINE_FILTER    "-autorefine"
#define OPTION_CREATE_STORAGE_GROUPS    "-cs"
#define OPTION_EXCLUSIVE_FILTER			"-ef"
#define OPTION_FAILBACK			"-failback"

bool bSldRoot  = false;
void usage();

bool getExchangeVersion(const string srcExchServer, string& version)
{
	bool rv = true;
	ADInterface ad;
	list<string> attrValues;	
	list<string>::const_iterator iter;
	//By default assume Exchange 2003/2000
	if(version.empty())
		version = "Exchange";	
	
	if(	ad.getAttrValueList(srcExchServer, attrValues, "serialNumber", "(objectclass=msExchExchangeServer)", "") == SVS_OK )
	{
        iter = attrValues.begin();
		if(!iter->empty())
		{
			if (ad.findPosIgnoreCase((*iter), "Version 8") == 0)
				version = "Exchange2007";
			else if (0 == ad.findPosIgnoreCase((*iter), "Version 14."))
				version = EXCHANGE2010;
		}
	}
	return rv;
}

int main(int argc, _TCHAR* argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
	CopyrightNotice displayCopyrightNotice;
	
    bool bDryRun            = false;
    bool bFilter            = false;
    bool bAutoRefineFilters = false;
	bool bCreateSGs			= false;
	bool bExclusiveUserDefinedFilter = false;
	bool bRetainDuplicateSPN = false;
	bool bFailover = false;
	bool bFailback = false;
	string op;
    int iParsedOptions      = 1;
    string szFilter = "";
	string pfSrcExchServerName = "";
	string pfTgtExchServerName = "";
	string destinationExchangeServerName = "";
	string sourceExchangeServerName = "";
	string mtaExchangeServerName = "";
	string sourceCAS,targetCAS;
	string sourceHT,targetHT;
    ULONG num = 0;

    if(argc < 3)
    {
		usage();
        return 0;
    }

	ADInterface ad;
	//ad.setTargetClusterFlag();

	for (;iParsedOptions < argc; iParsedOptions++)
	{
		if(argv[iParsedOptions][0] == '-') 
		{
			if((strcmpi(argv[iParsedOptions],OPTION_HELP1) == 0) ||
			   (strcmpi(argv[iParsedOptions],OPTION_HELP2) == 0))
            {
                usage();
				return 0;
            }
			if(strcmpi(argv[iParsedOptions],OPTION_DRYRUN) == 0)
			{
			    bDryRun = true;
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_FILTER) == 0)
			{
			    iParsedOptions++;
			    
				if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
			        cout << "Missing parameter for filter option\n";
					usage();
			        return 0;
			    }

			    ad.szFltUserDefinedFilter = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_REFINE_FILTER) == 0)
			{
			    bAutoRefineFilters = true;
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_EXCLUSIVE_FILTER) == 0)
			{
			    iParsedOptions++;
			    
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
			        cout << "Missing parameter for filter option\n";
					usage();
			        return 0;
			    }

			    ad.szFltUserDefinedFilter = argv[iParsedOptions];
			    bExclusiveUserDefinedFilter = true;
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_CREATE_STORAGE_GROUPS) == 0)
			{
				bCreateSGs = true;
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_RETAIN_DUPLICATE_SPN) == 0)
			{
				bRetainDuplicateSPN = true;
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_FAILOVER) == 0)
			{
				bFailover = true;
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_SLD_ROOT_DOMAIN) == 0)
			{
				bSldRoot = true;
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_FAILBACK) == 0)
			{
				bFailback = true;
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_SOURCE_HOST) == 0)
			{
			    iParsedOptions++;
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
					cout << "\nMissing parameter value for source public folder host\n\n";
					usage();
					return 0;
			    }
				sourceExchangeServerName = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_TARGET_HOST) == 0)
			{
			    iParsedOptions++;
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
			        cout << "\nMissing parameter value for target public folder host\n\n";
					usage();
					return 0;
			    }
				destinationExchangeServerName = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_PF_SOURCE_HOST) == 0)
			{
			    iParsedOptions++;
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
					cout << "\nMissing parameter value for source host\n\n";
					usage();
					return 0;
			    }
				pfSrcExchServerName = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_PF_TARGET_HOST) == 0)
			{
			    iParsedOptions++;
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
			        cout << "\nMissing parameter value for target host\n\n";
					usage();
					return 0;
			    }
				pfTgtExchServerName = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_MTASERVER) == 0)
			{
			    iParsedOptions++;
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
			        cout << "\nMissing parameter value for -mta\n\n";
					usage();
					return 0;
			    }
				mtaExchangeServerName = argv[iParsedOptions];
				cout << "[INFO]: -mta option is obsolete. Please avoid using this option from Scout 5.1 version or later\n";
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_SOURCE_CAS) == 0)
			{
			    iParsedOptions++;
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
			        cout << "\nMissing parameter value for -sourceCAS.\n\n";
					usage();
					return 0;
			    }
				sourceCAS = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_TARGET_CAS) == 0)
			{
			    iParsedOptions++;
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
			        cout << "\nMissing parameter value for -targetCAS.\n\n";
					usage();
					return 0;
			    }
				targetCAS = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_SOURCE_HT) == 0)
			{
			    iParsedOptions++;
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
			        cout << "\nMissing parameter value for -sourceHT.\n\n";
					usage();
					return 0;
			    }
				sourceHT = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_TARGET_HT) == 0)
			{
			    iParsedOptions++;
			    if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			    {
			        cout << "\nMissing parameter value for -targetHT.\n\n";
					usage();
					return 0;
			    }
				targetHT = argv[iParsedOptions];
			}
			else
			{
			    cout << "Detected Invalid option : " << argv[iParsedOptions] << std::endl;
				usage();
			    return 0;
			}
		}
	}

	if (sourceExchangeServerName.empty()) {
		cout << "\nPlease specify source host name\n";
		usage();
		return 0;
	}
	if (destinationExchangeServerName.empty()) {
		cout << "\nPlease specify target host name\n";
		usage();
		return 0;
	}
	if ( !(bFailover || bFailback) )	{
		cout << "\nPlease provide the operation: -failover or -failback\n";
		usage();
		return 0;
	}
	if ( bFailover && bFailback )	{
		cout << "\nPlease provide only one of the operations: -failover or failback\n";
		usage();
		return 0;
	}
	//Make sure that the -sourceCAS, -targetCAS options are specified properly. i.e both should be specified or nothing should be specified.
	if (!sourceCAS.empty() || !targetCAS.empty())
		if (sourceCAS.empty() || targetCAS.empty())
		{ 
			cout << "\nPlease specify both the source and the target Exchange servers hosting Client Access Role\n";
			usage();
			return 0;
		}
	//Make sure that the -sourceHT, -targetHT options are specified properly. i.e both should be specified or nothing should be specified.
	if (!sourceHT.empty() || !targetHT.empty())
		if (sourceHT.empty() || targetHT.empty())
		{ 
			cout << "\nPlease specify both the source and the target Exchange servers hosting Hub Transport Role\n";
			usage();
			return 0;
		}
	string version;
	//Exchange 2010 support for exFailover.exe
	getExchangeVersion(sourceExchangeServerName,version);
	if(version.compare( EXCHANGE2010 ) == 0)
	{
		//Verify that the target server specified is valied exchnage server or not
		getExchangeVersion(destinationExchangeServerName,version);
		if(version.compare(EXCHANGE2010) == 0)
		{
			Ex2010Failover m_Ex2010Failover;
			if(m_Ex2010Failover.SetCASServer(sourceCAS,targetCAS) == SVS_OK)
			{
				if(m_Ex2010Failover.SetHTServer(sourceHT,targetHT) == SVS_OK)
				{
					if(m_Ex2010Failover.FastFailover(sourceExchangeServerName,		//Source Exchange Server Name
						destinationExchangeServerName,								//Target Exchange server Name
						szFilter,													//User migration filet specified by the user
						EXCHANGE2010,
						(bFilter||bExclusiveUserDefinedFilter||bAutoRefineFilters), //is the user specified any filter option?
						bFailover,													//
						bFailback,													//
						bRetainDuplicateSPN,										//if this is true, then the SPN entries will not deleted. Duplicates SPN entrie will be there
						bDryRun) == SVS_OK)											//
					{
						cout<<"****** ";
						if(bFailback)
							cout<<"Failback ";
						else
							cout<<"Failover ";
						cout<<"From "<<sourceExchangeServerName
							<<" To " <<destinationExchangeServerName
							<<" was successful ******"<<endl;
					}
					else {
						cout<<"Can not continue ";
						if(bFailback)
							cout<<"Failback ";
						else
							cout<<"Failover ";
					}
				}
				else
				{
					cout<<"Can not continue ";
					if(bFailback)
						cout<<"Failback ";
					else
						cout<<"Failover ";
				}
			}
			else
			{
				cout<<"Can not continue ";
				if(bFailback)
					cout<<"Failback ";
				else
					cout<<"Failover ";
			}
		}
		else
		{
			cout<<"[WARNING]: Target server name specified is not a valied Exchange 2010 Server"<<endl
				<<"Please provide the valied source and target Exchange server names."<<endl;
		}

		return 0;
	}

	string srcMTA, tgtMTA;	
	if( ad.getResponsibleMTAServer(sourceExchangeServerName, srcMTA) == SVE_FAIL )
	{
		cout << "Error while detecting responsible MTA server responsible for source host: " << sourceExchangeServerName << endl;
		return SVE_FAIL;
	}
	else
	{
		cout << "The MTA server responsible for " << sourceExchangeServerName << " is " << srcMTA << endl;
	}
	if( ad.getResponsibleMTAServer(destinationExchangeServerName, tgtMTA) == SVE_FAIL )
	{
		cout << "Error while detecting responsible MTA server responsible for target host: " << destinationExchangeServerName << endl;
		return SVE_FAIL;
	}
	else
	{
			cout << "The MTA server responsible for " << destinationExchangeServerName << " is " << tgtMTA << endl;
	}

	//Validate the MTA associations w.r.t. source & target servers
	if( (strcmpi(sourceExchangeServerName.c_str(), srcMTA.c_str()) == 0) ^ (strcmpi(destinationExchangeServerName.c_str(), tgtMTA.c_str()) == 0))
	{
		cout << "[WARNING]: Either source virtual server or target virtual server is not associated with MTA resource...\n";
	}

	ad.srcMTAExchServerName = srcMTA;
	ad.tgtMTAExchServerName = tgtMTA;
	// Set the operation type so that modifyServicePrincipalName() can process accordingly
	if ( bFailback )	
		op = "Failback";
	if( bRetainDuplicateSPN )	
		cout << "\nRetaining the duplicate SPN entries related to Exchange...\n";	

    if(!bExclusiveUserDefinedFilter)
        ad.szDefaultFilter = "(objectCategory=person)(objectClass=user)" "(" + msExchHomeServerName + "=*cn=" + sourceExchangeServerName + ")";

	// search, compare and create missing storage groups on target
	list<string> proVolumes;
	list<string> proSGs;
	string sgDN;
	bool deleteExchConfigOnTarget = false;
	if (!bDryRun && bCreateSGs) {//XOR
		if (pfSrcExchServerName.empty() && pfTgtExchServerName.empty() == false) { 
			cout << "\nPlease specify both the source and the target hosts hosting the public folders\n";
			usage();
			return 0;
		}
		else if (pfTgtExchServerName.empty() && pfSrcExchServerName.empty() == false) { 
			cout << "\nPlease specify both the source and the target hosts hosting the public folders\n";
			usage();
			return 0;
		}

		map<string, string> placeHolder;
		deleteExchConfigOnTarget = true;
		
		if (ad.processExchangeTree(sourceExchangeServerName, destinationExchangeServerName, proVolumes, placeHolder, sgDN, proSGs, deleteExchConfigOnTarget,
								   pfSrcExchServerName, pfTgtExchServerName) == SVE_FAIL) {
			cout << "Failed to create Information Store Failover cannot continue\n";
			return 0;
		}
		else {
			cout << "Successfully processed Exchange Configuration in AD\n";
		}		
	}

	if (bFilter || bExclusiveUserDefinedFilter || bAutoRefineFilters) {
		if (ad.failoverExchangeServerUsers(sourceExchangeServerName, destinationExchangeServerName, bAutoRefineFilters, bDryRun,
										   proSGs, bFailover,bSldRoot) != SVS_OK)
		{
			cout<<"\nFailed to migrate users\n";
			return 0;
		}

		if( !bDryRun )
		{			
			if(!getExchangeVersion(sourceExchangeServerName, version))
			{
				cout << "\nWARNING: Failed to determine the version of Exchange server. Assuming Exchange 2003 server.\n";
			}
			
			//RUS update is considered for Exchange 2000/2003 server.			
			if(strcmpi(version.c_str(), EXCHANGE) == 0)
			{
                // Change the RUS (Routing Update Service) to point to the target server
				if (ad.rerouteRUS(sourceExchangeServerName, destinationExchangeServerName) == SVE_FAIL)
					cout << "\nWARNING: Failed to update RUS entries in AD. This will cause Mail Delivery issues between outlook clients and Exchange\n";
				else
					cout << "\n******Successfully rerouted RUS from " << sourceExchangeServerName.c_str() << " to " << destinationExchangeServerName.c_str() << std::endl;
			}

			// Remove entries from servicePrincipalName attribute from the source computer corresponding to source exchange server
			// and Add entries to servicePrincipalName attribute to the destination computer corresponding to target exchange server
			if (ad.modifyServicePrincipalName(sourceExchangeServerName, destinationExchangeServerName, bRetainDuplicateSPN, op, version) == SVE_FAIL)
				cout << "WARNING: Failed to update servicePrincipalName AD. This will cause Authentication failures between outlook clients and Exchange\n";

			cout << "\n*******Successfully Updated Computer " << destinationExchangeServerName.c_str() << " with necessary exchange entries" << std::endl;
		}
	}

    return 0;
}

void usage()
{
	cout<<"\nUsage:\n\n"
		<<"exfailover {-failover | -failback} \n"
		<<"            -s <SourceExchangeServerName> \n"
		<<"            -t <TargetExchangeServerName> \n"
		<<"           [-mta <MTA Exchange Virtual Server>] \n"
		<<"           [-dryrun] \n"
		<<"           [-autorefine] \n"
		<<"           [-ef FilterString] \n"
		<<"           [-f FilterString] \n"
		<<"           [-cs] \n"
		<<"           [-retainDuplicateSPN] \n"		
		<<"			  [-sourceCAS] \n"
		<<"			  [-targetCAS] \n"
		<<"			  [-sourceHT] \n"
		<<"			  [-targetHT] \n"
		<<"           [-pfs <PFSourceExchangeServerName>] \n"
		<<"           [-pft <PFTargetExchangeServerName>]\n\n"
		<<"           [-sldRoot]\n";
	
	cout<<"-failover -> failover operation.\n\n";
	cout<<"-failback -> failback operation.\n\n";
	cout<<"<SourceExchangeServerName>	-> Name of source Exchange server\n";
	cout<<"                                   during failover or failback operation.\n\n";
	cout<<"<TargetExchangeServerName>	-> Name of target Exchange server.\n";
	cout<<"                                   during failover or failback operation.\n\n";
	cout << "-mta		<MTA Exchange virtual Server>	=> Exchange virtual Server hosting MTA resource\n";
	cout << "    		                     	   This option is obsolete.\n";
    cout << "    		                     	   Supported only for backward compatibility of failover scripts.\n";
	cout<<"-retainDuplicateSPN		-> Option to retain duplicate SPN entries\n";
	cout<<"                                   related to Exchange in ActiveDirectory.\n\n";
    cout<<"-ef	-> Exclusive Filter. This overides all built in filters.\n\n";
	cout<<"-f	-> This is an additive filter to the built in filters\n\n";
	cout<<"-pfs	-> Exchange server hosting public folders for Source Exchange Server\n\n";
	cout<<"-pft	-> Exchange server hosting target public folders for Destination Exchange Server\n\n";
	cout<<"-sourceCAS, -targetCAS, -sourceHT, -targetHT -> options are used to failover Client Access & Hub Transport Server Roles in case they are installed seperate from Mailbaox Server Role.(Specific to Exchange Server 2010) \n\n";
	cout<<"Example Filter String1: \"msExchHomeServerName=*exchange*\"\n";
	cout<<"     - This will include all entries whose msExchHomeServerName\n";
	cout<<"       value contains \"exchange\" as a substring\n\n";
	cout<<"Example Filter String2: \"cn=Inmage*\"\n";
	cout<<"     - This will include all entries whose cn starts with \"Inmage\"\n\n";
	cout<<"Example Filter String3: \"|(cn=Inmage*)(cn=*Inmage)\"\n";
	cout<<"     - This will include all entries whose cn starts\n";
	cout<<"       with \"Inmage\" or ends with \"Inmage\"\n\n";
	cout<<"Example Filter String4: \"|(&(cn=Inmage*)(cn=*Inmage))(cn=*abhaisystems*)\"\n";
	cout<<"     - This will include all entries whose cn starts with \"Inmage\" and ends\n";
	cout<<"       with Inmage or contains \"abhaisystems\"\n\n" ;
	cout<<"-autorefine	-> This enables automatic refine of filter if result exceeds size limit\n\n";
	cout<<"-cs		-> This creates any missing storage groups on the Destination Exchange Server\n\n";
	cout<<"-sldRoot -> Use this option if single label DNS name is used for root domain and exchange is in child domain\n";
}