
#include <tchar.h>
#include <iostream>
using namespace std;

#include "SnmpSendTrap.h"
#include "scopeguard.h"

SNMPAPI_STATUS CALLBACK SnmpTrapAgentCallBack(HSNMP_SESSION snmpSession, HWND snmpAppHandle, UINT notifyMsg,
								WPARAM notifyType, LPARAM requestId, LPVOID clientData)
{
	return 0;
}

SnmpSendTraps::SnmpSendTraps()
{
	InitSnmpTrapSettings();
}
SnmpSendTraps::~SnmpSendTraps()
{
}
void SnmpSendTraps::InitSnmpTrapSettings()
{
	m_trapSettings.snmp_AgentIp = "0.0.0.0";
	m_trapSettings.snmp_ManagerIp = "0.0.0.0";
	m_trapSettings.snmp_CommunityStr = "public";
	m_trapSettings.snmp_Version = "2c";
	m_trapSettings.snmp_ManagerPort = 162;
	m_trapSettings.snmp_TransmitMode = SNMPAPI_UNTRANSLATED_V2;
	m_trapSettings.snmp_RetransmitMode = SNMPAPI_OFF;
	m_trapSettings.snmp_PduType = SNMP_PDU_TRAP;
	m_trapSettings.snmp_SysUpTimeOid = "1.3.6.1.2.1.1.3.0";
}
void SnmpSendTraps::BindSnmpTrapVariables(HSNMP_VBL snmpVbList)
{
	std::vector<SnmpTrapVar>::const_iterator iterEnd = m_trapSettings.snmp_trapVarVector.end();
	for (std::vector<SnmpTrapVar>::const_iterator varIter = m_trapSettings.snmp_trapVarVector.begin(); varIter != iterEnd; ++varIter) 
	{
		if ('s' == varIter->snmp_trapVarType[0])
		{
			BindSnmpStringVar(varIter->snmp_trapVarOid, varIter->snmp_trapVarData, snmpVbList);
		}		
		else
		{
			std::cout<<"\nTrap variable data type is empty. Skipping : "<<varIter->snmp_trapVarData.c_str()<<'\n';
		}
	}	
}
void SnmpSendTraps::BindSnmpTrapTypeOid(HSNMP_VBL snmpVbList)
{
	std::vector<SnmpTrapVar>::const_iterator iterEnd = m_trapSettings.snmp_trapTypeVector.end();
	for (std::vector<SnmpTrapVar>::const_iterator varIter = m_trapSettings.snmp_trapTypeVector.begin(); varIter != iterEnd; ++varIter) 
	{
		if ('o' == varIter->snmp_trapVarType[0])
		{
			BindSnmpOid(varIter->snmp_trapVarOid, varIter->snmp_trapVarData, snmpVbList);
		}		
		else
		{
			std::cout<<"\nTrap variable data type is empty. Skipping : "<<varIter->snmp_trapVarData.c_str()<<'\n';
		}
	}	
}
void SnmpSendTraps::BindSnmpStringVar(const string &strOid, const string &strOctect, HSNMP_VBL snmpVbList)
{
	smiOID smi_strOid = {0};
	SNMPAPI_STATUS snmpApiRetStatus = SnmpStrToOid(strOid.c_str(), &smi_strOid);
	if(SNMPAPI_FAILURE == snmpApiRetStatus)
	{
		std::cout<<"\nFailed to convert string to SNMP OID = "<<strOctect.c_str()<<" : "<<strOid.c_str()<<'\n';
		return;
	}
	
	smiVALUE smi_strVal = {0};
	smi_strVal.syntax = SNMP_SYNTAX_OCTETS;
	smi_strVal.value.string.len = _tcsclen(strOctect.c_str());
	smi_strVal.value.string.ptr = (smiLPBYTE) strOctect.c_str();	
	snmpApiRetStatus = SnmpSetVb(snmpVbList, 0, &smi_strOid, &smi_strVal);
	if(SNMPAPI_FAILURE == snmpApiRetStatus)
		cout<<"\nFailed to bind SNMP variable data = "<<strOctect.c_str()<<" : "<<strOid.c_str()<<'\n';
	
	SnmpFreeDescriptor(smi_strVal.syntax, (smiLPOPAQUE) &smi_strVal.value.string.ptr);
	SnmpFreeDescriptor(SNMP_SYNTAX_OID, (smiLPOPAQUE) &smi_strOid);
}
void SnmpSendTraps::BindSnmpOid(const string &strOid, const string &strOidData, HSNMP_VBL snmpVbList)
{
	smiOID smi_strOid = {0};
	SNMPAPI_STATUS snmpApiRetStatus = SnmpStrToOid(strOid.c_str(), &smi_strOid);
	if(SNMPAPI_FAILURE == snmpApiRetStatus)
	{
		std::cout<<"\nFailed to convert string to SNMP OID = "<<strOid.c_str()<<" : "<<strOid.c_str()<<'\n';
		return;
	}

	smiOID smi_dataOid= {0};
	snmpApiRetStatus = SnmpStrToOid(strOidData.c_str(), &smi_dataOid);
	if(SNMPAPI_FAILURE == snmpApiRetStatus)
	{
		std::cout<<"\nFailed to convert string to SNMP OID = "<<strOidData.c_str()<<" : "<<strOid.c_str()<<'\n';
		return;
	}

	smiVALUE smi_oidValue;
	smi_oidValue.syntax = SNMP_SYNTAX_OID;
	smi_oidValue.value.oid.len = smi_dataOid.len;
	smi_oidValue.value.oid.ptr = smi_dataOid.ptr;
	
	snmpApiRetStatus = SnmpSetVb(snmpVbList, 0, &smi_strOid, &smi_oidValue);
	if(SNMPAPI_FAILURE == snmpApiRetStatus)
		cout<<"\nFailed to bind SNMP variable data = "<<strOidData.c_str()<<" : "<<strOid.c_str()<<'\n';

	SnmpFreeDescriptor(smi_oidValue.syntax, (smiLPOPAQUE) &smi_oidValue.value.string.ptr);
	SnmpFreeDescriptor(SNMP_SYNTAX_OID, (smiLPOPAQUE) &smi_dataOid);
	SnmpFreeDescriptor(SNMP_SYNTAX_OID, (smiLPOPAQUE) &smi_strOid);
}
void SnmpSendTraps::BindSnmpTimeTicks(const string &strOid, const smiTIMETICKS &strTimeTickData, HSNMP_VBL snmpVbList)
{
	smiOID smi_strOid = {0};
	SNMPAPI_STATUS snmpApiRetStatus = SnmpStrToOid(strOid.c_str(), &smi_strOid);
	if(SNMPAPI_FAILURE == snmpApiRetStatus)
	{
		std::cout<<"\nFailed to convert string to SNMP OID = "<<strTimeTickData<<" : "<<strOid.c_str()<<'\n';
		return;
	}

	smiVALUE smi_oidValue;
	smi_oidValue.syntax = SNMP_SYNTAX_TIMETICKS;
	smi_oidValue.value.uNumber = strTimeTickData;

	snmpApiRetStatus = SnmpSetVb(snmpVbList, 0, &smi_strOid, &smi_oidValue);
	if(SNMPAPI_FAILURE == snmpApiRetStatus)
		cout<<"\nFailed to bind SNMP variable data = "<<strTimeTickData<<" : "<<strOid.c_str()<<'\n';

	SnmpFreeDescriptor(SNMP_SYNTAX_OID, (smiLPOPAQUE) &smi_strOid);
}
bool SnmpSendTraps::SendSnmpAlerts()
{
    smiUINT32 snmp_apiMajorVersion;
    smiUINT32 snmp_apiMinorVersion;
    smiUINT32 snmp_communicationLevel;
    smiUINT32 snmp_defaultTransmitMode;
    smiUINT32 snmp_defaultRetransmitMode;

    SNMPAPI_STATUS snmpApiRetStatus = SNMPAPI_SUCCESS;

	//Start snmp
	snmpApiRetStatus = SnmpStartupEx(&snmp_apiMajorVersion, &snmp_apiMinorVersion, &snmp_communicationLevel,
										&snmp_defaultTransmitMode, &snmp_defaultRetransmitMode);
	if(snmpApiRetStatus == SNMPAPI_FAILURE)
	{
		cout<<"\nFailed to initiate SNMP.\n";
		return false;
	}

	//Create snmp session
    HSNMP_SESSION snmp_sessionHdl = SnmpCreateSession(NULL, 0, SnmpTrapAgentCallBack, NULL);
    if (snmp_sessionHdl == SNMPAPI_FAILURE)
    {
        cout<<"\nFailed to create SNMP session.\n";
        return false;
    }
    ON_BLOCK_EXIT(&SnmpCleanupEx);

	//Set transmit mode
    snmpApiRetStatus = SnmpSetTranslateMode(m_trapSettings.snmp_TransmitMode);
    if(snmpApiRetStatus == SNMPAPI_FAILURE)
    {
        cout<<"\nFailed to set SNMP trap transmit mode. ErrorCode = "<<SnmpGetLastError(snmp_sessionHdl)<<'\n';
        return false;
    }
    ON_BLOCK_EXIT(&SnmpClose, snmp_sessionHdl);

	//Get snmp agent ip entity
    HSNMP_ENTITY snmpAgentEntity = SnmpStrToEntity(snmp_sessionHdl, m_trapSettings.snmp_AgentIp.c_str());
    if(snmpAgentEntity == SNMPAPI_FAILURE)
    {
        cout<<"\nFailed to get SNMP agent entity handle. ErrorCode = "<<SnmpGetLastError(snmp_sessionHdl)<<'\n';
        return false;
    }
    ON_BLOCK_EXIT(&SnmpFreeEntity, snmpAgentEntity);

    //snmp manager ip default is "0.0.0.0". means snmp will take local host ip.
    HSNMP_ENTITY snmpManagerEntity = SnmpStrToEntity(snmp_sessionHdl, m_trapSettings.snmp_ManagerIp.c_str());
    if(snmpManagerEntity == SNMPAPI_FAILURE)
    {
        cout<<"\nFailed to get SNMP manager entity handle. ErrorCode = "<<SnmpGetLastError(snmp_sessionHdl)<<'\n';
        return false;
    }
    ON_BLOCK_EXIT(&SnmpFreeEntity, snmpManagerEntity);

    // SNMPCommunityctxt set to default as "public"
    smiOCTETS communityCtxtString;
    communityCtxtString.len = strlen(m_trapSettings.snmp_CommunityStr.c_str());
    communityCtxtString.ptr = (smiLPBYTE) m_trapSettings.snmp_CommunityStr.c_str();
    HSNMP_CONTEXT snmpCommunityCtxt = SnmpStrToContext(snmp_sessionHdl, &communityCtxtString);
    if(snmpCommunityCtxt == SNMPAPI_FAILURE)
    {
        cout<<"\nFailed to set SNMPCommunityctxt. ErrorCode = "<<SnmpGetLastError(snmp_sessionHdl)<<'\n';
        return false;
    }
    ON_BLOCK_EXIT(&SnmpFreeContext, snmpCommunityCtxt);
 
	//Set snmp manager port number
    snmpApiRetStatus = SnmpSetPort(snmpManagerEntity, m_trapSettings.snmp_ManagerPort);
    if(snmpApiRetStatus == SNMPAPI_FAILURE)
    {
        cout<<"\nFailed to set SNMP manager port. ErrorCode = "<<SnmpGetLastError(snmp_sessionHdl)<<'\n';
        return false;
    }   

	//Create a variable binding list
    HSNMP_VBL snmpTrapVbl = SnmpCreateVbl(snmp_sessionHdl, NULL, NULL);
    if(snmpTrapVbl == SNMPAPI_FAILURE)
    {
        cout<<"\nFailed to create variable binding list. ErrorCode = "<<SnmpGetLastError(snmp_sessionHdl)<<'\n';
        return false;
    }
	ON_BLOCK_EXIT(&SnmpFreeVbl, snmpTrapVbl);

	//Bind each trap variables
	//Positions 1 and 2 in VarBindList are sysUpTime and snmpTrapOID respectively.
	smiTIMETICKS snmpSysUpTime = GetTickCount()/10;
	BindSnmpTimeTicks(m_trapSettings.snmp_SysUpTimeOid, snmpSysUpTime, snmpTrapVbl);
	BindSnmpTrapTypeOid(snmpTrapVbl);
    BindSnmpTrapVariables(snmpTrapVbl);
	

	//Create snmp pdu
    HSNMP_PDU trapSnmpPdu = SnmpCreatePdu(snmp_sessionHdl, m_trapSettings.snmp_PduType, NULL, NULL, NULL, snmpTrapVbl);
    if(trapSnmpPdu == SNMPAPI_FAILURE)
    {
        cout<<"\nFailed to create SNMP data unit. ErrorCode = "<<SnmpGetLastError(snmp_sessionHdl)<<'\n';
        return false;
    }
    ON_BLOCK_EXIT(&SnmpFreePdu, trapSnmpPdu);

	//Update the PDU 
    smiINT32 snmpRequestId = GetTickCount()/10;
    snmpApiRetStatus = SnmpSetPduData(trapSnmpPdu, &m_trapSettings.snmp_PduType, &snmpRequestId, NULL, NULL, &snmpTrapVbl);
    if(snmpApiRetStatus == SNMPAPI_FAILURE)
    {
        cout<<"\nFailed to set SNMP data unit. ErrorCode = "<<SnmpGetLastError(snmp_sessionHdl)<<'\n';
        return false;
    }

	//Send SNMP trap
    snmpApiRetStatus = SnmpSendMsg(snmp_sessionHdl, snmpAgentEntity, snmpManagerEntity, snmpCommunityCtxt, trapSnmpPdu);
    if(snmpApiRetStatus == SNMPAPI_FAILURE)
    {
		cout<<"\nFailed to send trap. ErrorCode = "<<SnmpGetLastError(snmp_sessionHdl)<<'\n';
        return false;
    }
	
    return true;
}