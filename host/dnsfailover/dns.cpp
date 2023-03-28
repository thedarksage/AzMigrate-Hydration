#include <windows.h>
#include <string>
#include <iostream>
#include <Windns.h>
#include "ADInterface.h"
#include "defs.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

#define OPTION_DNSFAILOVER				"-failover"
#define OPTION_DNSFAILBACK				"-failback"
#define OPTION_SOURCE_HOST				"-s"
#define OPTION_TARGET_HOST				"-t"
#define OPTION_HOST						"-host"
#define OPTION_IP						"-ip"
#define OPTION_DNSSERVERIP				"-dnsserverip"
#define OPTION_DNSDOMAIN				"-dnsdomain"
#define OPTION_USERNAME					"-user"
#define OPTION_PASSWORD					"-password" 
#define OPTION_FABRIC					"-fabric"

using namespace std;
void usage();
int queryCXforVirtualServerName(const string& srcHost, const string& application, string &virServerName);

int main (int argc, char *argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
	CopyrightNotice displayCopyRightNotice;

	bool bFailover = false;
	bool bFailback = false;
	bool bFabric = false;
	bool bCnameDnsFailover = true;
	string srcHost = "";
	string tgtHost = "";
	string ip = "";
	string dnsServIp = "";
	string dnsDomain = "";
	string user = "";
	string password = "";
    int iParsedOptions = 1;
	ADInterface ad;
	int retValue=0;

	if( argc == 1 )
	{
		usage();
		return -1;
	}

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
			if(strcmpi(argv[iParsedOptions],OPTION_DNSFAILOVER) == 0)
			{
			    bFailover = true;
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_DNSFAILBACK) == 0)
			{
			    bFailback = true;
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_SOURCE_HOST) == 0)
			{
			    iParsedOptions++;
			    if(argv[iParsedOptions][0] == '-')
			    {
			        cout << "Missing parameter for Source Host\n";
					usage();
			        return -1;
			    }
			    srcHost = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_TARGET_HOST) == 0)
			{
			    iParsedOptions++;
			    if(argv[iParsedOptions][0] == '-')
			    {
			        cout << "Missing parameter for Target Host\n";
					usage();
			        return -1;
			    }
			    tgtHost = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_HOST) == 0)
			{
			    iParsedOptions++;
			    if(argv[iParsedOptions][0] == '-')
			    {
			        cout << "Missing parameter for Host\n";
					usage();
			        return -1;
			    }
			    srcHost = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_IP) == 0)
			{
			    iParsedOptions++;
			    if(argv[iParsedOptions][0] == '-')
			    {
			        cout << "Missing parameter for IP\n";
					usage();
			        return -1;
			    }
			    ip = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_DNSSERVERIP) == 0)
			{
				iParsedOptions++;
			    if(argv[iParsedOptions][0] == '-')
			    {
			        cout << "Missing parameter for IP\n";
					usage();
			        return -1;
			    }
			    dnsServIp = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_DNSDOMAIN) == 0)
			{
				iParsedOptions++;
			    if(argv[iParsedOptions][0] == '-')
			    {
			        cout << "Missing parameter for -dnsDomain \n";
					usage();
			        return -1;
			    }
			    dnsDomain = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_USERNAME) == 0)
			{
				iParsedOptions++;
			    if(argv[iParsedOptions][0] == '-')
			    {
			        cout << "Missing parameter for -user \n";
					usage();
			        return -1;
			    }
			    user = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_PASSWORD) == 0)
			{
				iParsedOptions++;
			    if(argv[iParsedOptions][0] == '-')
			    {
			        cout << "Missing parameter for -password\n";
					usage();
			        return -1;
			    }
			    password = argv[iParsedOptions];
			}
			else if(strcmpi(argv[iParsedOptions],OPTION_USE_HOST_DNS_RECORD) == 0)
			{
				bCnameDnsFailover = false;
			}
			else if(strcmpi(argv[iParsedOptions], OPTION_FABRIC) == 0)
			{				
			    bFabric = true;
			}			
			else
			{
				cout << "Detected Invalid option : " << argv[iParsedOptions] << std::endl;
				usage();
			    return -1;
			}
		}
    }
    
	// DNS Failover parameter validation
	if (bFailover && bFailback) {
		cout << "Invalid option: Please request either DNS Failover or DNS Failback\n";
		usage();
		return -1;
	}

	if (bFailover) {
		if (srcHost.empty() || tgtHost.empty()) {
			cout << "Please provide both source and destination Exchange Server Names\n";
			usage();
			return -1;
		}
	}

	if (bFailback) {
		if (srcHost.empty() || ip.empty()) {
			cout << "Please provide both the Exchange Server Name and the IP Address\n";
			usage();
			return -1;
		}
	}

	if (!dnsServIp.empty())	{
		if (dnsDomain.empty() || user.empty() || password.empty() )	{		
			if (dnsDomain.empty())	{
				if (!(user.empty() || password.empty()))
                    cout << "Please provide the domain name of the dns server.\n";
				else
					cout << "Please provide the domain name of the dns server, user name & password \nfor an account on dns server with privileges to update the dns records\n";
			}
			else
					cout << "Please provide user name & password for an account on dns server \nwith privileges to update the dns records\n";		
			usage();
			return -1;
		}
	}                
	
	if(!bFabric)
	{
        // determine the virtual server name of the source if it's an Exchange or a SQL cluster     
		string srcVirServerName = "";
		cout << "\nConnecting to CX to determine if host : " << srcHost.c_str() << " is part of a cluster...\n";
		try
		{
            ad.queryCXforVirtualServerName(srcHost, EXCHANGE, srcVirServerName);
			if (srcVirServerName.empty()) {
				ad.queryCXforVirtualServerName(srcHost, MSSQL, srcVirServerName);
			}
		}
		catch(...)
		{
			cout << "[WARNING] Failed to connect to CX. Possible reasons are," << endl 
						<< "\t1) VX is not installed" << endl
						<< "\t2) CX is not online" << endl;
		}

		if (srcVirServerName.empty() == false) {
			cout << "Source Host : " << srcHost.c_str() << " is part of cluster " << srcVirServerName.c_str() << std::endl;
			srcHost = srcVirServerName;
		}
		// determine the virtual server name of the target if it's an Exchange or a SQL cluster
		if (tgtHost.empty() == false) {
			string tgtVirServerName = "";
			cout << "\nConnecting to CX to determine if host : " << tgtHost.c_str() << " is part of a cluster...\n";
			try
			{
				ad.queryCXforVirtualServerName(tgtHost, EXCHANGE, tgtVirServerName);			
				if (tgtVirServerName.empty()) {
					ad.queryCXforVirtualServerName(tgtHost, MSSQL, tgtVirServerName);
				}
			}
			catch(...)
			{
				cout << "[WARNING] Failed to connect to CX. Possible reasons are," << endl 
						<< "\t1) VX is not installed" << endl
						<< "\t2) CX is not online" << endl;
			}

			if (tgtVirServerName.empty() == false) {
				cout << "Target Host : " << tgtHost.c_str() << " is part of cluster " << tgtVirServerName.c_str() << std::endl;
				tgtHost = tgtVirServerName;
			}
		}
	}

	if (bFailover) {
		cout << "\nUsing DNS names : " << srcHost.c_str() << " and  " << tgtHost.c_str() << " For DNS failover\n";
	}
	else {
		cout << "Using DNS name : " << srcHost.c_str() << " For DNS failback\n";
		if(ad.getDNSRecordType(srcHost,bCnameDnsFailover)!=0)
		{
			cout<<"Failed to do DNS failback."<<endl;
			return -1;
		}
	}

	if(bCnameDnsFailover)
	{
		if(bFailback)
		{
			tgtHost=srcHost;
			srcHost="";
		}
		if(ad.dnsFailoverUsingCname(srcHost, tgtHost, ip, dnsServIp, dnsDomain, user, password) != 0) 
		{
			if (bFailover) 
			{
				cout << "Failed to perform DNS Failover\n";
			}
			else 
			{
				cout << "Failed to perform DNS Failback\n";
			}
			retValue=-1;
		}
		else
		{
			  if (bFailover) 
					cout << "\n***** DNS record for " << srcHost <<" now modified to point to "<< tgtHost << " successfully\n";
				else
					cout << "\n***** DNS record for " << tgtHost <<" now modified to point to "<< tgtHost << " successfully\n";

				cout << "Flushing DNS cache...\n" <<endl;
				if(ad.FlushDnsCache())
				{
					cout << "DNS cache flushed successfully.\n" <<endl;
				}
				else
				{
					cout <<"WARNING: Failed to flush DNS cache.\n" <<endl;
				}
		}
		//Flushing the dns cache in above call itself
	}
	else
	{
		if (ad.DNSChangeIP(srcHost, tgtHost, ip, dnsServIp, dnsDomain, user, password, bFailover) != 0) {
			if (bFailover) {
				cout << "Failed to perform DNS Failover\n";
			}
			else {
				cout << "Failed to perform DNS Failback\n";
			}
			retValue=-1;
		}
		else
		{		
			if(ad.FlushDnsCache())
			{
				cout << "DNS cache flushed successfully.\n" <<endl;
			}
			else
			{
				cout <<"WARNING: Failed to flush DNS cache.\n" <<endl;
			}
		}
	}

    return retValue;
}

