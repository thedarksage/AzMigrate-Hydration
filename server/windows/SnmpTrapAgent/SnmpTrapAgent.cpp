// SnmpTrapAgentMain.cpp : Defines the entry point for the console application.
//

#include <tchar.h>
#include <iostream>
using namespace std;

#include "SnmpSendTrap.h"
#include "inmsafecapis.h"

void DisplaySnmpTrapAgentUsage();
bool ParseSnmpTrapVar(std::string trapVar, SnmpTrapVar *tVar);
bool ParseSnmpTrapAgentArgs(int argc, char* argv[], SnmpTrapSettings *trapSettings);

int _tmain(int argc, _TCHAR* argv[])
{
	//calling init funtion of inmsafec library
	init_inm_safe_c_apis();

	int snmpMainRet = 1;
	try {
		SnmpSendTraps  snmpTrapObj;
		if(ParseSnmpTrapAgentArgs(argc, argv, &snmpTrapObj.m_trapSettings))
		{
			if(snmpTrapObj.SendSnmpAlerts())
			{
				snmpMainRet = 0;
			}
		}
	} 
	catch (std::exception const& e) {
		cout << "\nException in Snmp trap agent: " << e.what() << '\n';
	}
	catch (...) {
		cout << "\nUnknown exception in Snmp trap agent.\n";
	}
	return snmpMainRet;
}
void DisplaySnmpTrapAgentUsage()
{
	cout << "\nUsage: SnmpTrapAgent.exe -v <SNMPVersion> -c <CommunityContext> -ip <SNMPManagerIp> -port <TrapReceiverPort> -traptype <oid::datatype::value> -trapvar <oid::datatype::value>\n\n"
		 << "Where, \n\n"
		 << "-v <SNMPVersion>		: Version of SNMP protocol. [Supported version = 2c]\n\n"
		 << "-c <CommunityContext>		: Community context. [Default = public]\n\n"
		 << "-ip <SNMPManagerIp>		: Destination host ip where the trap receiver application launched to receive trap messages.\n\n"
		 << "-port <TrapReceiverPort>	: Destination host port number where the trap receiver application is listening for trap messages.\n\n"
		 << "-traptype <oid::datatype::value>	: Type of trap message as defined in MIB file.\n\n"
		 << "-trapvar <oid::datatype::value>	: List of trap message variables to bind with SNMP Protocol Data Unit.\n\n\n";

	cout << "Example: SnmpTrapAgent.exe -v 2c -c public -ip 127.0.0.1 -port 162 -traptype 1.3.6.1.2.1.1.3.0::o::1.3.6.1.4.1.17282.1.14.0.245  -trapvar 1.3.6.1.4.1.17282.1.14.1::s::A4FEEFA4-5525-064F-A174AA100EE6E57C  -trapvar 1.3.6.1.4.1.17282.1.14.50::s::\"No communication from Process Server on imits152 since more than 900 seconds.\"\n\n\n";
}
bool ParseSnmpTrapAgentArgs(int argc, char* argv[], SnmpTrapSettings *trapSettings)
{
	bool bArgsOk = true;

	if(1 == argc)
	{
		DisplaySnmpTrapAgentUsage();
		return false;
	}

	int snmpArgIndex = 1;
    for (;snmpArgIndex < argc; ++snmpArgIndex)
	{
		if ('-' != argv[snmpArgIndex][0]) 
		{
			cout << "\n*** Invalid option: " << argv[snmpArgIndex] << '\n';
            bArgsOk = false;
			break;
        } 
		else if ('\0' == argv[snmpArgIndex][1]) 
		{
            cout << "\n*** Invalid option: " << argv[snmpArgIndex] << '\n';
            bArgsOk = false;
			break;
        } 
		else if ('-' == argv[snmpArgIndex][0]) 
		{
			//-v
			if (0 == strcmpi("-v", argv[snmpArgIndex])) 
			{
				++snmpArgIndex;
				if (snmpArgIndex >= argc || '-' == argv[snmpArgIndex][0]) 
				{
                    cout << "\n*** missing value for -v\n";
                    bArgsOk = false;
					break;
				} 
				else 
				{
					if(0 == strcmpi("2c", argv[snmpArgIndex]))
					{
						trapSettings->snmp_Version = argv[snmpArgIndex];
						trapSettings->snmp_TransmitMode = SNMPAPI_UNTRANSLATED_V2;
					}
					else
					{
						cout<< "\n*** Unsupported SNMP version. SnmpTrapAgent supports only version 2c\n";
						bArgsOk = false;
						break;
					}					
				}				
			}

			//-c
			if (0 == strcmpi("-c", argv[snmpArgIndex])) 
			{
				++snmpArgIndex ;
				if (snmpArgIndex >= argc || '-' == argv[snmpArgIndex][0]) 
				{
                    cout << "\n*** missing value for -c\n";
                    bArgsOk = false;
					break;
				} 
				else 
				{
					trapSettings->snmp_CommunityStr = argv[snmpArgIndex];
				}
			}

			//-ip
			if (0 == strcmpi("-ip", argv[snmpArgIndex])) 
			{
				++snmpArgIndex;
				if (snmpArgIndex >= argc || '-' == argv[snmpArgIndex][0]) 
				{
                    cout << "\n*** missing value for -ip\n";
                    bArgsOk = false;
					break;
				} 
				else 
				{
					trapSettings->snmp_ManagerIp = argv[snmpArgIndex];
				}
			}

			//-port
			if (0 == strcmpi("-port", argv[snmpArgIndex])) 
			{
				++snmpArgIndex;
				if (snmpArgIndex >= argc || '-' == argv[snmpArgIndex][0]) 
				{
                    cout << "\n*** missing value for -port\n";
                    bArgsOk = false;
					break;
				} 
				else 
				{
					UINT trapRcvrPortNum = strtoul(argv[snmpArgIndex], NULL, 0);
					if((trapRcvrPortNum > 0) && (trapRcvrPortNum < 65536))
						trapSettings->snmp_ManagerPort = trapRcvrPortNum;
					else
					{
						cout<< "\n*** Invalid port number : "<<trapRcvrPortNum<<"\nPort number should range from 1 to 65535\n";
						bArgsOk = false;
						break;
					}
				}
			}

			//-traptype
			if (0 == strcmpi("-traptype", argv[snmpArgIndex])) 
			{
				++snmpArgIndex;
				if (snmpArgIndex >= argc || '-' == argv[snmpArgIndex][0]) 
				{
                    cout << "\n*** missing value for -traptype\n";
                    bArgsOk = false;
					break;
				} 
				else 
				{
					SnmpTrapVar tVar;
					ParseSnmpTrapVar(argv[snmpArgIndex], &tVar);
					trapSettings->snmp_trapTypeVector.push_back(tVar);
				}
			}

			//-trapvar
			while(0 == strcmpi("-trapvar", argv[snmpArgIndex]))
			{
				++snmpArgIndex;
				if (snmpArgIndex >= argc || '-' == argv[snmpArgIndex][0]) 
				{
                    cout << "\n*** missing value for -trapvar\n";
                    bArgsOk = false;
					break;
				} 
				else 
				{
					SnmpTrapVar tVar;
					ParseSnmpTrapVar(argv[snmpArgIndex], &tVar);
					trapSettings->snmp_trapVarVector.push_back(tVar);
				}
			}
		}
	}
	return bArgsOk;
}
bool ParseSnmpTrapVar(std::string trapVarStr, SnmpTrapVar *trapVar)
{
	if(trapVarStr.empty())
	{
		cout<<"\nSNMP trap variable is empty.\n";
		return false;
	}

	int colon_delim_pos = 0;
	int trapVectorIndex = 0;
	std::vector<std::string> trapVarVector (3);
	std::string trapVarDelim = "::";

	for(;colon_delim_pos <= trapVarStr.length();++trapVectorIndex)
	{
		int trapVarPos = trapVarStr.find(trapVarDelim, colon_delim_pos);
        if(trapVarPos == std::string::npos)
            trapVarPos = trapVarStr.size();
		if(trapVectorIndex < 3)
			trapVarVector.at(trapVectorIndex) = trapVarStr.substr( colon_delim_pos, trapVarPos - colon_delim_pos);
        colon_delim_pos = trapVarPos + trapVarDelim.size();
	}

	trapVar->snmp_trapVarOid = trapVarVector.at(0);
	trapVar->snmp_trapVarType = trapVarVector.at(1);
	trapVar->snmp_trapVarData = trapVarVector.at(2);

	return true;
}

