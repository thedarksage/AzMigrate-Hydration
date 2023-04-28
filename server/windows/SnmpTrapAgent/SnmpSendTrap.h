#pragma once

#include <string>
#include <vector>
#include <winsnmp.h>

enum AmethystTrapVariables
{
	trapAmethystGuid = 1,
	trapHostGuid = 5,
	trapVolumeGuid = 10,
	trapAgentLicenseID = 40,
	trapLicenseExpirationDate = 45,
	trapAgentErrorMessage = 50,
	trapAgentLoggedErrorLevel = 55,
	trapAgentLoggedErrorMessageTime = 60,
	trapFilerepConfig = 65,
	trapCSHostname = 70,
	trapPSHostname = 75,
	trapSeverity = 80,
	trapSourceHostName = 85,
	trapSourceDeviceName = 90,
	trapTargetHostName = 95,
	trapTargetDeviceName = 100,
	trapRetentionDeviceName = 105,
	trapAffectedSystemHostName = 110
};

enum AmethystTrapType
{
	trapUnknown = 0,
	trapSentinelResyncRequired = 215,
	trapSecondaryStorageUtilizationDangerLevel = 260,
	trapFxAgentLicenseExpired = 310,
	trapVxAgentLicenseExpired = 315,
	trapCxAgentLicenseExpired = 320,
	trapFxAgentLicenseExpirationWarning = 325,
	trapVxAgentLicenseExpirationWarning = 330,
	trapCxAgentLicenseExpirationWarning = 335,
	trapAgentLoggedErrorMessage = 340,
	trapLicenseFileMissing = 345,
	volumeReSize = 250,
	processServerFailover = 350,
	moveRetentionLog = 255,
	insufficientRetentionSpace = 270,
	capacityThresholdExceeded = 355,
	capacityUtilizationReachedLimit = 360,
	CXNodeFailover = 370,
	BandwidthShaping = 375,
	ProcessServerUnInstallation = 380,
	trapFXJobError = 385,
	ApplicationProtectionAlerts = 390,
	trapFXJobFailedWithError = 395,
	trapCXHAClusterNodeFailover = 400,
	trapFXProtectionJobFailedWithError = 405,
	trapFXRecoveryJobFailedWithError = 410,
	trapFXConsistencyJobFailedWithError = 415,
	trapFXProtectionJobSuccessful = 420,
	trapFXFailoverJobSuccessful = 425,
	trapVXPairInitialDiffSync = 430,
	trapbitmapmodechange = 435,
	trapVersionMismatch = 440,
	trapMacMismatch = 445,
	trapAppAgentDown = 230,
	trapFxAgentDown = 235,
	trapVxAgentDown = 240,
	trapPSNodeDown = 245,
	trapVXRPOSLAThresholdExceeded = 280,
	trapFXRPOSLAThresholdExceeded = 285,
	trapPSRPOSLAThresholdExceeded = 290,
	trapCSUpAndRunning = 450,
	trapDiskSpaceThresholdOnSwitchBPExceeded = 460,
	trapStorageAgentLoggedErrorMessage = 500,
	trapStorageNodeDown = 501
};

enum SnmpTrapVariableType
{
	SNMP_INTEGER_TYPE = 0,		//i
	SNMP_UNSIGNED_TYPE,			//u
	SNMP_COUNTER32_TYPE,		//c
    SNMP_STRING_TYPE,			//s
    SNMP_HEXSTRING_TYPE,		//x
    SNMP_DECIMALSTRING_TYPE,	//d
    SNMP_NULLOBJ_TYPE,			//n
    SNMP_OBJID_TYPE,			//o
    SNMP_TIMETICKS_TYPE,		//t
    SNMP_IPADDRESS_TYPE,		//a
    SNMP_BITS_TYPE				//b
};

struct SnmpTrapVar
{
	std::string snmp_trapVarOid;
	std::string snmp_trapVarType;
	std::string snmp_trapVarData;

	SnmpTrapVar()
		: snmp_trapVarOid(""),
		  snmp_trapVarType(""),
		  snmp_trapVarData("")
	{ }
};

struct SnmpTrapSettings
{
	std::string snmp_AgentIp;
	std::string snmp_ManagerIp;
	std::string snmp_CommunityStr;
	std::string snmp_Version;
	std::string snmp_SysUpTimeOid;
	UINT snmp_ManagerPort;
	smiUINT32 snmp_TransmitMode;
	smiUINT32 snmp_RetransmitMode;
	smiINT snmp_PduType;	
	std::vector<SnmpTrapVar> snmp_trapVarVector;
	std::vector<SnmpTrapVar> snmp_trapTypeVector;
};

class SnmpSendTraps
{
public:
	SnmpSendTraps(void);
	~SnmpSendTraps();

	void InitSnmpTrapSettings();
	bool SendSnmpAlerts();

	void BindSnmpTrapVariables(HSNMP_VBL snmpVbList);
	void BindSnmpTrapTypeOid(HSNMP_VBL snmpVbList);

	void BindSnmpTimeTicks(const std::string &strOid, const smiTIMETICKS &strTimeTickData, HSNMP_VBL snmpVbList);
	void BindSnmpOid(const std::string &strOid, const std::string &strOidData, HSNMP_VBL snmpVbList);	
	void BindSnmpStringVar(const std::string &strOid, const std::string &strOctect, HSNMP_VBL snmpVbList);
	
	SnmpTrapSettings m_trapSettings;
};