void usage()
{
	cout << "\n**************** DNS Failover Failback Usage ********************\n\n";
    cout << "dns -failover -s SourceServerName -t DestinationServerName\n";
	cout << "    [ -dnsserverip DNSServerIP -dnsdomain DomainOfDNSServer\n";
	cout << "      -user UserName -password Password\n";
	cout << "      -fabric -useHostDnsRecord]\n\n";
	cout << "dns -failback -host PrimaryServerName -ip PrimaryServerIP\n";
	cout << "    [ -dnsserverip DNSServerIP -dnsdomain DomainOfDNSServer\n";
	cout << "      -user UserName -password Password\n";
	cout << "      -fabric -useHostDnsRecord]\n\n";
  cout << "Please note that Host based DNS Failback can be done only if DNS Failover is done using -useHostDnsRecord option\n\n";
	cout << "-s\t\tSourceServerName		 \n\t\tSpecify Fully Qualified Domain Name of the Source server\n\n";
	cout << "-t\t\tDestinationServerName \n\t\tSpecify Fully Qualified Domain Name of the Destination server\n\n";
	cout << "-ip\t\tSourceServerIP		 \n\t\tIP address of the source server\n\n";
	cout << "-host\t\tHost			 \n\t\tHost for which IP address is to be assigned\n\n";	
	cout << "-dnsserverip\tDNSServerIP  \n\t\tIP of DNS Server where the dns record has to be updated \n\n";
	cout << "-dnsdomain\tDNSDomain    \n\t\tName of the domain to which DNS server belongs to\n\n";
	cout << "-user\t\tUserName     \n\t\tUser with Domain Admin privileges OR"; 
	cout << "\n\t\tUser Authorized to update dns records in DNS server \n\n";
	cout << "-password\tPassword     \n\t\tPassword of UserName to logon to DNS Server \n\n";
	cout << "-fabric\t\tRequired for dns failover in fabric setup OR";
	cout << "\n	\t\tIn setup where VX is not installed OR";
	cout << "\n	\t\tWhen CX is not online\n";
  cout << "\n-useHostDnsRecord\tUse this option when you want to do Host based DNS Failover.\n\n";
	cout << "\n	\tUser should provide appropriate Virtul server name as source/target\n\n";

}