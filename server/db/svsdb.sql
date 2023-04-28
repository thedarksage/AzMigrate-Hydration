SET FOREIGN_KEY_CHECKS = 0;
SET SQL_MODE="NO_AUTO_VALUE_ON_ZERO";
--
-- Database: svsdb1
-- ------------------------------------------------------
--
-- Table structure for table `ApplicationInstance`
--

CREATE TABLE `ApplicationInstance` (
  `id` int(11) NOT NULL,
  `hostId` varchar(150) NOT NULL DEFAULT '',
  `storageGroupName` varchar(150) DEFAULT NULL,
  `dbName` varchar(150) NOT NULL DEFAULT '',
  `dbPath` varchar(255) NOT NULL DEFAULT '',
  `logPath` varchar(255) NOT NULL DEFAULT '',
  CONSTRAINT `hosts_ApplicationInstance` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `Applications`
--

CREATE TABLE `Applications` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(150) NOT NULL DEFAULT '',
  `Alias` varchar(150) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `BPMAllocations`
--

CREATE TABLE `BPMAllocations` (
  `Id` int(11) NOT NULL AUTO_INCREMENT,
  `Policyid` int(11) NOT NULL DEFAULT '0',
  `sourceHostId` varchar(40) DEFAULT NULL,
  `SourceIP` varchar(15) NOT NULL DEFAULT '',
  `targetHostId` varchar(40) DEFAULT NULL,
  `TargetIP` varchar(15) NOT NULL DEFAULT '',
  `AllocatedBandwidth` int(14) NOT NULL DEFAULT '0',
  `VPNServerPort` int(8) NOT NULL default '0',
  PRIMARY KEY (`Id`),
  KEY `BPMAllocations_TargetIP` (`TargetIP`),
  KEY `BPMAllocations_Policyid` (`Policyid`),
  KEY `BPMAllocations_sourceHostId` (`sourceHostId`),
  CONSTRAINT `hosts_BPMAllocations` FOREIGN KEY (`sourceHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `BPMPolicies`
--
CREATE TABLE `BPMPolicies` (
  `Id` int(11) NOT NULL AUTO_INCREMENT,
  `PolicyName` varchar(50) NOT NULL DEFAULT '',
  `PolicyDescription` varchar(250) NOT NULL DEFAULT '',
  `Type` varchar(20) NOT NULL DEFAULT '',
  `DefaultPolicy` int(1) NOT NULL DEFAULT '0',
  `sourceHostId` varchar(40) DEFAULT NULL,
  `SourceIP` varchar(15) NOT NULL DEFAULT '',
  `Status` int(1) NOT NULL DEFAULT '0',
  `Priority` int(10) NOT NULL DEFAULT '0',
  `Share` int(1) NOT NULL DEFAULT '0',
  `CumulativeBandwidth` int(14) NOT NULL DEFAULT '0',
  `ScheduleType` int(1) NOT NULL DEFAULT '0',
  `ScheduleFromTime` varchar(5) NOT NULL DEFAULT '',
  `ScheduleTOTime` varchar(5) NOT NULL DEFAULT '',
  `Schedule` varchar(250) NOT NULL DEFAULT '',
  `lastRunDate` date NOT NULL DEFAULT '0000-00-00',
  `networkDeviceName` varchar(240) DEFAULT NULL,
  `isActivePolicy` tinyint(1) DEFAULT '0',
  `isBPMEnabled` tinyint(1) DEFAULT '0',
  PRIMARY KEY (`Id`),
  KEY `BPMPolicies_Type` (`Type`),
  KEY `BPMPolicies_sourceHostId` (`sourceHostId`),
  KEY `BPMPolicies_networkDeviceName` (`networkDeviceName`),
  KEY `BPMPolicies_Priority` (`Priority`),
  CONSTRAINT `hosts_BPMPolicies` FOREIGN KEY (`sourceHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `Exchange2010Mailstore`
--

CREATE TABLE `Exchange2010Mailstore` (
  `mailstoreId` int(11) NOT NULL AUTO_INCREMENT,
  `applicationInstanceId` int(11) DEFAULT NULL,
  `mailStoreName` varchar(255) DEFAULT NULL,
  `mailStoreType` varchar(40) DEFAULT NULL,
  `mailstoreStatus` enum('Online','Offline') DEFAULT NULL,
  `dbpath` varchar(255) DEFAULT NULL,
  `logpath` varchar(255) DEFAULT NULL,
  `publicFolderHost` varchar(255) DEFAULT NULL,
  `publicFolderMailStore` varchar(255) DEFAULT NULL,
  `copyHosts` varchar(255) DEFAULT NULL,
  `distinguishedName` varchar(255) DEFAULT NULL,
  `edbFilePath` text,
  `logFolderPath` text,
  `guid` varchar(40) NOT NULL,
  `mountInfo` text,
  PRIMARY KEY (`mailstoreId`),
  KEY `applicationInstanceId` (`applicationInstanceId`),
  KEY `mailStoreName` (`mailStoreName`),
  KEY `mailStoreType` (`mailStoreType`),
  CONSTRAINT `applicationHosts_Exchange2010Mailstore` FOREIGN KEY (`applicationInstanceId`) REFERENCES `applicationHosts` (`applicationInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `ExchangeDAG`
--
CREATE TABLE `ExchangeDAG` (
  `DAGName` varchar(40) NOT NULL,
  `applicationInstanceIds` varchar(255) DEFAULT NULL,
  `participantHosts` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`DAGName`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `accessControlGroup`
--
CREATE TABLE `accessControlGroup` (
  `accessControlGroupId` varchar(255) NOT NULL,
  `accessControlGroupName` varchar(255) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  `type` varchar(255) DEFAULT 'ACG',
  PRIMARY KEY (`accessControlGroupId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `accessControlGroupPorts`
--

CREATE TABLE `accounts` (
  `accountId` int(11) NOT NULL AUTO_INCREMENT,
  `accountName` varchar(160) DEFAULT NULL,
  `domain` varchar(100) DEFAULT '',
  `userName` text,
  `password` text,
  `accountType` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`accountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `accounts`
--

CREATE TABLE `accessControlGroupPorts` (
  `exportedInitiatorPortWwn` varchar(255) NOT NULL,
  `accessControlGroupId` varchar(255) NOT NULL,
  PRIMARY KEY (`exportedInitiatorPortWwn`,`accessControlGroupId`),
  KEY `accessControlGroup_accessControlGroupPorts` (`accessControlGroupId`),
  CONSTRAINT `accessControlGroup_accessControlGroupPorts` FOREIGN KEY (`accessControlGroupId`) REFERENCES `accessControlGroup` (`accessControlGroupId`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `agentCalls`
--

CREATE TABLE `agentCalls` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `hostId` varchar(255) NOT NULL DEFAULT '',
  `callName` varchar(255) NOT NULL DEFAULT '',
  `payload` longtext NOT NULL,
  `callResponse` longtext,
  `callTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `callDuration` double DEFAULT NULL,
  `timePassedSinceLastCall` bigint(20) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  CONSTRAINT `hosts_agentCalls` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `agentJobs`
--

CREATE TABLE agentJobs (
  `jobId` bigint(20) NOT NULL AUTO_INCREMENT,
  `hostId` varchar(40) DEFAULT NULL,
  `jobType` varchar(255) NOT NULL COMMENT 'For now the only job type is MTPrepareForAadMigration', 
  `inputPayload` text,
  `outputPayload` text,
  `jobStatus` enum('Pending', 'InProgress', 'Completed', 'Failed', 'Cancelled') default 'Pending',
  `jobCreationTime` datetime DEFAULT NULL,
  `jobUpdateTime` datetime DEFAULT NULL,
  `errorDetails` text,
  `activityId` varchar(255) DEFAULT NULL,
  `clientRequestId` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`jobId`),
  KEY `hostId` (`hostId`),
  KEY `activityId` (`activityId`),
  KEY `clientRequestId` (`clientRequestId`),
  KEY `jobType` (`jobType`),
  CONSTRAINT `hosts_agentJobs` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
)ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `agentInstallerDetails`
--

CREATE TABLE `agentInstallerDetails` (
  `ipaddress` varchar(320) NOT NULL,
  `agentInstallerLocation` varchar(255) NOT NULL,
  `state` tinyint(2) DEFAULT '1',
  `lastUpdatedTime` datetime NOT NULL,
  `logFileLocation` varchar(255) DEFAULT NULL,
  `logFile` text,
  `startTime` datetime NOT NULL,
  `endTime` datetime NOT NULL,
  `jobId` bigint(20) NOT NULL AUTO_INCREMENT,
  `statusMessage` text,
  `hostId` varchar(40) DEFAULT NULL,
  `agentinstallationPath` varchar(255) NOT NULL,
  `rebootRequired` tinyint(1) NOT NULL DEFAULT '0',
  `build` varchar(100) DEFAULT NULL,
  `upgradeRequest` tinyint(1) NOT NULL DEFAULT '0',
  `osType` varchar(25) DEFAULT NULL,
  `rxAgentInstallersId` int(11) NOT NULL DEFAULT '0',
  `errCode` varchar(50) NOT NULL DEFAULT '',
  `errPlaceHolders` text NOT NULL,
  `infrastructureVmId` varchar(255) NOT NULL,
  `installerExtendedErrors` longtext DEFAULT '',
  `telemetry` tinyint(1) DEFAULT '0',
  PRIMARY KEY (`jobId`),
  KEY `ipaddress` (`ipaddress`),
  KEY `state` (`state`),
  KEY `hostId` (`hostId`),
  KEY `jobId` (`jobId`),
  CONSTRAINT `agentInstallers_agentInstallerDetails` FOREIGN KEY (`ipaddress`) REFERENCES `agentInstallers` (`ipaddress`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `agentInstallers`
--

CREATE TABLE `agentInstallers` (
  `ipaddress` varchar(320) NOT NULL,
  `userName` varchar(255) NOT NULL,
  `password` varchar(255) DEFAULT NULL,
  `osType` tinyint(1) NOT NULL DEFAULT '1',
  `domain` varchar(100) NOT NULL DEFAULT '',
  `hostName` varchar(100) DEFAULT NULL,
  `osDetailType` varchar(25) DEFAULT NULL,
  `version` varchar(25) DEFAULT NULL,
  `cs_ip` varchar(320) NOT NULL,
  `port` int(5) NOT NULL,
  `agentFeature` varchar(25) NOT NULL,
  `parllelPushCount` int(5) NOT NULL DEFAULT '0',
  `agentType` varchar(15) NOT NULL,
  `pushServerId` varchar(40) NOT NULL,
  `apiRequestId` int(10) DEFAULT '0',
  `rxAgentInstallersId` int(11) NOT NULL DEFAULT '0',
  `communicationMode` enum('HTTP','HTTPS') DEFAULT 'HTTP',
  `accountId` int(11) DEFAULT NULL,
  `infrastructureVmId` varchar(255) NOT NULL,
  PRIMARY KEY (`ipaddress`),
  KEY `ipaddress` (`ipaddress`),
  KEY `agentInstallers_pushServerId` (`pushServerId`),
  KEY `agentInstallers_infrastructureVmId` (`infrastructureVmId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `apiRequest`
--
CREATE TABLE `apiRequest` (
  `apiRequestId` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `requestType` varchar(255) DEFAULT NULL,
  `requestXml` longtext,
  `requestTime` datetime DEFAULT NULL,
  `md5CheckSum` varchar(255) DEFAULT NULL,
  `state` enum('1','2','3','4') DEFAULT '1' COMMENT 'state : 1 = pending, 2 = in progress, 3 = success, 4 = failed',
  `activityId` varchar(255) DEFAULT NULL,
  `clientRequestId` varchar(255) DEFAULT NULL,
  `referenceId` varchar(255) DEFAULT NULL,
  `srsActivityId` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`apiRequestId`),
  KEY `apiRequest_requestXml` (`requestXml`(700)),
  KEY `apiRequest_md5CheckSum` (`md5CheckSum`),
  KEY `apiRequest_requestTime` (`requestTime`),
  KEY `apiRequest_state` (`state`),
  KEY `apiRequest_activityId` (`activityId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `apiRequestDetail`
--
CREATE TABLE `apiRequestDetail` (
  `apiRequestDetailId` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `apiRequestId` bigint(20) unsigned NOT NULL,
  `hostId` varchar(40) DEFAULT NULL,
  `requestdata` text COMMENT 'holds the input parameters',
  `state` enum('1','2','3','4') DEFAULT '1' COMMENT 'state : 1 = pending, 2 = in progress, 3 = success, 4 = failed',
  `status` text,
  `lastUpdateTime` datetime DEFAULT NULL,
  PRIMARY KEY (`apiRequestDetailId`),
  KEY `apiRequestDetail_apiRequestId` (`apiRequestId`),
  KEY `apiRequestDetail_requestData` (`requestdata`(700)),
  KEY `apiRequestDetail_state` (`state`),
  CONSTRAINT `apiRequestDetail_apiRequestId` FOREIGN KEY (`apiRequestId`) REFERENCES `apiRequest` (`apiRequestId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `appliance`
--
CREATE TABLE `appliance` (
  `applianceGuid` varchar(40) NOT NULL DEFAULT '',
  `applianceName` varchar(300) DEFAULT '',
  PRIMARY KEY (`applianceGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='To keep the private data for CX appliance luns';


--
-- Table structure for table `applianceCluster`
--
CREATE TABLE `applianceCluster` (
  `applianceId` varchar(255) NOT NULL DEFAULT '',
  `applianceIp` varchar(15) DEFAULT NULL,
  `applianceType` varchar(20) DEFAULT NULL,
  `dbSyncJobState` enum('ENABLED','DISABLED') DEFAULT 'DISABLED',
  PRIMARY KEY (`applianceId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `applianceFrBindingInitiators`
--
CREATE TABLE `applianceFrBindingInitiators` (
  `applianceFrBindingInitiatorPortWwn` varchar(255) NOT NULL COMMENT 'world wide name for this initiator. It is a key back to the hba ',
  `processServerId` varchar(40) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `state` enum('Stable','TransientPending','TransientComplete','RebootReconfigurePending','RebootReconfigureFailed') DEFAULT NULL,
  `error` int(32) DEFAULT '0',
  `groupName` varchar(256) NOT NULL,
  PRIMARY KEY (`applianceFrBindingInitiatorPortWwn`,`processServerId`,`fabricId`),
  KEY `processServer_applianceFrBindingInitiators` (`processServerId`),
  KEY `sanFabrics_applianceFrBindingInitiators` (`fabricId`),
  CONSTRAINT `sanFabrics_applianceFrBindingInitiators` FOREIGN KEY (`fabricId`) REFERENCES `sanFabrics` (`fabricId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `applianceNports_applianceFrBindingInitiators` FOREIGN KEY (`applianceFrBindingInitiatorPortWwn`) REFERENCES `applianceNports` (`appliancePortWwn`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `processServer_applianceFrBindingInitiators` FOREIGN KEY (`processServerId`) REFERENCES `processServer` (`processServerId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='appliance inititiators';


--
-- Table structure for table `applianceFrBindingTargets`
--
CREATE TABLE `applianceFrBindingTargets` (
  `applianceFrBindingTargetPortWwn` varchar(255) NOT NULL COMMENT 'world wide name for this initiator. It is a key back to the hba ',
  `processServerId` varchar(40) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `state` enum('Stable','TransientPending','TransientComplete','RebootReconfigurePending','RebootReconfigureFailed') DEFAULT NULL,
  `error` int(32) DEFAULT '0',
  PRIMARY KEY (`applianceFrBindingTargetPortWwn`,`processServerId`,`fabricId`),
  KEY `processServer_applianceFrBindingTargets` (`processServerId`),
  KEY `sanFabrics_applianceFrBindingTargets` (`fabricId`),
  CONSTRAINT `sanFabrics_applianceFrBindingTargets` FOREIGN KEY (`fabricId`) REFERENCES `sanFabrics` (`fabricId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `applianceNports_applianceFrBindingTargets` FOREIGN KEY (`applianceFrBindingTargetPortWwn`) REFERENCES `applianceNports` (`appliancePortWwn`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `processServer_applianceFrBindingTargets` FOREIGN KEY (`processServerId`) REFERENCES `processServer` (`processServerId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='appliance targets';


--
-- Table structure for table `applianceInitiators`
--
CREATE TABLE `applianceInitiators` (
  `applianceInitiatorPortWwn` varchar(255) NOT NULL COMMENT 'world wide name for this initiator. It is a key back to the hba ',
  `hostId` varchar(40) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `state` enum('Stable','TransientPending','TransientComplete','RebootReconfigurePending','RebootReconfigureFailed') DEFAULT NULL,
  `error` int(32) DEFAULT '0',
  `groupName` varchar(256) NOT NULL,
  PRIMARY KEY (`applianceInitiatorPortWwn`,`hostId`),
  CONSTRAINT `applianceNports_applianceInitiators` FOREIGN KEY (`applianceInitiatorPortWwn`, `hostId`) REFERENCES `applianceNports` (`appliancePortWwn`, `hostId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='appliance targets';


--
-- Table structure for table `applianceInitiatorsTargetLunMap`
--
CREATE TABLE `applianceInitiatorsTargetLunMap` (
  `applianceInitiatorPortWwn` varchar(255) NOT NULL COMMENT 'world wide name for this initiator. It is a key back to the hba ',
  `hostId` varchar(40) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `state` enum('Stable','TransientPending','TransientComplete','RebootReconfigurePending','RebootReconfigureFailed') DEFAULT NULL,
  `error` int(32) DEFAULT '0',
  `groupName` varchar(256) NOT NULL,
  `status` enum('1','0') DEFAULT '0',
  PRIMARY KEY (`applianceInitiatorPortWwn`,`hostId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='applianceInitiatorsTargetLunMap';


--
-- Table structure for table `applianceNodeMap`
--
CREATE TABLE `applianceNodeMap` (
  `applianceId` varchar(255) NOT NULL DEFAULT '',
  `nodeId` varchar(40) NOT NULL,
  `nodeIp` varchar(15) DEFAULT NULL,
  `nodeRole` enum('ACTIVE','PASSIVE','N/A') DEFAULT 'N/A',
  `dbTimeStamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `sequenceNumber` bigint(32) DEFAULT NULL,
  `lastDBSyncTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `lastDBSyncStatus` enum('Success','Failed','') DEFAULT '',
  `lastDBSyncStatusLog` text NOT NULL,
  PRIMARY KEY (`applianceId`,`nodeId`),
  CONSTRAINT `appCluster_appNodeMap` FOREIGN KEY (`applianceId`) REFERENCES `applianceCluster` (`applianceId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `applianceNports`
--
CREATE TABLE `applianceNports` (
  `appliancePortWwn` varchar(255) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `nodeWwn` varchar(255) DEFAULT NULL,
  `symbolicName` varchar(128) DEFAULT NULL,
  `pathState` tinyint(1) DEFAULT '1',
  `transportType` varchar(50) DEFAULT 'FC',
  `ipAddress` varchar(16) DEFAULT NULL,
  `portNo` int(4) DEFAULT NULL,
  PRIMARY KEY (`appliancePortWwn`,`hostId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `applianceTLNexus`
--
CREATE TABLE `applianceTLNexus` (
  `applianceTargetLunId` varchar(128) NOT NULL,
  `applianceFrBindingTargetPortWwn` varchar(255) NOT NULL,
  `processServerId` varchar(40) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `sanLunId` varchar(128) NOT NULL,
  `applianceTargetLunNumber` bigint(64) NOT NULL,
  PRIMARY KEY (`applianceTargetLunId`,`applianceFrBindingTargetPortWwn`,`processServerId`,`fabricId`,`sanLunId`,`applianceTargetLunNumber`),
  KEY `applianceFrBindingTargets_applianceTLNexus` (`applianceFrBindingTargetPortWwn`,`processServerId`,`fabricId`),
  KEY `sanLuns_applianceTLNexus` (`sanLunId`),
  CONSTRAINT `sanLuns_applianceTLNexus` FOREIGN KEY (`sanLunId`) REFERENCES `sanLuns` (`sanLunId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `applianceFrBindingTargets_applianceTLNexus` FOREIGN KEY (`applianceFrBindingTargetPortWwn`, `processServerId`, `fabricId`) REFERENCES `applianceFrBindingTargets` (`applianceFrBindingTargetPortWwn`, `processServerId`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `applianceTargetLunMapping`
--
CREATE TABLE `applianceTargetLunMapping` (
  `applianceTargetLunMappingId` varchar(40) NOT NULL,
  `sharedDeviceId` varchar(128) NOT NULL,
  `applianceTargetLunMappingNumber` bigint(11) NOT NULL,
  `processServerId` varchar(40) NOT NULL,
  PRIMARY KEY (`applianceTargetLunMappingId`,`sharedDeviceId`,`applianceTargetLunMappingNumber`,`processServerId`),
  KEY `sharedDevices_applianceTargetLunMapping` (`sharedDeviceId`),
  CONSTRAINT `sharedDevices_applianceTargetLunMapping` FOREIGN KEY (`sharedDeviceId`) REFERENCES `sharedDevices` (`sharedDeviceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `applianceTargets`
--
CREATE TABLE `applianceTargets` (
  `hostId` varchar(40) NOT NULL,
  `applianceTargetPortWwn` varchar(255) NOT NULL COMMENT 'world wide name for this initiator. It is a key back to the hba ',
  `fabricId` varchar(40) NOT NULL,
  `state` enum('Stable','TransientPending','TransientComplete','RebootReconfigurePending','RebootReconfigureFailed') DEFAULT NULL,
  `error` int(32) DEFAULT '0',
  PRIMARY KEY (`hostId`,`applianceTargetPortWwn`),
  KEY `applianceNports_applianceTargets` (`applianceTargetPortWwn`,`hostId`),
  CONSTRAINT `applianceNports_applianceTargets` FOREIGN KEY (`applianceTargetPortWwn`, `hostId`) REFERENCES `applianceNports` (`appliancePortWwn`, `hostId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='appliance targets';

--
-- Table structure for table `applicationGroups`
--
CREATE TABLE `applicationGroups` (
  `groupId` varchar(255) NOT NULL DEFAULT '',
  `hostIP` varchar(255) NOT NULL DEFAULT '',
  `hostName` varchar(255) NOT NULL DEFAULT '',
  `hostId` varchar(255) NOT NULL DEFAULT '',
  `role` varchar(255) NOT NULL DEFAULT '',
  `portDetails` varchar(255) NOT NULL DEFAULT '',
  `recoveryOrder` varchar(255) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `applicationHosts`
--
CREATE TABLE `applicationHosts` (
  `applicationInstanceId` int(11) NOT NULL AUTO_INCREMENT,
  `hostId` varchar(40) NOT NULL,
  `applicationType` varchar(40) DEFAULT NULL,
  `applicationVersion` varchar(150) DEFAULT NULL,
  `applicationEdition` varchar(150) DEFAULT NULL,
  `applicationInstanceName` varchar(40) DEFAULT NULL,
  `applicationCompatibility` varchar(40) DEFAULT NULL,
  `instanceGuid` varchar(40) DEFAULT NULL,
  `installPath` varchar(150) DEFAULT NULL,
  `isClustered` tinyint(1) DEFAULT NULL,
  `clusterId` varchar(40) DEFAULT NULL,
  `clusterName` varchar(150) DEFAULT NULL,
  `isProtected` tinyint(1) DEFAULT NULL,
  `discoveryVersion` varchar(40) DEFAULT NULL,
  `lastUpdatedTime` varchar(40) DEFAULT NULL,
  `dependentHosts` varchar(255) DEFAULT NULL,
  `appRole` varchar(50) DEFAULT NULL,
  `ipAddress` varchar(255) DEFAULT NULL,
  `dnsIp` varchar(255) DEFAULT NULL,
  `recoveryOrder` int(11) DEFAULT NULL,
  `groupId` varchar(255) DEFAULT NULL,
  `portDetails` longtext,
  PRIMARY KEY (`applicationInstanceId`),
  KEY `hostId` (`hostId`),
  KEY `applicationType` (`applicationType`),
  KEY `applicationInstanceName` (`applicationInstanceName`),
  CONSTRAINT `hosts_applicationHosts` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `applicationInstanceServiceHealth`
--
CREATE TABLE `applicationInstanceServiceHealth` (
  `applicationInstanceServiceHealthId` int(11) NOT NULL AUTO_INCREMENT,
  `applicationInstanceId` int(11) DEFAULT NULL,
  `serviceName` varchar(150) DEFAULT NULL,
  `serviceStartType` varchar(40) DEFAULT NULL,
  `serviceStatus` varchar(40) DEFAULT NULL,
  `logonUser` varchar(150) DEFAULT NULL,
  `dependencies` varchar(150) DEFAULT NULL,
  PRIMARY KEY (`applicationInstanceServiceHealthId`),
  KEY `applicationInstanceId` (`applicationInstanceId`),
  CONSTRAINT `applicationHosts_applicationInstanceServiceHealth` FOREIGN KEY (`applicationInstanceId`) REFERENCES `applicationHosts` (`applicationInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `applicationPlan`
--
CREATE TABLE `applicationPlan` (
  `planId` int(11) NOT NULL AUTO_INCREMENT,
  `planName` varchar(255) DEFAULT NULL,
  `planType` enum('APPLICATION','BULK','OTHER','CLOUD','CLOUD_MIGRATION','CLOUD_CLONE','CLOUD_BURST') NOT NULL DEFAULT 'APPLICATION',
  `solutionType` enum('HOST','FABRIC','PRISM','ARRAY','ESX','AZURE','AWS','VMWARE','PHYSICAL','HYPERV') DEFAULT 'HOST',
  `planHealth` enum('INACTIVE','HEALTHY','WARNING','CRITICAL') DEFAULT 'INACTIVE',
  `planCreationTime` bigint(20) DEFAULT '0',
  `batchResync` int(2) DEFAULT '0',  
  `applicationPlanStatus` enum('Readiness Check Pending','Readiness Check Success','Readiness Check Failed','Recovery Pending','Recovery Success','Recovery Failed','Configuration Pending','Configuration Success','Configuration Failed','Prepare Target Pending','Prepare Target Success','Prepare Target Failed','Active','InActive','Rollback InProgress','Rollback Completed','Rollback Failed','Recovery InProgress','Delete Pending','Delete InProgress','Delete Success','Delete Failed','Delete Configuration Failed') DEFAULT NULL,
  `dependentPlanId` int(11) DEFAULT '0',
  `rxPlanId` int(11) DEFAULT '0',
  `errorMessage` text,
  `planDetails` text,
  `TargetDataPlane` tinyint(3) DEFAULT '1',
  PRIMARY KEY (`planId`),
  KEY `applicationPlan_solutionType` (`solutionType`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `applicationPlanType`
--
CREATE TABLE `applicationPlanType` (
  `applicationPlanId` int(11) NOT NULL,
  `applicationPlanType` varchar(40) NOT NULL,
  KEY `applicationPlanId` (`applicationPlanId`),
  CONSTRAINT `applicationPlanType_ibfk_1` FOREIGN KEY (`applicationPlanId`) REFERENCES `applicationPlan` (`planId`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `applicationScenario`
--
CREATE TABLE `applicationScenario` (
  `scenarioId` int(11) NOT NULL AUTO_INCREMENT,
  `planId` int(11) DEFAULT NULL,
  `scenarioType` enum('DR Protection','Backup Protection','Bulk Protection','Planned Failover','Unplanned Failover','Failback','Fast Failback','Data Validation and Backup','DR - Exercise','Rollback','Profiler Protection','Recovery') DEFAULT NULL,
  `scenarioName` varchar(100) DEFAULT NULL,
  `scenarioDescription` varchar(255) DEFAULT NULL,
  `scenarioVersion` varchar(255) DEFAULT NULL,
  `scenarioCreationDate` datetime DEFAULT NULL,
  `scenarioDetails` longtext,
  `validationStatus` varchar(40) DEFAULT NULL,
  `executionStatus` int(11) DEFAULT NULL,
  `applicationType` varchar(40) DEFAULT NULL,
  `applicationVersion` text,
  `srcInstanceId` text,
  `trgInstanceId` text,
  `referenceId` int(11) NOT NULL,
  `resyncLimit` int(11) DEFAULT NULL,
  `autoProvision` varchar(40) DEFAULT NULL,
  `sourceId` text,
  `targetId` text,
  `sourceVolumes` text,
  `targetVolumes` text,
  `retentionVolumes` text,
  `reverseReplicationOption` tinyint(4) DEFAULT NULL,
  `protectionDirection` varchar(40) DEFAULT NULL,
  `currentStep` varchar(40) DEFAULT NULL,
  `creationStatus` varchar(40) DEFAULT NULL,
  `isPrimary` enum('0','1') DEFAULT '0',
  `isDisabled` enum('0','1') DEFAULT '0',
  `isModified` int(2) DEFAULT NULL,
  `reverseRetentionVolumes` text,
  `volpackVolumes` text,
  `volpackBaseVolume` text,
  `isVolumeResized` int(2) NOT NULL,
  `sourceArrayGuid` text,
  `targetArrayGuid` text,
  `isRetentionReuse` enum('0','1') DEFAULT '0',
  `rxScenarioId` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`scenarioId`),
  KEY `isPrimary` (`isPrimary`),
  KEY `isDisabled` (`isDisabled`),
  KEY `isModified` (`isModified`),
  KEY `currentStep` (`currentStep`),
  KEY `executionStatus` (`executionStatus`),
  KEY `applicationType` (`applicationType`),
  KEY `planId` (`planId`),
  KEY `scenarioType` (`scenarioType`),
  KEY `referenceId` (`referenceId`),
  CONSTRAINT `applicationPlan_applicationScenario` FOREIGN KEY (`planId`) REFERENCES `applicationPlan` (`planId`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `arrayAgentMap`
--
CREATE TABLE `arrayAgentMap` (
  `arrayGuid` varchar(40) NOT NULL DEFAULT '',
  `agentGuid` varchar(40) DEFAULT NULL,
  PRIMARY KEY (`arrayGuid`),
  CONSTRAINT `arrayInfo_arrayAgentMap` FOREIGN KEY (`arrayGuid`) REFERENCES `arrayInfo` (`arrayGuid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `arrayBindingNexus`
--
CREATE TABLE `arrayBindingNexus` (
  `fabricId` varchar(40) NOT NULL,
  `arrayGuid` varchar(255) NOT NULL,
  `applianceInitiatorPortWwn` varchar(255) NOT NULL,
  `sanTargetPortWwn` varchar(255) NOT NULL,
  `sharedDeviceId` varchar(128) NOT NULL,
  `sharedDeviceNumber` bigint(64) NOT NULL,
  `processServerId` varchar(40) NOT NULL,
  `sharedDeviceMapId` varchar(256) NOT NULL,
  `state` int(32) NOT NULL DEFAULT '0' COMMENT 'PROTECTED = 2,  DISCOVER_BINDING_DEVICE_PENDING = 9,DISCOVER_BINDING_DEVICE_DONE = 10',
  KEY `applianceFrBindingInitiators_arrayBindingNexus` (`applianceInitiatorPortWwn`,`processServerId`,`fabricId`),
  CONSTRAINT `applianceFrBindingInitiators_arrayBindingNexus` FOREIGN KEY (`applianceInitiatorPortWwn`, `processServerId`, `fabricId`) REFERENCES `applianceFrBindingInitiators` (`applianceFrBindingInitiatorPortWwn`, `processServerId`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `arrayITLNexus`
--
CREATE TABLE `arrayITLNexus` (
  `sanInitiatorPortWwn` varchar(244) NOT NULL DEFAULT '',
  `sanTargetPortWwn` varchar(244) NOT NULL DEFAULT '',
  `sanLunId` varchar(100) NOT NULL,
  `sanLunNumber` bigint(64) NOT NULL DEFAULT '0',
  `arrayGuid` varchar(36) NOT NULL DEFAULT '',
  `fabricId` varchar(40) NOT NULL DEFAULT '',
  `deleteState` tinyint(8) DEFAULT NULL,
  `hostName` varchar(100) NOT NULL DEFAULT '',
  `isGlobalLun` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`sanInitiatorPortWwn`,`sanTargetPortWwn`,`sanLunId`,`sanLunNumber`,`fabricId`,`arrayGuid`,`hostName`),
  KEY `arrayITLNexus_isGlobalLun` (`isGlobalLun`),
  KEY `arrayInfo_arrayITLNexus` (`arrayGuid`),
  CONSTRAINT `arrayInfo_arrayITLNexus` FOREIGN KEY (`arrayGuid`) REFERENCES `arrayInfo` (`arrayGuid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='initiator, target and lun mapping';


--
-- Table structure for table `arrayInfo`
--
CREATE TABLE `arrayInfo` (
  `arrayGuid` varchar(40) NOT NULL DEFAULT '',
  `vendor` varchar(40) DEFAULT NULL,
  `arrayType` varchar(40) DEFAULT NULL,
  `ipAddress` varchar(40) DEFAULT NULL,
  `login` varchar(40) DEFAULT NULL,
  `password` varchar(40) DEFAULT NULL,
  `agentGuid` varchar(40) DEFAULT NULL,
  `arrayName` varchar(40) DEFAULT NULL,
  `slNo` varchar(40) DEFAULT NULL,
  `modelNo` varchar(40) DEFAULT NULL,
  `otherInfo` text,
  `status` enum('REG-SUCCESS','REG-PENDING','REG-FAILED','DEL-PENDING','REG-CS-WITH-ARRAY-PENDING','UNREGISTERED','UNREG-FAILED','UNREG-PENDING') DEFAULT NULL,
  `isRegistered` enum('YES','NO') DEFAULT 'NO',
  `arrayConfigureTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `errorMessage` varchar(1024) DEFAULT NULL,
  PRIMARY KEY (`arrayGuid`),
  KEY `arrayInfo_status` (`status`),
  KEY `arrayInfo_isRegistered` (`isRegistered`),
  KEY `arrayInfo_agentGuid` (`agentGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `arrayLunMapInfo`
--
CREATE TABLE `arrayLunMapInfo` (
  `sharedDeviceId` varchar(128) NOT NULL,
  `arrayGuid` varchar(128) NOT NULL DEFAULT '',
  `protectedProcessServerId` varchar(40) DEFAULT NULL,
  `fabricId` varchar(40) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  `status` bigint(32) DEFAULT NULL,
  `lunType` enum('SOURCE','TARGET','RETENTION') DEFAULT NULL,
  `mountPoint` text,
  `fileSystem` varchar(255) DEFAULT NULL,
  `sourceSharedDeviceId` varchar(128) NOT NULL DEFAULT '',
  PRIMARY KEY (`sharedDeviceId`,`sourceSharedDeviceId`,`arrayGuid`),
  KEY `arrayInfo_state` (`state`),
  KEY `arrayInfo_fabricId` (`fabricId`),
  KEY `arrayInfo_protectedProcessServerId` (`protectedProcessServerId`),
  KEY `arrayInfo_arrayLunMapInfo` (`arrayGuid`),
  CONSTRAINT `arrayInfo_arrayLunMapInfo` FOREIGN KEY (`arrayGuid`) REFERENCES `arrayInfo` (`arrayGuid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `arrayLuns`
--
CREATE TABLE `arrayLuns` (
  `sanLunId` varchar(255) NOT NULL,
  `arrayGuid` varchar(255) NOT NULL,
  `deviceName` varchar(255) NOT NULL,
  `capacity` bigint(20) NOT NULL,
  `lunStatus` varchar(255) DEFAULT NULL,
  `volumeGroup` varchar(255) DEFAULT NULL,
  `mirrored` enum('MIRRORED_BY_ME','MIRRORED_BY_NONE','MIRRORED_BY_OTHER') DEFAULT 'MIRRORED_BY_NONE',
  `privateData` varchar(300) DEFAULT NULL,
  `iscsiAccess` enum('YES','NO') DEFAULT 'NO',
  `fcAccess` enum('YES','NO') DEFAULT 'NO',
  `applianceGuid` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`sanLunId`,`arrayGuid`),
  KEY `arrayLuns_mirrored` (`mirrored`),
  KEY `arrayLuns_privateData` (`privateData`),
  KEY `arrayInfo_arrayLuns` (`arrayGuid`),
  CONSTRAINT `arrayInfo_arrayLuns` FOREIGN KEY (`arrayGuid`) REFERENCES `arrayInfo` (`arrayGuid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='Storage array luns information';


--
-- Table structure for table `arraySanHostInfo`
--
CREATE TABLE `arraySanHostInfo` (
  `sanHostName` varchar(255) NOT NULL,
  `portId` varchar(255) NOT NULL,
  `portType` varchar(10) NOT NULL,
  `arrayGuid` varchar(255) NOT NULL,
  PRIMARY KEY (`arrayGuid`,`portId`),
  CONSTRAINT `arrayInfo_arraySanHostInfo` FOREIGN KEY (`arrayGuid`) REFERENCES `arrayInfo` (`arrayGuid`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `auditLog`
--
CREATE TABLE `auditLog` (
  `userName` varchar(100) NOT NULL DEFAULT '',
  `dateTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `ipAddress` varchar(50) NOT NULL DEFAULT '',
  `details` varchar(2000) DEFAULT '',
  `callerInfo` varchar(1000) DEFAULT '',
  KEY `auditLog_userName_index` (`userName`),
  KEY `auditLog_dateTime_index` (`dateTime`),
  KEY `auditLog_ipAddress_index` (`ipAddress`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `clusterAvailableVolumes`
--
CREATE TABLE `clusterAvailableVolumes` (
  `hostId` varchar(40) NOT NULL DEFAULT '',
  `deviceName` varchar(255) NOT NULL DEFAULT '',
  `deviceId` varchar(40) NOT NULL DEFAULT '',
  CONSTRAINT `hosts_clusterAvailableVolumes` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `clusterGroups`
--
CREATE TABLE `clusterGroups` (
  `clusterGroupId` varchar(40) NOT NULL,
  `vmHost` varchar(40) NOT NULL,
  `vmStatus` varchar(10) NOT NULL,
  PRIMARY KEY (`clusterGroupId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `clusterNetworkNameInfo`
--
CREATE TABLE `clusterNetworkNameInfo` (
  `applicationInstanceId` int(5) DEFAULT NULL,
  `virtualServerName` varchar(150) DEFAULT NULL,
  `ipAddress` varchar(40) DEFAULT NULL,
  `state` tinyint(1) DEFAULT NULL,
  KEY `applicationHosts_clusterNetworkNameInfo` (`applicationInstanceId`),
  CONSTRAINT `applicationHosts_clusterNetworkNameInfo` FOREIGN KEY (`applicationInstanceId`) REFERENCES `applicationHosts` (`applicationInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `clusterResourceDependencies`
--
CREATE TABLE `clusterResourceDependencies` (
  `clusterId` varchar(40) NOT NULL DEFAULT '',
  `clusterGroupId` varchar(40) NOT NULL DEFAULT '',
  `clusterResourceId` varchar(40) NOT NULL DEFAULT '',
  `clusterResourceName` varchar(40) NOT NULL DEFAULT '',
  `dependsOnId` varchar(40) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `clusterResources`
--
CREATE TABLE `clusterResources` (
  `clusterResourceId` varchar(40) NOT NULL,
  `devicePosition` varchar(20) NOT NULL,
  `description` varchar(255) DEFAULT NULL,
  `availableOnHost` varchar(40) NOT NULL,
  PRIMARY KEY (`clusterResourceId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `clusters`
--
CREATE TABLE `clusters` (
  `hostId` varchar(40) NOT NULL DEFAULT '',
  `deviceName` varchar(255) NOT NULL DEFAULT '',
  `deviceId` varchar(255) NOT NULL,
  `clusterName` varchar(255) NOT NULL DEFAULT '',
  `clusterGroupName` varchar(255) NOT NULL DEFAULT '',
  `clusterGroupId` varchar(40) NOT NULL DEFAULT '',
  `clusterruleId` bigint(11) DEFAULT '0',
  `exchangeVirtualServerName` varchar(255) DEFAULT '',
  `sqlVirtualServerName` varchar(255) DEFAULT '',
  `fileServerVirtualServerName` varchar(255) DEFAULT '',
  `clusterResourceName` varchar(255) NOT NULL DEFAULT '',
  `clusterResourceId` varchar(40) NOT NULL DEFAULT '',
  `clusterId` varchar(40) NOT NULL DEFAULT '',
  KEY `hostId` (`hostId`),
  KEY `deviceName` (`deviceName`),
  KEY `clusterGroupId` (`clusterGroupId`),
  KEY `clusterruleId` (`clusterruleId`),
  KEY `clusters_deviceId` (`deviceId`),
  KEY `clusters_clusterResourceId` (`clusterResourceId`),
  KEY `clusters_clusterId` (`clusterId`),
  CONSTRAINT `hosts_clusters` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `consistencyGroups`
--
CREATE TABLE `consistencyGroups` (
  `groupId` int(11) NOT NULL AUTO_INCREMENT,
  `policyId` int(11) NOT NULL,
  `groupName` varchar(255) DEFAULT NULL,
  `masterNode` varchar(20) NOT NULL DEFAULT '',
  `coordinateNodes` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`groupId`),
  KEY `policyId` (`policyId`),
  CONSTRAINT `policy_consistencygroups` FOREIGN KEY (`policyId`) REFERENCES `policy` (`policyId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `consistencyTagList`
--
CREATE TABLE `consistencyTagList` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `ValueName` varchar(150) NOT NULL DEFAULT '',
  `ValueData` varchar(150) NOT NULL DEFAULT '',
  `ValueType` varchar(150) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `cpuInfo`
--
CREATE TABLE `cpuInfo` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `hostId` varchar(40) DEFAULT NULL,
  `cpuNumber` varchar(40) DEFAULT NULL,
  `architecture` varchar(255) DEFAULT NULL,
  `manufacturer` varchar(255) DEFAULT NULL,
  `name` varchar(255) DEFAULT NULL,
  `noOfCores` tinyint(1) DEFAULT '1',
  `noOfLogicalProcessors` tinyint(1) DEFAULT '1',
  PRIMARY KEY (`id`),
  KEY `cpuInfo_hostId` (`hostId`),
  CONSTRAINT `cpuInfo_hostId` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `cxClusterLog`
--
CREATE TABLE `cxClusterLog` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `errorString` varchar(255) DEFAULT NULL,
  `timeString` varchar(255) DEFAULT NULL,
  `alertSent` tinyint(1) DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `cxPair`
--
CREATE TABLE `cxPair` (
  `hostId` varchar(40) NOT NULL,
  `ip_address` varchar(15) NOT NULL,
  `port_number` int(5) DEFAULT '80',
  `role` enum('ACTIVE','PASSIVE','N/A') DEFAULT 'N/A',
  `timeout` int(4) DEFAULT '0',
  `nat_ip` varchar(15) DEFAULT NULL,
  `clusterIp` varchar(15) DEFAULT NULL,
  `dbTimeStamp` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `state` bigint(32) DEFAULT '0',
  `sequenceNumber` bigint(32) DEFAULT '0',
  `lastUpdateTime` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `dashboardAlerts`
--
CREATE TABLE `dashboardAlerts` (
  `alertId` int(11) NOT NULL AUTO_INCREMENT,
  `errLevel` varchar(255) NOT NULL DEFAULT '',
  `errCategory` varchar(255) NOT NULL DEFAULT '',
  `errSummary` varchar(255) NOT NULL DEFAULT '',
  `errMessage` text NOT NULL,
  `errTemplateId` varchar(255) NOT NULL DEFAULT '',
  `errTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `errStartTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `errCount` int(11) NOT NULL DEFAULT '0',
  `hostId` varchar(255) NOT NULL,
  `errCode` varchar(255) NOT NULL DEFAULT '',
  `errPlaceholders` text NOT NULL,
  `eventCodesDependency` tinyint(1) NOT NULL DEFAULT '1',
  `errComponent` varchar(255) DEFAULT '',
  PRIMARY KEY (`alertId`),
  KEY `errLevel` (`errLevel`),
  KEY `errCategory` (`errCategory`),
  KEY `errSummary` (`errSummary`),
  KEY `errTemplateId` (`errTemplateId`),
  KEY `errTime` (`errTime`),
  KEY `hostId` (`hostId`),
  KEY `eventCodesDependency_indx` (`eventCodesDependency`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `discoveredInitiators`
--
CREATE TABLE `discoveredInitiators` (
  `discoveredPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`discoveredPortWwn`,`fabricId`),
  CONSTRAINT `discoveredNPorts_discoveredInitiators` FOREIGN KEY (`discoveredPortWwn`, `fabricId`) REFERENCES `discoveredNPorts` (`discoveredPortWwn`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `discoveredNPorts`
--
CREATE TABLE `discoveredNPorts` (
  `discoveredPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `discoveredNodeWwn` varchar(32) NOT NULL,
  `symbolicName` varchar(128) DEFAULT NULL,
  `hostId` varchar(256) DEFAULT NULL,
  `hostLabel` varchar(256) DEFAULT NULL,
  `portLabel` varchar(256) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`discoveredPortWwn`,`fabricId`),
  KEY `sanFabrics_discoveredNPorts` (`fabricId`),
  CONSTRAINT `sanFabrics_discoveredNPorts` FOREIGN KEY (`fabricId`) REFERENCES `sanFabrics` (`fabricId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `discoveredTargets`
--
CREATE TABLE `discoveredTargets` (
  `discoveredPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`discoveredPortWwn`,`fabricId`),
  CONSTRAINT `discoveredNPorts_discoveredTargets` FOREIGN KEY (`discoveredPortWwn`, `fabricId`) REFERENCES `discoveredNPorts` (`discoveredPortWwn`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `discoveredUnknownRoles`
--
CREATE TABLE `discoveredUnknownRoles` (
  `discoveredPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`discoveredPortWwn`,`fabricId`),
  CONSTRAINT `discoveredNPorts_discoveredUnknownRoles` FOREIGN KEY (`discoveredPortWwn`, `fabricId`) REFERENCES `discoveredNPorts` (`discoveredPortWwn`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `discoveredZoneITNexus`
--
CREATE TABLE `discoveredZoneITNexus` (
  `discoveredInitiatorPortWwn` varchar(32) NOT NULL,
  `discoveredTargetportWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `zoneName` varchar(32) NOT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`discoveredInitiatorPortWwn`,`discoveredTargetportWwn`,`fabricId`,`zoneName`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `discoveredZoneMembers`
--
CREATE TABLE `discoveredZoneMembers` (
  `zoneName` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `zoneMemberValue` varchar(32) NOT NULL,
  `zoneMemberType` varchar(40) NOT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`zoneName`,`fabricId`,`zoneMemberValue`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `discoveredZoneNSMembers`
--

CREATE TABLE `discoveredZoneNSMembers` (
  `discoveredPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `zoneName` varchar(32) NOT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`discoveredPortWwn`,`fabricId`,`zoneName`),
  KEY `discoveredZoneMembers_discoveredZoneNSMembers` (`zoneName`,`fabricId`,`discoveredPortWwn`),
  CONSTRAINT `discoveredZoneMembers_discoveredZoneNSMembers` FOREIGN KEY (`zoneName`, `fabricId`, `discoveredPortWwn`) REFERENCES `discoveredZoneMembers` (`zoneName`, `fabricId`, `zoneMemberValue`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `discoveredNPorts_discoveredZoneNSMembers` FOREIGN KEY (`discoveredPortWwn`, `fabricId`) REFERENCES `discoveredNPorts` (`discoveredPortWwn`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `discoveryNexus`
--

CREATE TABLE `discoveryNexus` (
  `sanInitiatorPortWwn` varchar(32) NOT NULL,
  `sanTargetPortWwn` varchar(32) NOT NULL,
  `virtualTargetPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `switchAgentId` varchar(40) NOT NULL,
  `virtualInitiatorPortWwn` varchar(32) NOT NULL,
  `dpcNumber` bigint(32) DEFAULT NULL,
  `virtualTargetFcIdIndex` bigint(32) DEFAULT NULL,
  `virtualInitiatorFcIdIndex` bigint(32) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`sanInitiatorPortWwn`,`sanTargetPortWwn`,`virtualTargetPortWwn`,`fabricId`,`switchAgentId`,`virtualInitiatorPortWwn`),
  KEY `virtualInitiators_discoveryNexus` (`virtualInitiatorPortWwn`,`fabricId`,`switchAgentId`),
  KEY `virtualTargets_discoveryNexus` (`virtualTargetPortWwn`,`fabricId`,`switchAgentId`),
  KEY `sanITNexus_discoveryNexus` (`sanInitiatorPortWwn`,`sanTargetPortWwn`,`fabricId`),
  CONSTRAINT `sanITNexus_discoveryNexus` FOREIGN KEY (`sanInitiatorPortWwn`, `sanTargetPortWwn`, `fabricId`) REFERENCES `sanITNexus` (`sanInitiatorPortWwn`, `sanTargetPortWwn`, `fabricId`) ON UPDATE CASCADE,
  CONSTRAINT `virtualInitiators_discoveryNexus` FOREIGN KEY (`virtualInitiatorPortWwn`, `fabricId`, `switchAgentId`) REFERENCES `virtualInitiators` (`virtualInitiatorPortWwn`, `fabricId`, `switchAgentId`) ON UPDATE CASCADE,
  CONSTRAINT `virtualTargets_discoveryNexus` FOREIGN KEY (`virtualTargetPortWwn`, `fabricId`, `switchAgentId`) REFERENCES `virtualTargets` (`virtualTargetPortWwn`, `fabricId`, `switchAgentId`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `diskGroups`
--
CREATE TABLE `diskGroups` (
  `hostId` varchar(40) NOT NULL,
  `deviceName` varchar(255) NOT NULL,
  `diskGroupName` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`hostId`,`deviceName`,`diskGroupName`),
  CONSTRAINT `hosts_diskGroups` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `diskPartitions`
--
CREATE TABLE `diskPartitions` (
  `diskPartitionId` varchar(255) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `diskId` varchar(255) NOT NULL,
  `isPartitionAtBlockZero` tinyint(3) unsigned DEFAULT NULL,
  KEY `diskPartitionId_indx` (`diskPartitionId`),
  KEY `hostId_indx` (`hostId`),
  KEY `diskId_indx` (`diskId`),
  CONSTRAINT `hosts_diskPartitions` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `dpsLogicalVolumes`
--
CREATE TABLE `dpsLogicalVolumes` (
  `deviceName` varchar(255) NOT NULL DEFAULT '',
  `id` varchar(40) NOT NULL DEFAULT '',
  `capacity` bigint(20) NOT NULL DEFAULT '0',
  PRIMARY KEY (`deviceName`,`id`),
  KEY `capacity` (`capacity`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `eACGKeyConstraint`
--
CREATE TABLE `eACGKeyConstraint` (
  `exportedInitiatorPortWwn` varchar(255) NOT NULL,
  PRIMARY KEY (`exportedInitiatorPortWwn`),
  CONSTRAINT `accessControlGroupPorts_eACGKeyConstraint` FOREIGN KEY (`exportedInitiatorPortWwn`) REFERENCES `accessControlGroupPorts` (`exportedInitiatorPortWwn`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `edKeyConstraint`
--
CREATE TABLE `edKeyConstraint` (
  `hostId` varchar(40) NOT NULL,
  `exportedDeviceName` varchar(255) NOT NULL,
  PRIMARY KEY (`hostId`,`exportedDeviceName`),
  CONSTRAINT `exportedDevices_eDKeyConstraint` FOREIGN KEY (`hostId`, `exportedDeviceName`) REFERENCES `exportedDevices` (`hostId`, `exportedDeviceName`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `errAlertable`
--
CREATE TABLE `errAlertable` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `hostid` varchar(40) NOT NULL,
  `errlvl` varchar(20) DEFAULT '',
  `errMessage` text,
  `errtime` datetime DEFAULT '0000-00-00 00:00:00',
  `module` tinyint(1) DEFAULT '0',
  `alert_type` tinyint(1) DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `hostid` (`hostid`),
  KEY `errMessage` (`errMessage`(300))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `errorCodes`
--
CREATE TABLE `errorCodes` (
  `errorCode` int(11) NOT NULL COMMENT '1 - HOSTNAME_ MATCH_ERROR, 2 - GET_VOLUME_NAME_FAILEDERROR, 3 - NESTED_MOUNT_POINT_ERROR, 4 - APPLICATION_DATA_ON_SYSTEM_DRIVE, 5 - MULTI_NIC_VALIDATION_ERROR, 6 - LCR_ENABLED_ERROR, 7 - CCR_ENABLED_ERROR, 8 - VERSION_MISMATCH_ERROR, 9 - EXCHANGE_ADMINGRO',
  `errorDescription` text,
  `fixSteps` text,
  `ruleId` int(11) DEFAULT NULL,
  `sideEffects` text,
  PRIMARY KEY (`errorCode`),
  KEY `ruleTemplate_errorCodes` (`ruleId`),
  CONSTRAINT `ruleTemplate_errorCodes` FOREIGN KEY (`ruleId`) REFERENCES `ruleTemplate` (`ruleId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `error_message`
--

CREATE TABLE `error_message` (
  `error_message_id` bigint(20) NOT NULL AUTO_INCREMENT,
  `error_id` varchar(255) NOT NULL DEFAULT '',
  `error_template_id` varchar(25) DEFAULT NULL,
  `summary` text NOT NULL,
  `message` mediumtext NOT NULL,
  `first_update_time` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `last_update_time` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `message_count` int(11) NOT NULL DEFAULT '0',
  `user_id` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`error_message_id`),
  KEY `error_id` (`error_id`),
  KEY `error_template_id` (`error_template_id`),
  KEY `user_id` (`user_id`),
  KEY `summary` (`summary`(767))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `error_policy`
--

CREATE TABLE `error_policy` (
  `error_policy_id` bigint(20) NOT NULL AUTO_INCREMENT,
  `error_template_id` varchar(25) DEFAULT NULL,
  `send_mail` tinyint(1) NOT NULL DEFAULT '0',
  `user_id` int(11) NOT NULL DEFAULT '0',
  `dispatch_interval` int(11) NOT NULL DEFAULT '0',
  `last_dispatch_time` bigint(20) NOT NULL DEFAULT '0',
  `send_trap` tinyint(1) NOT NULL DEFAULT '0',
  `monitor_display` tinyint(1) NOT NULL DEFAULT '0',
  `schecule_time` varchar(10) DEFAULT '',
  PRIMARY KEY (`error_policy_id`),
  KEY `error_policy_user_id` (`user_id`),
  KEY `error_policy_error_template_id` (`error_template_id`),
  KEY `error_policy_monitor_display` (`monitor_display`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `error_template`
--

CREATE TABLE `error_template` (
  `error_template_id` varchar(25) NOT NULL DEFAULT '',
  `level` enum('FATAL','ERROR','WARNING','NOTICE') DEFAULT 'NOTICE',
  `mail_subject` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`error_template_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `exchangeMailStores`
--

CREATE TABLE `exchangeMailStores` (
  `exchangeMailStoreId` int(5) NOT NULL AUTO_INCREMENT,
  `exchangeStorageGroupId` int(5) DEFAULT NULL,
  `mailStoreName` varchar(40) DEFAULT NULL,
  `mailStoreType` varchar(40) DEFAULT NULL,
  `dbPath` varchar(40) DEFAULT NULL,
  `publicFolderPath` varchar(40) DEFAULT NULL,
  `publicFolderHost` varchar(40) DEFAULT NULL,
  `publicFolderStorageGroup` varchar(40) DEFAULT NULL,
  `onlineStatus` varchar(40) DEFAULT NULL,
  `edbFilePath` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`exchangeMailStoreId`),
  KEY `mailStoreName` (`mailStoreName`),
  KEY `mailStoreType` (`mailStoreType`),
  KEY `exchangeStorageGroupId` (`exchangeStorageGroupId`),
  CONSTRAINT `exchangeStorageGroup_exchangeMailStores` FOREIGN KEY (`exchangeStorageGroupId`) REFERENCES `exchangeStorageGroup` (`exchangeStorageGroupId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `exchangeStorageGroup`
--

CREATE TABLE `exchangeStorageGroup` (
  `exchangeStorageGroupId` int(5) NOT NULL AUTO_INCREMENT,
  `applicationInstanceId` int(5) DEFAULT NULL,
  `serverRole` varchar(40) DEFAULT NULL,
  `serverType` varchar(20) DEFAULT NULL,
  `clusterStorageType` varbinary(15) DEFAULT NULL,
  `storageGroupName` varchar(40) DEFAULT NULL,
  `logPath` varchar(150) DEFAULT NULL,
  `distinguishedName` varchar(200) DEFAULT NULL,
  `LCRstatus` varchar(15) DEFAULT NULL,
  `CCRstatus` varchar(15) DEFAULT NULL,
  `SCRstatus` varchar(15) DEFAULT NULL,
  `onlineStatus` varchar(150) DEFAULT NULL,
  `logFolderPath` varchar(255) DEFAULT NULL,
  `StorageGroupSystemPath` varchar(255) DEFAULT NULL,
  `StorageGroupSystemVolume` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`exchangeStorageGroupId`),
  KEY `storageGroupName` (`storageGroupName`),
  KEY `applicationInstanceId` (`applicationInstanceId`),
  CONSTRAINT `applicationHosts_exchangeStorageGroup` FOREIGN KEY (`applicationInstanceId`) REFERENCES `applicationHosts` (`applicationInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;
--
-- Table structure for table `executionStepTasks`
--

CREATE TABLE `executionStepTasks` (
  `executionStepTaskId` int(11) NOT NULL AUTO_INCREMENT,
  `task` varchar(200) NOT NULL,
  `taskDescription` text,
  `taskStatus` tinyint(1) NOT NULL COMMENT '1-Queued,2-InProgress,3-Complete,4-Failed',
  `errorMessage` text,
  `fixSteps` text,
  `executionStepId` int(11) NOT NULL,
  `logPath` varchar(200) DEFAULT NULL,
  PRIMARY KEY (`executionStepTaskId`),
  KEY `executionStepTasks_task` (`task`),
  KEY `executionStepTasks_executionStepId` (`executionStepId`),
  CONSTRAINT `executionStepTasks_ibfk_1` FOREIGN KEY (`executionStepId`) REFERENCES `executionSteps` (`executionStepId`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `executionSteps`
--

CREATE TABLE `executionSteps` (
  `executionStepId` int(11) NOT NULL AUTO_INCREMENT,
  `planId` int(11) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `executionStep` varchar(100) NOT NULL,
  `status` tinyint(1) NOT NULL COMMENT '1-Queued,2-InProgress,3-Complete,4-Failed',
  PRIMARY KEY (`executionStepId`),
  KEY `executionSteps_planId` (`planId`),
  KEY `executionSteps_hostId` (`hostId`),
  KEY `executionSteps_executionStep` (`executionStep`),
  CONSTRAINT `executionSteps_ibfk_1` FOREIGN KEY (`planId`) REFERENCES `applicationPlan` (`planId`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `exportImportDetails`
--

CREATE TABLE `exportImportDetails` (
  `targetHostId` varchar(40) NOT NULL,
  `deviceName` varchar(255) NOT NULL,
  `initiatorHostId` varchar(40) DEFAULT NULL,
  `initiatorIp` varchar(16) DEFAULT NULL,
  `action` enum('Export','Unexport') NOT NULL DEFAULT 'Export',
  `exportType` enum('CIFS','ISCSI') NOT NULL DEFAULT 'CIFS',
  `targetPolicyId` int(11) DEFAULT NULL,
  `initiatorPolicyId` int(11) DEFAULT NULL,
  `scenarioId` int(11) DEFAULT NULL,
  `data` text,
  `status` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `exportedDevices`
--

CREATE TABLE `exportedDevices` (
  `hostId` varchar(40) NOT NULL,
  `exportedDeviceName` varchar(255) NOT NULL,
  `creationTime` datetime NOT NULL,
  PRIMARY KEY (`hostId`,`exportedDeviceName`),
  CONSTRAINT `hosts_exportedDevices` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;
--
-- Table structure for table `exportedFCLuns`
--

CREATE TABLE `exportedFCLuns` (
  `exportedFCLunId` varchar(128) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `exportedDeviceName` varchar(255) NOT NULL,
  `lunCapacity` bigint(20) NOT NULL,
  `blockSize` bigint(20) DEFAULT NULL,
  `vendorName` varchar(255) DEFAULT NULL,
  `modelNumber` varchar(255) DEFAULT NULL,
  `lunLabel` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`exportedFCLunId`,`hostId`,`exportedDeviceName`),
  KEY `exportedDevices_exportedFCLuns` (`hostId`,`exportedDeviceName`),
  KEY `IDX_exportedFCLuns_1` (`exportedFCLunId`,`hostId`),
  CONSTRAINT `exportedDevices_exportedFCLuns` FOREIGN KEY (`hostId`, `exportedDeviceName`) REFERENCES `exportedDevices` (`hostId`, `exportedDeviceName`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `exportedISCSILuns`
--

CREATE TABLE `exportedISCSILuns` (
  `exportedISCSILunId` varchar(128) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `exportedDeviceName` varchar(255) NOT NULL,
  `lunNumber` varchar(40) DEFAULT NULL,
  `lunCapacity` bigint(20) DEFAULT NULL,
  `initiatorIPAddress` varchar(100) DEFAULT NULL COMMENT 'This table should actually be called as iSCSI target LUNS. ',
  PRIMARY KEY (`exportedISCSILunId`,`hostId`,`exportedDeviceName`),
  KEY `exportedDevices_exportedISCSILuns` (`hostId`,`exportedDeviceName`),
  CONSTRAINT `exportedDevices_exportedISCSILuns` FOREIGN KEY (`hostId`, `exportedDeviceName`) REFERENCES `exportedDevices` (`hostId`, `exportedDeviceName`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `exportedITLNexus`
--

CREATE TABLE `exportedITLNexus` (
  `exportedInitiatorPortWwn` varchar(255) NOT NULL,
  `exportedLunNumber` bigint(64) NOT NULL,
  `exportedTargetPortWwn` varchar(255) NOT NULL,
  `exportedFCLunId` varchar(128) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `accessControlGroupId` varchar(40) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`exportedInitiatorPortWwn`,`exportedLunNumber`,`exportedTargetPortWwn`,`exportedFCLunId`,`hostId`,`accessControlGroupId`),
  KEY `exportedTLNexus_exportedITLNexus` (`exportedLunNumber`,`exportedTargetPortWwn`,`exportedFCLunId`,`hostId`),
  KEY `accessControlGroupPorts_exportedITLNexus` (`exportedInitiatorPortWwn`,`accessControlGroupId`),
  CONSTRAINT `accessControlGroupPorts_exportedITLNexus` FOREIGN KEY (`exportedInitiatorPortWwn`, `accessControlGroupId`) REFERENCES `accessControlGroupPorts` (`exportedInitiatorPortWwn`, `accessControlGroupId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `exportedTLNexus_exportedITLNexus` FOREIGN KEY (`exportedLunNumber`, `exportedTargetPortWwn`, `exportedFCLunId`, `hostId`) REFERENCES `exportedTLNexus` (`exportedLunNumber`, `exportedTargetPortWwn`, `exportedFCLunId`, `hostId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `exportedInitiators`
--

CREATE TABLE `exportedInitiators` (
  `exportedInitiatorPortWwn` varchar(255) NOT NULL,
  `nodeWwn` varchar(255) DEFAULT NULL,
  `portLabel` varchar(32) DEFAULT NULL,
  `hostLabel` varchar(32) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`exportedInitiatorPortWwn`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `exportedMountPoints`
--

CREATE TABLE `exportedMountPoints` (
  `mountPoint` varchar(100) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `exportedDeviceName` varchar(255) NOT NULL,
  PRIMARY KEY (`mountPoint`,`hostId`,`exportedDeviceName`),
  KEY `exportedDevices_exportedMountPoints` (`hostId`,`exportedDeviceName`),
  CONSTRAINT `exportedDevices_exportedMountPoints` FOREIGN KEY (`hostId`, `exportedDeviceName`) REFERENCES `exportedDevices` (`hostId`, `exportedDeviceName`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `exportedTLNexus`
--

CREATE TABLE `exportedTLNexus` (
  `exportedLunNumber` bigint(64) NOT NULL,
  `exportedTargetPortWwn` varchar(255) NOT NULL,
  `exportedFCLunId` varchar(128) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `state` bigint(32) DEFAULT NULL COMMENT 'CREATE_EXPORT_LUN_PENDING = 2100; CREATE_EXPORT_LUN_DONE = 2101; CREATE_EXPORT_LUN_ID_FAILURE = -2102; CREATE_EXPORT_LUN_NUMBER_FAILURE = -2103; DELETE_EXPORT_LUN_PENDING = 2110; DELETE_EXPORT_LUN_DONE = 2111; DELETE_EXPORT_LUN_NUMBER_FAILURE = -2112; DEL',
  PRIMARY KEY (`exportedLunNumber`,`exportedTargetPortWwn`,`exportedFCLunId`,`hostId`),
  KEY `exportedFCLuns_exportedTLNexus` (`exportedFCLunId`,`hostId`),
  KEY `applianceTargets_exportedTLNexus` (`hostId`,`exportedTargetPortWwn`),
  KEY `IDX_exportedTLNexus_1` (`exportedLunNumber`,`exportedTargetPortWwn`,`exportedFCLunId`),
  CONSTRAINT `applianceTargets_exportedTLNexus` FOREIGN KEY (`hostId`, `exportedTargetPortWwn`) REFERENCES `applianceTargets` (`hostId`, `applianceTargetPortWwn`),
  CONSTRAINT `exportedFCLuns_exportedTLNexus` FOREIGN KEY (`exportedFCLunId`, `hostId`) REFERENCES `exportedFCLuns` (`exportedFCLunId`, `hostId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `fileServerInfo`
--

CREATE TABLE `fileServerInfo` (
  `shareId` int(11) NOT NULL AUTO_INCREMENT,
  `applicationInstanceId` int(11) DEFAULT NULL,
  `shareName` varchar(255) NOT NULL,
  `MountPoint` varchar(255) DEFAULT NULL,
  `AbsolutePath` varchar(255) DEFAULT NULL,
  `ShareInfo` text,
  `SecurityInfo` text,
  `SecurityPwd` varchar(255) DEFAULT '0',
  `ShareRemark` varchar(255) DEFAULT '0',
  `ShareType` varchar(255) DEFAULT '0',
  `ShareUsers` varchar(255) DEFAULT '0',
  PRIMARY KEY (`shareId`),
  KEY `applicationInstanceId` (`applicationInstanceId`),
  CONSTRAINT `applicationHosts_fileServerInfo` FOREIGN KEY (`applicationInstanceId`) REFERENCES `applicationHosts` (`applicationInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `frBindingNexus`
--

CREATE TABLE `frBindingNexus` (
  `virtualTargetBindingPortWwn` varchar(32) NOT NULL,
  `switchAgentId` varchar(40) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `virtualInitiatorPortWwn` varchar(32) NOT NULL,
  `sanInitiatorPortWwn` varchar(32) NOT NULL,
  `sanTargetPortWwn` varchar(32) NOT NULL,
  `sanLunId` varchar(128) NOT NULL,
  `sanLunNumber` bigint(64) NOT NULL,
  `applianceTargetLunId` varchar(128) NOT NULL,
  `applianceFrBindingTargetPortWwn` varchar(32) NOT NULL,
  `processServerId` varchar(40) NOT NULL,
  `applianceTargetLunNumber` bigint(64) NOT NULL,
  `virtualTargetPortWwn` varchar(32) NOT NULL,
  `applianceFrBindingInitiatorPortWwn` varchar(32) NOT NULL,
  `dpcNumber` bigint(32) DEFAULT NULL,
  `frPolicy` varchar(40) DEFAULT NULL,
  `virtualTargetFcIdIndex` bigint(32) DEFAULT NULL,
  `virtualInitiatorFcIdIndex` bigint(32) DEFAULT NULL,
  `virtualTargetBindingFcIdIndex` bigint(32) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL COMMENT 'can be one of PROTECTED = 2,  CREATE_FR_BINDING_PENDING = 5, CREATE_FR_BINDING_DONE = 6, CREATE_AT_LUN_PENDING = 7,  CREATE_AT_LUN_DONE = 8, DISCOVER_BINDING_DEVICE_PENDING = 9, DISCOVER_BINDING_DEVICE_DONE = 10,  DELETE_FR_BINDING_PENDING = 11, DELETE_FR',
  PRIMARY KEY (`virtualTargetBindingPortWwn`,`switchAgentId`,`fabricId`,`virtualInitiatorPortWwn`,`sanInitiatorPortWwn`,`sanTargetPortWwn`,`sanLunId`,`sanLunNumber`,`applianceTargetLunId`,`applianceFrBindingTargetPortWwn`,`processServerId`,`applianceTargetLunNumber`,`virtualTargetPortWwn`,`applianceFrBindingInitiatorPortWwn`),
  KEY `virtualInitiators_frBindingNexus` (`virtualInitiatorPortWwn`,`fabricId`,`switchAgentId`),
  KEY `sanITLNexus_frBindingNexus` (`sanInitiatorPortWwn`,`sanTargetPortWwn`,`sanLunId`,`sanLunNumber`,`fabricId`),
  KEY `applianceTLNexus_frBindingNexus` (`applianceTargetLunId`,`applianceFrBindingTargetPortWwn`,`processServerId`,`fabricId`,`sanLunId`,`applianceTargetLunNumber`),
  KEY `virtualTargets_frBindingNexus` (`virtualTargetPortWwn`,`fabricId`,`switchAgentId`),
  KEY `applianceFrBindingInitiators_frBindingNexus` (`applianceFrBindingInitiatorPortWwn`,`processServerId`,`fabricId`),
  CONSTRAINT `applianceFrBindingInitiators_frBindingNexus` FOREIGN KEY (`applianceFrBindingInitiatorPortWwn`, `processServerId`, `fabricId`) REFERENCES `applianceFrBindingInitiators` (`applianceFrBindingInitiatorPortWwn`, `processServerId`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `applianceTLNexus_frBindingNexus` FOREIGN KEY (`applianceTargetLunId`, `applianceFrBindingTargetPortWwn`, `processServerId`, `fabricId`, `sanLunId`, `applianceTargetLunNumber`) REFERENCES `applianceTLNexus` (`applianceTargetLunId`, `applianceFrBindingTargetPortWwn`, `processServerId`, `fabricId`, `sanLunId`, `applianceTargetLunNumber`) ON UPDATE CASCADE,
  CONSTRAINT `sanITLNexus_frBindingNexus` FOREIGN KEY (`sanInitiatorPortWwn`, `sanTargetPortWwn`, `sanLunId`, `sanLunNumber`, `fabricId`) REFERENCES `sanITLNexus` (`sanInitiatorPortWwn`, `sanTargetPortWwn`, `sanLunId`, `sanLunNumber`, `fabricId`) ON UPDATE CASCADE,
  CONSTRAINT `virtualInitiators_frBindingNexus` FOREIGN KEY (`virtualInitiatorPortWwn`, `fabricId`, `switchAgentId`) REFERENCES `virtualInitiators` (`virtualInitiatorPortWwn`, `fabricId`, `switchAgentId`) ON UPDATE CASCADE,
  CONSTRAINT `virtualTargetBindings_frBindingNexus` FOREIGN KEY (`virtualTargetBindingPortWwn`, `switchAgentId`, `fabricId`) REFERENCES `virtualTargetBindings` (`virtualTargetBindingPortWwn`, `switchAgentId`, `fabricId`) ON UPDATE CASCADE,
  CONSTRAINT `virtualTargets_frBindingNexus` FOREIGN KEY (`virtualTargetPortWwn`, `fabricId`, `switchAgentId`) REFERENCES `virtualTargets` (`virtualTargetPortWwn`, `fabricId`, `switchAgentId`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='contans the ITL nexus, applicance TL nexus, VT, and VI used ';

--
-- Table structure for table `frbErrorReceivers`
--

CREATE TABLE `frbErrorReceivers` (
  `uid` int(11) NOT NULL DEFAULT '0',
  `jobConfigId` int(11) NOT NULL DEFAULT '0',
  `trapLevels` varchar(11) NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `frbGlobalSettings`
--

CREATE TABLE `frbGlobalSettings` (
  `autoClearLogYear` int(11) NOT NULL DEFAULT '0',
  `autoClearLogMonth` int(11) NOT NULL DEFAULT '0',
  `autoClearLogWeek` int(11) NOT NULL DEFAULT '0',
  `autoClearLogDay` int(11) NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `frbHeartbeat`
--

CREATE TABLE `frbHeartbeat` (
  `id` varchar(40) NOT NULL DEFAULT '',
  `heartbeat` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `frbJobConfig`
--

CREATE TABLE `frbJobConfig` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `groupId` bigint(20) unsigned NOT NULL DEFAULT '0',
  `jobOrder` int(11) NOT NULL DEFAULT '0',
  `sourceHost` varchar(40) NOT NULL DEFAULT 'X',
  `sourcePath` text NOT NULL,
  `destHost` varchar(40) NOT NULL DEFAULT 'X',
  `destPath` text NOT NULL,
  `scheduleId` bigint(20) unsigned NOT NULL DEFAULT '0',
  `optionsId` bigint(20) unsigned NOT NULL DEFAULT '0',
  `direction` int(2) NOT NULL DEFAULT '0',
  `rpoThreshold` int(11) NOT NULL DEFAULT '0',
  `preCommandSource` text NOT NULL,
  `postCommandSource` text NOT NULL,
  `preCommandTarget` text NOT NULL,
  `postCommandTarget` text NOT NULL,
  `cpuThrottle` int(4) NOT NULL DEFAULT '0',
  `progressThreshold` int(11) NOT NULL DEFAULT '0',
  `comment` varchar(80) NOT NULL DEFAULT 'X',
  `jobDescription` varchar(100) DEFAULT 'X',
  `deleted` tinyint(1) NOT NULL DEFAULT '0',
  `appname` varchar(255) DEFAULT 'Ungrouped',
  `sshEnable` tinyint(1) DEFAULT '0',
  `sshKeyType` tinyint(1) DEFAULT '-1',
  `sshCipherType` int(20) DEFAULT NULL,
  `secshPublicKey` text,
  `sshEncrPublicKey` text,
  `keygen_state` tinyint(1) DEFAULT '0',
  `sch_mode` tinyint(1) NOT NULL,
  `planId` int(10) DEFAULT '0',
  `parent_id` bigint(20) DEFAULT '0',
  `is_direction_changed` tinyint(1) DEFAULT '0',
  `alert_category` int(2) DEFAULT '0',
  `alert_on_success` int(2) DEFAULT '0',
  `alert_on_failure` int(2) DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `frbJobConfig_groupId` (`groupId`),
  KEY `frbJobConfig_jobOrder` (`jobOrder`),
  KEY `frbJobConfig_sourceHost` (`sourceHost`),
  KEY `frbJobConfig_destHost` (`destHost`),
  KEY `frbJobConfig_scheduleId` (`scheduleId`),
  KEY `frbJobConfig_optionsId` (`optionsId`),
  KEY `frbJobConfig_direction` (`direction`),
  KEY `frbJobConfig_progressThreshold` (`progressThreshold`),
  KEY `frbJobConfig_deleted` (`deleted`),
  KEY `frbJobConfig_planId` (`planId`),
  KEY `frbJobConfig_parent_id` (`parent_id`),
  CONSTRAINT `hosts_frbJobConfig_sourceHost` FOREIGN KEY (`sourceHost`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_frbJobConfig_destHost` FOREIGN KEY (`destHost`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `frbJobGroups`
--

CREATE TABLE `frbJobGroups` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `scheduleId` bigint(20) unsigned NOT NULL DEFAULT '0',
  `executionType` int(11) NOT NULL DEFAULT '0',
  `scheduleMode` int(4) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `frbJobTemplates`
--

CREATE TABLE `frbJobTemplates` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `templateName` varchar(255) NOT NULL DEFAULT '',
  `templateString` mediumtext NOT NULL,
  `templateType` tinyint(1) DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `frbJobs`
--

CREATE TABLE `frbJobs` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `jobConfigId` bigint(20) unsigned NOT NULL DEFAULT '0',
  `assignedState` int(11) NOT NULL DEFAULT '0',
  `currentState` int(11) NOT NULL DEFAULT '0',
  `daemonState` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `frbJobs_jobConfigId` (`jobConfigId`),
  KEY `frbJobs_assignedState` (`assignedState`),
  KEY `frbJobs_currentState` (`currentState`),
  KEY `frbJobs_daemonState` (`daemonState`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `frbOptions`
--

CREATE TABLE `frbOptions` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `port` int(11) NOT NULL DEFAULT '0',
  `tempDir` varchar(255) NOT NULL DEFAULT '',
  `recursive` tinyint(1) NOT NULL DEFAULT '0',
  `copyWholeFiles` tinyint(1) NOT NULL DEFAULT '0',
  `backupExisting` tinyint(1) NOT NULL DEFAULT '0',
  `backupDirectory` varchar(255) NOT NULL DEFAULT '',
  `backupSuffix` varchar(255) NOT NULL DEFAULT '',
  `blockSize` int(11) NOT NULL DEFAULT '0',
  `compress` tinyint(11) NOT NULL DEFAULT '0',
  `updateOnly` tinyint(1) NOT NULL DEFAULT '0',
  `updateExistingOnly` tinyint(1) NOT NULL DEFAULT '0',
  `ignoreExisting` tinyint(1) NOT NULL DEFAULT '0',
  `ignoreTimestamp` tinyint(11) NOT NULL DEFAULT '0',
  `alwaysChecksum` tinyint(1) NOT NULL DEFAULT '0',
  `checkSizeOnly` tinyint(11) NOT NULL DEFAULT '0',
  `modifyWindow` int(11) NOT NULL DEFAULT '0',
  `excludePattern` varchar(255) NOT NULL DEFAULT '',
  `includePattern` varchar(255) NOT NULL DEFAULT '',
  `deleteNonExisting` tinyint(1) NOT NULL DEFAULT '0',
  `deleteExcluded` tinyint(1) NOT NULL DEFAULT '0',
  `deleteAfter` tinyint(1) NOT NULL DEFAULT '0',
  `keepPartial` tinyint(1) NOT NULL DEFAULT '0',
  `retainSymlinks` tinyint(1) NOT NULL DEFAULT '0',
  `copyLinkReferents` tinyint(1) NOT NULL DEFAULT '0',
  `copyUnsafeLinks` tinyint(1) NOT NULL DEFAULT '0',
  `copySafeLinks` tinyint(1) NOT NULL DEFAULT '0',
  `preservePermissions` tinyint(1) NOT NULL DEFAULT '0',
  `preserveOwner` tinyint(1) NOT NULL DEFAULT '0',
  `preserveGroup` tinyint(1) NOT NULL DEFAULT '0',
  `preserveDevices` tinyint(1) NOT NULL DEFAULT '0',
  `preserveTimes` tinyint(1) NOT NULL DEFAULT '0',
  `dontCrossFileSystem` tinyint(1) NOT NULL DEFAULT '0',
  `bwLimit` int(11) NOT NULL DEFAULT '0',
  `ioTimeout` int(11) NOT NULL DEFAULT '0',
  `printStats` tinyint(1) NOT NULL DEFAULT '0',
  `progress` tinyint(1) NOT NULL DEFAULT '0',
  `verboseLevel` int(11) NOT NULL DEFAULT '0',
  `logFileFormat` varchar(255) NOT NULL DEFAULT '',
  `catchAll` varchar(255) NOT NULL DEFAULT '',
  `directoryContents` enum('directory_itself','directory_contents') NOT NULL DEFAULT 'directory_itself',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `frbSchedule`
--

CREATE TABLE `frbSchedule` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `startTime` datetime DEFAULT '0000-00-00 00:00:00',
  `repeat` int(4) DEFAULT '0',
  `everyDay` int(4) DEFAULT '0',
  `everyHour` int(4) DEFAULT '0',
  `everyMinute` int(4) DEFAULT '0',
  `atYear` int(5) DEFAULT '0',
  `atMonth` int(5) DEFAULT '0',
  `atDay` int(4) DEFAULT '0',
  `atHour` int(4) DEFAULT '0',
  `atMinute` int(4) DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `frbStatus`
--

CREATE TABLE `frbStatus` (
  `id` bigint(20) unsigned NOT NULL,
  `jobConfigId` bigint(20) unsigned NOT NULL DEFAULT '0',
  `startTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `endTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `exitCode` int(4) NOT NULL DEFAULT '0',
  `exitReason` enum('exit_none','exit_user_cancel','exit_process_finished') NOT NULL DEFAULT 'exit_none',
  `statusUpdateTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `totalProgressBytes` bigint(20) NOT NULL DEFAULT '0',
  `currentProgressBytes` bigint(20) NOT NULL DEFAULT '0',
  `compressTotalBytes` bigint(20) NOT NULL DEFAULT '0',
  `compressDifferentialBytes` bigint(20) NOT NULL DEFAULT '0',
  `compressSentBytes` bigint(20) NOT NULL DEFAULT '0',
  `log` longtext NOT NULL,
  `logOffset` int(8) NOT NULL DEFAULT '0',
  `logComplete` tinyint(1) NOT NULL DEFAULT '0',
  `lastProgressTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `rollOverLine` text NOT NULL,
  `daemonLogPath` mediumtext,
  PRIMARY KEY (`id`),
  KEY `frbStatus_jobConfigId` (`jobConfigId`),
  KEY `frbStatus_startTime` (`startTime`),
  KEY `frbStatus_endTime` (`endTime`),
  KEY `frbStatus_exitCode` (`exitCode`),
  KEY `frbStatus_exitReason` (`exitReason`),
  KEY `frbStatus_totalProgressBytes` (`totalProgressBytes`),
  KEY `frbStatus_currentProgressBytes` (`currentProgressBytes`),
  KEY `frbStatus_compressTotalBytes` (`compressTotalBytes`),
  KEY `frbStatus_compressDifferentialBytes` (`compressDifferentialBytes`),
  KEY `frbStatus_compressSentBytes` (`compressSentBytes`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `fxNetworkMap`
--

CREATE TABLE `fxNetworkMap` (
  `cxip` varchar(40) DEFAULT NULL,
  `agentId` varchar(40) DEFAULT NULL,
  `csId` varchar(40) DEFAULT NULL,
  `device_name` varchar(255) DEFAULT NULL,
  KEY `fxNetworkMap_agentId` (`agentId`),
  KEY `fxNetworkMap_csId` (`csId`),
  CONSTRAINT `fxNetworkMap_agentId` FOREIGN KEY (`agentId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fxNetworkMap_csId` FOREIGN KEY (`csId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `globalHealthSettings`
--

CREATE TABLE `globalHealthSettings` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `planName` varchar(255) DEFAULT NULL,
  `rpoHealth` varchar(20) DEFAULT NULL,
  `dataFlowHealth` varchar(20) DEFAULT NULL,
  `dataUploadHealth` varchar(20) DEFAULT NULL,
  `noCommunication` varchar(20) DEFAULT NULL,
  `targetCacheHealth` varchar(20) DEFAULT NULL,
  `pauseHealth` varchar(20) DEFAULT NULL,
  `dataConsistencyHealth` varchar(20) DEFAULT NULL,
  `retentionHealth` varchar(20) DEFAULT NULL,
  `commonConsistencyHealth` varchar(20) DEFAULT NULL,
  `applicationProtectionHealth` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `hdsStorageArray`
--

CREATE TABLE `hdsStorageArray` (
  `subsystemId` varchar(40) NOT NULL,
  `controller0IpAddress` varchar(16) NOT NULL,
  `controller1IpAddress` varchar(16) NOT NULL,
  `dataPort0AIpAddress` varchar(16) NOT NULL,
  `dataPort0AIpPortNumber` int(10) NOT NULL DEFAULT '3260',
  `dataPort0BIpAddress` varchar(16) NOT NULL,
  `dataPort0BIpPortNumber` int(10) NOT NULL DEFAULT '3260',
  `dataPort1AIpAddress` varchar(16) NOT NULL,
  `dataPort1AIpPortNumber` int(10) NOT NULL DEFAULT '3260',
  `dataPort1BIpAddress` varchar(16) NOT NULL,
  `dataPort1BIpPortNumber` int(10) NOT NULL DEFAULT '3260',
  `status` varchar(40) NOT NULL DEFAULT 'Online' COMMENT 'State = "Online" | "Online (failed over)" | "Failed" | "Degraded"',
  `degradedInfo` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`subsystemId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `hdsStorageArrayHost`
--

CREATE TABLE `hdsStorageArrayHost` (
  `hostId` varchar(40) NOT NULL,
  `subsystemId` varchar(40) NOT NULL,
  PRIMARY KEY (`hostId`,`subsystemId`),
  KEY `hdsStorageArray_hdsStorageArrayHost` (`subsystemId`),
  CONSTRAINT `hdsStorageArray_hdsStorageArrayHost` FOREIGN KEY (`subsystemId`) REFERENCES `hdsStorageArray` (`subsystemId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `healthReport`
--

CREATE TABLE `healthReport` (
  `sourceHostId` varchar(40) NOT NULL DEFAULT '',
  `sourceDeviceName` varchar(255) NOT NULL DEFAULT '',
  `destinationHostId` varchar(255) NOT NULL DEFAULT '',
  `destinationDeviceName` varchar(255) NOT NULL DEFAULT '',
  `reportDate` date NOT NULL DEFAULT '0000-00-00',
  `dataChangeWithCompression` double DEFAULT NULL,
  `dataChangeWithoutCompression` double DEFAULT NULL,
  `maxRPO` float NOT NULL DEFAULT '0',
  `retentionWindow` float NOT NULL DEFAULT '0',
  `protectionCoverage` float NOT NULL DEFAULT '0',
  `rpoHealth` float NOT NULL DEFAULT '0',
  `throttleHealth` float NOT NULL DEFAULT '0',
  `retentionHealth` float NOT NULL DEFAULT '0',
  `resyncHealth` float NOT NULL DEFAULT '0',
  `replicationHealth` float NOT NULL DEFAULT '0',
  `rpoTreshold` int(20) DEFAULT NULL,
  `retention_policy_duration` float DEFAULT NULL,
  `availableConsistencyPoints` int(20) NOT NULL DEFAULT '0',
  `pairId` int(11) NOT NULL DEFAULT '0',
  `planId` int(10) DEFAULT NULL,
  KEY `sourceHostId` (`sourceHostId`),
  KEY `sourceDeviceName` (`sourceDeviceName`),
  KEY `destinationHostId` (`destinationHostId`),
  KEY `destinationDeviceName` (`destinationDeviceName`),
  KEY `reportDate` (`reportDate`),
  KEY `pairId` (`pairId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `hostApplianceTargetLunMapping`
--

CREATE TABLE `hostApplianceTargetLunMapping` (
  `hostId` varchar(128) NOT NULL,
  `sharedDeviceId` varchar(128) NOT NULL,
  `applianceTargetLunMappingId` varchar(40) NOT NULL,
  `applianceTargetLunMappingNumber` bigint(11) NOT NULL,
  `exportedInitiatorPortWwn` varchar(255) NOT NULL,
  `accessControlGroupId` varchar(40) NOT NULL,
  `processServerId` varchar(40) NOT NULL,
  `applianceInitiatorPortWwn` varchar(255) NOT NULL,
  `applianceTargetPortWwn` varchar(255) NOT NULL,
  `fabricId` varchar(40) DEFAULT NULL,
  `srcDeviceName` varchar(255) NOT NULL DEFAULT '',
  `syncDeviceName` varchar(128) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  `status` bigint(32) NOT NULL,
  KEY `hostSharedDeviceMapping_hostApplianceTargetLunMapping` (`sharedDeviceId`,`hostId`,`exportedInitiatorPortWwn`,`srcDeviceName`),
  KEY `applianceTargetLunMapping_hostApplianceTargetLunMapping` (`applianceTargetLunMappingId`,`sharedDeviceId`,`applianceTargetLunMappingNumber`,`processServerId`),
  KEY `accessControlGroupPorts_hostApplianceTargetLunMapping` (`accessControlGroupId`),
  KEY `applianceTargets_hostApplianceTargetLunMapping` (`applianceTargetPortWwn`,`processServerId`),
  CONSTRAINT `applianceTargets_hostApplianceTargetLunMapping` FOREIGN KEY (`applianceTargetPortWwn`, `processServerId`) REFERENCES `applianceTargets` (`applianceTargetPortWwn`, `hostId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `accessControlGroupPorts_hostApplianceTargetLunMapping` FOREIGN KEY (`accessControlGroupId`) REFERENCES `accessControlGroupPorts` (`accessControlGroupId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `applianceTargetLunMapping_hostApplianceTargetLunMapping` FOREIGN KEY (`applianceTargetLunMappingId`, `sharedDeviceId`, `applianceTargetLunMappingNumber`, `processServerId`) REFERENCES `applianceTargetLunMapping` (`applianceTargetLunMappingId`, `sharedDeviceId`, `applianceTargetLunMappingNumber`, `processServerId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hostSharedDeviceMapping_hostApplianceTargetLunMapping` FOREIGN KEY (`sharedDeviceId`, `hostId`, `exportedInitiatorPortWwn`, `srcDeviceName`) REFERENCES `hostSharedDeviceMapping` (`sharedDeviceId`, `hostId`, `sanInitiatorPortWwn`, `srcDeviceName`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `hostIscsiInitiator`
--

CREATE TABLE `hostIscsiInitiator` (
  `hostIscsiInitiatorName` varchar(255) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  PRIMARY KEY (`hostIscsiInitiatorName`,`hostId`),
  CONSTRAINT `hosts_hostIscsiInitiator` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `hostNetworkAddress`
--

CREATE TABLE `hostNetworkAddress` (
  `hostId` varchar(40) NOT NULL DEFAULT '',
  `deviceName` varchar(255) DEFAULT '',
  `ipAddress` varchar(50) DEFAULT NULL,
  `deviceType` tinyint(1) NOT NULL DEFAULT '0',
  `nicSpeed` varchar(30) DEFAULT NULL,
  `macAddress` varchar(100) DEFAULT NULL,
  `gateway` varchar(255) DEFAULT NULL,
  `dnsHostName` varchar(255) DEFAULT NULL,
  `dnsServerAddresses` varchar(255) DEFAULT NULL,
  `networkAddressIndex` tinyint(1) DEFAULT NULL,
  `isDhcpEnabled` enum('0','1') DEFAULT '0',
  `manufacturer` varchar(255) DEFAULT NULL,
  `subnetMask` varchar(255) DEFAULT NULL,
  `ipType` tinyint(3) DEFAULT '0',
  KEY `hostNetworkAddress_hostId` (`hostId`),
  KEY `hostNetworkAddress_ipAddress` (`ipAddress`),
  CONSTRAINT `hosts_hostNetworkAddress` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `hostNicInfo`
--

CREATE TABLE `hostNicInfo` (
  `macAddress` varchar(40) NOT NULL,
  `hostId` varchar(40) DEFAULT NULL,
  `ipAddress` varchar(40) DEFAULT NULL,
  PRIMARY KEY (`macAddress`),
  KEY `hostId` (`hostId`),
  CONSTRAINT `hosts_hostNicInfo` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `hostSharedDeviceMapping`
--

CREATE TABLE `hostSharedDeviceMapping` (
  `sharedDeviceId` varchar(128) NOT NULL,
  `hostId` varchar(128) NOT NULL,
  `sanInitiatorPortWwn` varchar(255) NOT NULL DEFAULT '',
  `srcDeviceName` varchar(255) NOT NULL DEFAULT '',
  `replicationState` bigint(32) DEFAULT NULL,
  `deleteState` tinyint(8) DEFAULT NULL,
  `resyncRequired` tinyint(4) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`sharedDeviceId`,`hostId`,`sanInitiatorPortWwn`,`srcDeviceName`),
  KEY `sanInitiators_hostSharedDeviceMapping` (`sanInitiatorPortWwn`),
  CONSTRAINT `sanInitiators_hostSharedDeviceMapping` FOREIGN KEY (`sanInitiatorPortWwn`) REFERENCES `sanInitiators` (`sanInitiatorPortWwn`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `sharedDevices_hostSharedDeviceMapping` FOREIGN KEY (`sharedDeviceId`) REFERENCES `sharedDevices` (`sharedDeviceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `hostVssProviderDetails`
--

CREATE TABLE `hostVssProviderDetails` (
  `hostVssProviderDetailsId` int(5) NOT NULL AUTO_INCREMENT,
  `hostId` varchar(40) DEFAULT NULL,
  `providerName` varchar(40) DEFAULT NULL,
  `providerGuid` varchar(40) DEFAULT NULL,
  `providerType` varchar(15) DEFAULT NULL,
  `useDefault` tinyint(1) DEFAULT NULL,
  PRIMARY KEY (`hostVssProviderDetailsId`),
  KEY `hostId` (`hostId`),
  CONSTRAINT `hosts_hostVssProviderDetails` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `hostXsResourcePools`
--

CREATE TABLE `hostXsResourcePools` (
  `hostId` varchar(40) NOT NULL,
  `poolId` varchar(40) NOT NULL,
  PRIMARY KEY (`hostId`),
  KEY `hostXsResourcePools_poolid_index` (`poolId`),
  CONSTRAINT `xsResourcePools_hostXsResourcePools` FOREIGN KEY (`poolId`) REFERENCES `xsResourcePools` (`poolId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_hostXsResourcePools` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `hosts`
--

CREATE TABLE `hosts` (
  `id` varchar(40) NOT NULL DEFAULT '',
  `name` varchar(255) NOT NULL DEFAULT '',
  `ipaddress` varchar(320) NOT NULL DEFAULT '',
  `sentinelEnabled` tinyint(1) NOT NULL DEFAULT '0',
  `outpostAgentEnabled` tinyint(1) NOT NULL DEFAULT '0',
  `filereplicationAgentEnabled` tinyint(1) NOT NULL DEFAULT '0',
  `sentinel_version` varchar(64) NOT NULL DEFAULT '',
  `outpost_version` varchar(64) NOT NULL DEFAULT '',
  `fr_version` varchar(64) NOT NULL DEFAULT '',
  `involflt_version` varchar(64) NOT NULL DEFAULT '',
  `osType` mediumtext NOT NULL,
  `vxTimeout` bigint(20) NOT NULL DEFAULT '0',
  `permissionState` int(4) NOT NULL DEFAULT '0',
  `lastHostUpdateTime` bigint(20) DEFAULT '0',
  `vxAgentPath` varchar(255) DEFAULT NULL,
  `fxAgentPath` varchar(255) DEFAULT NULL,
  `compatibilityNo` bigint(20) DEFAULT '0',
  `usecxnatip` tinyint(1) NOT NULL,
  `cx_natip` varchar(16) NOT NULL,
  `osFlag` tinyint(1) NOT NULL DEFAULT '1',
  `lastHostUpdateTimeFx` datetime NOT NULL,
  `sshPort` int(6) DEFAULT '0',
  `sshVersion` varchar(100) DEFAULT 'Unknown',
  `sshUser` varchar(100) DEFAULT 'Unknown',
  `fos_version` varchar(64) NOT NULL,
  `type` varchar(8) NOT NULL COMMENT 'Switch or Host',
  `hardware_configuration` text NOT NULL,
  `extended_version` varchar(64) NOT NULL,
  `time_zone` varchar(10) NOT NULL,
  `PatchHistoryVX` text,
  `PatchHistoryFX` text,
  `agentTimeStamp` timestamp NULL DEFAULT NULL,
  `agentTimeZone` varchar(5) DEFAULT NULL,
  `InVolCapacity` bigint(10) DEFAULT NULL,
  `InVolFreeSpace` bigint(10) DEFAULT NULL,
  `SysVolPath` varchar(255) DEFAULT NULL,
  `SysVolCap` bigint(10) DEFAULT NULL,
  `SysVolFreeSpace` bigint(10) DEFAULT NULL,
  `agent_state` bigint(32) DEFAULT '0' COMMENT 'it will set UNINSTALL_PENDING in case of unregister fabric Vx.',
  `alias` varchar(50) DEFAULT NULL,
  `usepsnatip` tinyint(1) NOT NULL DEFAULT '0',
  `processServerEnabled` tinyint(1) DEFAULT NULL,
  `prod_version` varchar(50) DEFAULT NULL,
  `domainName` varchar(25) DEFAULT NULL,
  `lastHostUpdateTimePs` datetime DEFAULT NULL,
  `lastHostUpdateTimeApp` datetime DEFAULT NULL,
  `hypervisorState` int(20) NOT NULL DEFAULT '0',
  `hypervisorName` varchar(255) DEFAULT NULL,
  `endianess` varchar(255) DEFAULT NULL,
  `processorArchitecture` varchar(255) DEFAULT NULL,
  `cxInfo` text,
  `psVersion` text,
  `psInstallTime` text,
  `psPatchVersion` text,
  `patchInstallTime` text,
  `accountId` varchar(40) NOT NULL DEFAULT '',
  `accountType` varchar(255) NOT NULL DEFAULT '',
  `csEnabled` tinyint(1) DEFAULT NULL,
  `appAgentEnabled` tinyint(1) NOT NULL DEFAULT '0',
  `appAgentVersion` text,
  `PatchHistoryAppAgent` text,
  `osBuild` varchar(255) DEFAULT NULL,
  `majorVersion` varchar(255) DEFAULT NULL,
  `minorVersion` varchar(255) DEFAULT NULL,
  `memory` bigint(20) DEFAULT NULL,
  `osCaption` varchar(255) DEFAULT NULL,
  `appAgentPath` varchar(255) DEFAULT NULL,
  `refreshStatus` int(11) DEFAULT '0',
  `systemType` varchar(50) DEFAULT NULL,
  `latestMBRFileName` varchar(40) DEFAULT NULL,
  `agentRole` varchar(20) DEFAULT NULL,
  `resourceId` varchar(40) NOT NULL,
  `originalHost` enum('0','1') DEFAULT '0',
  `originalVMId` varchar(255) DEFAULT '00000000-0000-0000-0000-000000000000',
  `creationTime` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `TargetDataPlane` varchar(255) NOT NULL DEFAULT 'INMAGE_DATA_PLANE' COMMENT 'can be one of  UNSUPPORTED_DATA_PLANE = 0,  INMAGE_DATA_PLANE = 1,  AZURE_DATA_PLANE = 2',
  `systemDrive` varchar(255),
  `systemDirectory`  text,
  `systemDriveDiskExtents` text,
  `certExpiryDetails` text,
  `publicIp` varchar(320) DEFAULT NULL,
  `biosId` varchar(70) DEFAULT NULL,
  `marsVersion` varchar(255) DEFAULT NULL,
  `FirmwareType` varchar(255) NOT NULL DEFAULT '',
  `AgentBiosId` varchar(255) NOT NULL DEFAULT '',
  `AgentFqdn` text,
  `isGivenToMigration` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Migrated=1, NotMigrated=0',
  PRIMARY KEY (`id`),
  KEY `name` (`name`),
  KEY `hosts_ipaddress` (`ipaddress`),
  KEY `hosts_sentinelEnabled` (`sentinelEnabled`),
  KEY `hosts_outpostAgentEnabled` (`outpostAgentEnabled`),
  KEY `hosts_filereplicationAgentEnabled` (`filereplicationAgentEnabled`),
  KEY `hosts_osFlag` (`osFlag`),
  KEY `hosts_usecxnatip` (`usecxnatip`),
  KEY `hosts_type` (`type`),
  KEY `hosts_processServerEnabled` (`processServerEnabled`),
  KEY `hosts_accountId` (`accountId`),
  KEY `hosts_accountType` (`accountType`),
  KEY `hosts_publicIp` (`publicIp`),
  KEY `hosts_csEnabled` (`csEnabled`),
  KEY `hosts_agentrole` (`agentRole`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `infrastructurehealthfactors`
--

CREATE TABLE `infrastructurehealthfactors` (
  `hostId` varchar(40) NOT NULL DEFAULT '',
  `healthFactorCode` varchar(100) NOT NULL,
  `healthFactor` tinyint(1) NOT NULL DEFAULT '2' COMMENT 'Warning=1, Critical=2',
  `priority` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'This is to know priority of the health factor between the health factors warning, critical for the same protection.',
  `component` varchar(100) NOT NULL,
  `healthDescription` text NOT NULL,
  `healthCreationTime` datetime DEFAULT '0000-00-00 00:00:00',
  `healthUpdateTime` datetime DEFAULT '0000-00-00 00:00:00',
  `placeHolders` text NOT NULL,
  PRIMARY KEY (`hostId`,`healthFactorCode`),
  KEY `infrastructureHealthfactors_priority` (`priority`),
  KEY `infrastructureHealthfactors_component` (`component`),
  CONSTRAINT `hosts_infrastructureHealthfactors_hostId` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC ;

--
-- Table structure for table `infrastructureHosts`
--

CREATE TABLE `infrastructureHosts` (
  `infrastructureHostId` int(11) NOT NULL AUTO_INCREMENT,
  `inventoryId` int(11) NOT NULL,
  `hostIp` varchar(255) NOT NULL DEFAULT '',
  `hostName` varchar(60) NOT NULL DEFAULT '',
  `organization` varchar(60) NOT NULL DEFAULT '' COMMENT 'stores organisation name in case of vCenter or the organization name in case of vCloud',
  `datastoreDetails` longtext NOT NULL,
  `resourcepoolDetails` longtext NOT NULL,
  `networkDetails` longtext NOT NULL,
  `configDetails` longtext NOT NULL,
  `location` varchar(60) NOT NULL DEFAULT '',
  PRIMARY KEY (`infrastructureHostId`),
  KEY `indx_hostIp` (`hostIp`),
  KEY `infrastructureInventory_Hosts` (`inventoryId`,`hostIp`),
  CONSTRAINT `InfrastructureInventory_Hosts` FOREIGN KEY (`inventoryId`) REFERENCES `infrastructureInventory` (`inventoryId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `infrastructureInventory`
--

CREATE TABLE `infrastructureInventory` (
  `inventoryId` int(11) NOT NULL AUTO_INCREMENT,
  `siteName` varchar(20) NOT NULL,
  `hostIp` varchar(255) NOT NULL DEFAULT '',
  `hostPort` int(6) NOT NULL DEFAULT '443',
  `infrastructureType` enum('Primary','Secondary') DEFAULT 'Primary',
  `hostType` enum('Azure','AWS','CenturyLink','HyperV','vCloud','vCenter','vSphere','Physical') DEFAULT 'vSphere',
  `login` varchar(60) NOT NULL DEFAULT '',
  `passwd` varchar(160) NOT NULL DEFAULT '',
  `certificate` blob DEFAULT NULL,
  `organization` varchar(60) NOT NULL DEFAULT '',
  `autoDiscovery` enum('0','1') DEFAULT '1',
  `discoveryInterval` smallint(6) NOT NULL DEFAULT '900',
  `discoverNow` enum('0','1') DEFAULT '0',
  `lastDiscoverySuccessTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `lastDiscoveryErrorMessage` text,
  `lastDiscoveryErrorLog` text,
  `autoLoginVerify` enum('0','1') DEFAULT '1',
  `loginVerifyTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `loginVerifyStatus` enum('0','1') DEFAULT '0',
  `connectionStatus` enum('0','1') DEFAULT '0',
  `loginVerifyLog` varchar(255) DEFAULT NULL,
  `creationTime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `esxAgentId` varchar(40) NOT NULL DEFAULT '',
  `rxInventoryId` int(11) NOT NULL DEFAULT '0',
  `dnsServers` longtext,
  `localNetworkSites` longtext,
  `virtualNetworkSites` longtext,
  `storageInfo` longtext,
  `subscriptionInfo` text,
  `errCode` varchar(50) NOT NULL default '',   
  `errPlaceHolders` text,
  `accountId` int(11) DEFAULT NULL,
  `additionalDetails` longtext,
  PRIMARY KEY (`inventoryId`),
  KEY `indx_siteName` (`siteName`,`infrastructureType`,`hostType`),
  KEY `indx_hostIp` (`hostIp`,`organization`),
  KEY `indx_discovernow` (`discoverNow`),
  KEY `indx_infrastructure` (`infrastructureType`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `infrastructureVms`
--

CREATE TABLE `infrastructureVMs` (
  `infrastructureVMId` varchar(255) NOT NULL,
  `infrastructureHostId` int(11) NOT NULL,
  `hostIp` varchar(100) NOT NULL DEFAULT '',
  `instanceUuid` varchar(100) NOT NULL DEFAULT '',
  `hostUuid` varchar(255) NOT NULL DEFAULT '',
  `hostName` varchar(255) NOT NULL DEFAULT '',
  `publicIp` varchar(16) NOT NULL DEFAULT '',
  `publicPort` varchar(10) NOT NULL DEFAULT '',
  `protocol` varchar(10) NOT NULL DEFAULT '',
  `ipList` text NOT NULL,
  `replicaId` varchar(255) NOT NULL DEFAULT '',
  `macIpAddressList` text NOT NULL,
  `hostDetails` longtext NOT NULL COMMENT 'stores hash array with keys as host, nic and disk. nic and disk can have multiple hash arrays',  
  `diskCount` int(4) NOT NULL DEFAULT 0,
  `diskDetails` longtext,
  `displayName` varchar(255) NOT NULL DEFAULT '',
  `OS` enum('WINDOWS','LINUX','SOLARIS','OTHER') NOT NULL DEFAULT 'WINDOWS' COMMENT 'WINDOWS, LINUX, SOLARIS, OTHER',
  `isVMPoweredOn` enum('0','1') NOT NULL DEFAULT '0',
  `isVMProtected` enum('0','1') NOT NULL DEFAULT '0',
  `hostId` varchar(40) NOT NULL DEFAULT '',
  `vmSize` varchar(30) NOT NULL DEFAULT '',
  `isTemplate` enum('0','1') NOT NULL DEFAULT '0',
  `isVmDeleted` enum('0','1') NOT NULL DEFAULT '0',
  `validationErrors` text,
  `macAddressList` text NOT NULL,
  `isPersonaChanged` enum('0','1') NOT NULL DEFAULT '0',
  `discoveryType` varchar(255) NOT NULL DEFAULT 'ON-PREMISE',
  `createdTime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`infrastructureVMId`),
  KEY `infrastructureHosts_VMs` (`infrastructureHostId`),
  KEY `indx_ip_host_displayname` (`hostIp`,`hostName`,`displayName`),
  KEY `indx_os` (`OS`),
  KEY `indx_vmprotect` (`isVMProtected`),
  KEY `indx_host_id` (`hostId`),
  KEY `indx_discoveryType` (`discoveryType`),
  CONSTRAINT `infrastructureHosts_infrastructureVMs` FOREIGN KEY (`infrastructureHostId`) REFERENCES `infrastructureHosts` (`infrastructureHostId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `ipType`
--

CREATE TABLE `ipType` (
  `id` tinyint(3) NOT NULL AUTO_INCREMENT,
  `type` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `ldapServer`
--

CREATE TABLE `ldapServer` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `serverAddr` varchar(255) NOT NULL,
  `isDefault` enum('Y','N') NOT NULL DEFAULT 'N',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `logRotationList`
--

CREATE TABLE `logRotationList` (
  `id` int(20) NOT NULL AUTO_INCREMENT,
  `logName` varchar(255) DEFAULT '',
  `logPathLinux` varchar(90) DEFAULT 'NULL',
  `logPathWindows` varchar(90) DEFAULT 'NULL',
  `logType` tinyint(2) NOT NULL DEFAULT '0',
  `logSizeLimit` int(40) NOT NULL DEFAULT '10',
  `logTimeLimit` int(30) NOT NULL DEFAULT '0',
  `startTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `logicalVolumes`
--

CREATE TABLE `logicalVolumes` (
  `hostId` varchar(40) NOT NULL DEFAULT '',
  `deviceName` varchar(255) NOT NULL DEFAULT '',
  `fileSystemType` varchar(255) NOT NULL DEFAULT '',
  `capacity` bigint(20) NOT NULL DEFAULT '0',
  `systemVolume` tinyint(1) NOT NULL DEFAULT '0',
  `bootVolume` tinyint(1) NOT NULL DEFAULT '0',
  `cacheVolume` tinyint(1) NOT NULL DEFAULT '0',
  `lastSentinelChange` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `lastOutpostAgentChange` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `lastDeviceUpdateTime` bigint(20) NOT NULL DEFAULT '0',
  `dpsId` varchar(40) NOT NULL DEFAULT '',
  `farLineProtected` tinyint(1) NOT NULL DEFAULT '0',
  `returnHomeEnabled` tinyint(1) NOT NULL DEFAULT '0',
  `doResync` tinyint(1) NOT NULL DEFAULT '0',
  `startingPhysicalOffset` bigint(20) NOT NULL DEFAULT '0',
  `tmId` int(11) NOT NULL DEFAULT '0',
  `visible` tinyint(1) NOT NULL DEFAULT '0',
  `deviceLocked` tinyint(1) NOT NULL DEFAULT '0',
  `deviceFlagInUse` tinyint(3) DEFAULT '0',
  `freeSpace` bigint(20) DEFAULT '0',
  `readwritemode` tinyint(1) DEFAULT '0',
  `preDriveState` tinyint(1) DEFAULT '0',
  `isVisible` tinyint(1) NOT NULL DEFAULT '0',
  `volumeType` tinyint(1) DEFAULT '0',
  `transProtocol` tinyint(1) DEFAULT '1',
  `volumeLabel` varchar(255) DEFAULT '',
  `maxDiffFilesThreshold` bigint(20) DEFAULT NULL,
  `mountPoint` text NOT NULL,
  `Phy_Lunid` varchar(255) DEFAULT NULL,
  `lun_state` int(5) DEFAULT NULL COMMENT 'can be one of  START_PROTECTION_PENDING = 1,  PROTECTED = 2,  STOP_PROTECTION_PENDING = 3 or < 0 for error',
  `offset` int(20) NOT NULL,
  `deviceId` varchar(255) DEFAULT NULL,
  `isUsedForPaging` int(5) DEFAULT '0',
  `rawSize` bigint(20) NOT NULL DEFAULT '0',
  `writecacheEnabled` int(5) DEFAULT '0',
  `isMultipath` varchar(40) DEFAULT NULL,
  `vendorOrigin` varchar(40) DEFAULT NULL,
  `formatLabel` tinyint(1) DEFAULT '0',
  `volumeSolution` varchar(40) DEFAULT NULL,
  `dataSizeOnDisk` bigint(20) DEFAULT '0',
  `diskSpaceSavedByThinprovision` bigint(20) DEFAULT '0',
  `diskSpaceSavedByCompression` bigint(20) DEFAULT '0',
  `deviceProperties` text,
  `isProtectable` enum('0','1') DEFAULT '1',
  `isImpersonate` tinyint(1) DEFAULT '0',
  KEY `hostId` (`hostId`),
  KEY `capacity` (`capacity`),
  KEY `mountPoint` (`mountPoint`(500)),
  KEY `device_id_indx` (`deviceId`),
  KEY `isusedforpaging_indx` (`isUsedForPaging`),
  KEY `rawsize_indx` (`rawSize`),
  KEY `writecacheenabled_indx` (`writecacheEnabled`),
  KEY `volumetype_indx` (`volumeType`),
  KEY `logicalVolumes_systemVolume` (`systemVolume`),
  KEY `logicalVolumes_bootVolume` (`bootVolume`),
  KEY `logicalVolumes_cacheVolume` (`cacheVolume`),
  KEY `logicalVolumes_tmId` (`tmId`),
  KEY `logicalVolumes_visible` (`visible`),
  KEY `logicalVolumes_deviceLocked` (`deviceLocked`),
  KEY `logicalVolumes_deviceFlagInUse` (`deviceFlagInUse`),
  KEY `logicalVolumes_farLineProtected` (`farLineProtected`),
  KEY `logicalVolumes_readwritemode` (`readwritemode`),
  KEY `logicalVolumes_isVisible` (`isVisible`),
  KEY `logicalVolumes_isMultipath` (`isMultipath`),
  KEY `logicalVolumes_transProtocol` (`transProtocol`),
  KEY `logicalVolumes_Phy_Lunid` (`Phy_Lunid`),
  CONSTRAINT `hosts_logicalVolumes` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `managedLuns`
--
CREATE TABLE `managedLuns` (
  `sanLunId` varchar(32) NOT NULL,
  `arrayGuid` varchar(40) NOT NULL,
  `processServerId` varchar(40) NOT NULL,
  `fsType` varchar(40) NOT NULL,
  `mountPoint` varchar(256) NOT NULL,
  `lunType` enum('RETENTION','CXCACHE','GENERAL') DEFAULT NULL,
  `state` bigint(64) DEFAULT NULL COMMENT 'MAP_DISCOVERY_PENDING = 300; MAP_DISCOVERY_DONE = 301;',
  PRIMARY KEY (`sanLunId`,`arrayGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `networkAddress`
--
CREATE TABLE `networkAddress` (
  `hostId` varchar(40) NOT NULL DEFAULT '',
  `deviceName` varchar(255) NOT NULL DEFAULT '',
  `ipAddress` varchar(16) NOT NULL DEFAULT '',
  `natIpAddress` varchar(16) DEFAULT NULL,
  `deviceType` tinyint(1) NOT NULL DEFAULT '0',
  `nicSpeed` varchar(30) DEFAULT NULL,
  `macAddress` varchar(100) DEFAULT NULL,
  `detail` text,
  `netmask` varchar(255) DEFAULT NULL,
  `gateway` varchar(255) DEFAULT NULL,
  `bcast` varchar(16) DEFAULT NULL,
  `dns` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`deviceName`,`hostId`,`deviceType`,`ipAddress`),
  CONSTRAINT `hosts_networkAddress` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `networkAddressMap`
--

CREATE TABLE `networkAddressMap` (
  `mapId` int(11) NOT NULL AUTO_INCREMENT,
  `vxAgentId` varchar(40) NOT NULL,
  `processServerId` varchar(40) NOT NULL,
  `nicDeviceName` varchar(255) NOT NULL,
  `status` ENUM('online','offline','unconfigured'),
  `dns` VARCHAR(50) DEFAULT NULL,
  `nicMTU` VARCHAR(50) DEFAULT NULL,
  PRIMARY KEY (`mapId`),
  CONSTRAINT `hosts_networkAddressMap_vxAgentId` FOREIGN KEY (`vxAgentId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_networkAddressMap_processServerId` FOREIGN KEY (`processServerId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `nextApplianceLunIdLunNumber`
--
CREATE TABLE `nextApplianceLunIdLunNumber` (
  `nextLunId` bigint(32) NOT NULL,
  `nextLunNumber` bigint(32) NOT NULL,
  `lunIdPrefix` varchar(40) NOT NULL DEFAULT 'inmage'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='This table is used for generating unique applianceLunIds and';


--
-- Table structure for table `nports`
--

CREATE TABLE `nports` (
  `portWwn` varchar(255) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `nodeWwn` varchar(32) NOT NULL,
  `symbolicName` varchar(128) NOT NULL,
  `hostId` varchar(256) DEFAULT NULL,
  `hostLabel` varchar(256) DEFAULT NULL,
  `portLabel` varchar(255) DEFAULT NULL,
  `portType` varchar(50) DEFAULT 'FC',
  `ipAddress` varchar(16) NOT NULL DEFAULT '0.0.0.0',
  `portNo` varchar(255) DEFAULT NULL,
  `iqn` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`portWwn`,`fabricId`,`ipAddress`),
  KEY `sanFabrics_nports` (`fabricId`),
  CONSTRAINT `sanFabrics_nports` FOREIGN KEY (`fabricId`) REFERENCES `sanFabrics` (`fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='General HBA information';

--
-- Table structure for table `oracleClusterDevices`
--

CREATE TABLE `oracleClusterDevices` (
  `hostId` varchar(40) NOT NULL,
  `deviceName` varchar(255) NOT NULL,
  `deviceType` varchar(40) NOT NULL,
  PRIMARY KEY (`hostId`,`deviceName`,`deviceType`),
  CONSTRAINT `hosts_oracleClusterDevices` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;
--
-- Table structure for table `oracleClusters`
--

CREATE TABLE `oracleClusters` (
  `oracleClusterId` int(11) NOT NULL AUTO_INCREMENT,
  `clusterName` varchar(40) DEFAULT NULL,
  `isCluster` int(2) DEFAULT NULL,
  PRIMARY KEY (`oracleClusterId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;
--
-- Table structure for table `oracleDatabaseDeviceViews`
--

CREATE TABLE `oracleDatabaseDeviceViews` (
  `oracleDatabaseInstanceId` int(11) DEFAULT NULL,
  `viewLevel` varchar(40) DEFAULT NULL,
  `parentName` varchar(255) DEFAULT NULL,
  `viewLevelName` varchar(255) DEFAULT NULL,
  `viewLevelType` varchar(40) DEFAULT NULL,
  `leftValue` int(10) DEFAULT NULL,
  `rightValue` int(10) DEFAULT NULL,
  KEY `oracleDatabaseInstances_oracleDatabaseDeviceViews` (`oracleDatabaseInstanceId`),
  CONSTRAINT `oracleDatabaseInstances_oracleDatabaseDeviceViews` FOREIGN KEY (`oracleDatabaseInstanceId`) REFERENCES `oracleDatabaseInstances` (`oracleDatabaseInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `oracleDatabaseFailoverDetails`
--

CREATE TABLE `oracleDatabaseFailoverDetails` (
  `hostId` varchar(40) NOT NULL,
  `applicationInstanceName` varchar(40) NOT NULL DEFAULT '',
  `fileName` varchar(40) NOT NULL DEFAULT '',
  `fileContents` text,
  `fileType` varchar(40) NOT NULL,
  PRIMARY KEY (`hostId`,`applicationInstanceName`,`fileName`),
  CONSTRAINT `hosts_oracleDatabaseFailoverDetails` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `oracleDatabaseInstances`
--

CREATE TABLE `oracleDatabaseInstances` (
  `oracleDatabaseInstanceId` int(11) NOT NULL AUTO_INCREMENT,
  `oracleClusterId` int(11) DEFAULT NULL,
  `instanceName` varchar(40) DEFAULT NULL,
  `instanceVersion` varchar(40) DEFAULT NULL,
  `recoveryLogEnabled` int(11) NOT NULL,
  PRIMARY KEY (`oracleDatabaseInstanceId`),
  KEY `oracleClusters_oracleDatabaseInstances` (`oracleClusterId`),
  CONSTRAINT `oracleClusters_oracleDatabaseInstances` FOREIGN KEY (`oracleClusterId`) REFERENCES `oracleClusters` (`oracleClusterId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `oracleDevices`
--

CREATE TABLE `oracleDevices` (
  `applicationInstanceId` int(11) DEFAULT NULL,
  `oracleDatabaseInstanceId` int(11) DEFAULT NULL,
  `srcDeviceName` varchar(255) DEFAULT NULL,
  `sharedDeviceId` varchar(128) NOT NULL,
  `volumeType` tinyint(1) DEFAULT '0',
  `vendorOrigin` varchar(40) DEFAULT NULL,
  KEY `oracleDevices_sharedDeviceId` (`sharedDeviceId`),
  KEY `applicationHosts_oracleDevices` (`applicationInstanceId`),
  KEY `oracleDatabaseInstances_oracleDevices` (`oracleDatabaseInstanceId`),
  CONSTRAINT `oracleDatabaseInstances_oracleDevices` FOREIGN KEY (`oracleDatabaseInstanceId`) REFERENCES `oracleDatabaseInstances` (`oracleDatabaseInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `applicationHosts_oracleDevices` FOREIGN KEY (`applicationInstanceId`) REFERENCES `applicationHosts` (`applicationInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `oracleNodes`
--

CREATE TABLE `oracleNodes` (
  `oracleClusterId` int(11) DEFAULT NULL,
  `hostId` varchar(40) DEFAULT NULL,
  `nodeName` varchar(40) DEFAULT NULL,
  KEY `oracleClusters_oracleNodes` (`oracleClusterId`),
  CONSTRAINT `oracleClusters_oracleNodes` FOREIGN KEY (`oracleClusterId`) REFERENCES `oracleClusters` (`oracleClusterId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `partitions`
--

CREATE TABLE `partitions` (
  `partitionId` varchar(255) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `diskId` varchar(255) NOT NULL,
  `partitionName` varchar(255) NOT NULL,
  `freeSpace` bigint(20) unsigned NOT NULL,
  `capacity` bigint(20) unsigned NOT NULL,
  `fileSystem` varchar(255) DEFAULT NULL,
  `isPartitionAtBlockZero` tinyint(3) unsigned DEFAULT NULL,
  KEY `partitionId_indx` (`partitionId`),
  KEY `hostId_indx` (`hostId`),
  KEY `diskId_indx` (`diskId`),
  CONSTRAINT `hosts_partitions` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `patchDetails`
--

CREATE TABLE `patchDetails` (
  `patchFilename` varchar(150) NOT NULL DEFAULT '',
  `processServerId` varchar(40) NOT NULL DEFAULT '',
  `rebootRequired` tinyint(1) DEFAULT NULL,
  `criticality` varchar(20) NOT NULL DEFAULT '',
  `updateType` enum('Update', 'Upgrade') NOT NULL DEFAULT 'Update',
  `patchVersion` varchar(15) NOT NULL DEFAULT '',
  `dependency` varchar(255) NOT NULL DEFAULT '',
  `osType` varchar(20) NOT NULL DEFAULT '',
  `description` text,
  `targetOs` text,
  PRIMARY KEY (`patchFilename`, `processServerId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `physicalDisks`
--

CREATE TABLE `physicalDisks` (
  `diskId` varchar(255) NOT NULL,
  `hostId` varchar(40) NOT NULL,
  `diskName` varchar(255) NOT NULL,
  `diskVendor` varchar(255) DEFAULT NULL,
  `fileSystem` varchar(255) DEFAULT NULL,
  KEY `hostId_indx` (`hostId`),
  KEY `diskId_indx` (`diskId`),
  CONSTRAINT `hosts_physicalDisks` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `planHealthSettings`
--

CREATE TABLE `planHealthSettings` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `planId` int(11) DEFAULT NULL,
  `planName` varchar(255) DEFAULT NULL,
  `rpoHealth` varchar(20) DEFAULT NULL,
  `dataFlowHealth` varchar(20) DEFAULT NULL,
  `dataUploadHealth` varchar(20) DEFAULT NULL,
  `noCommunication` varchar(20) DEFAULT NULL,
  `targetCacheHealth` varchar(20) DEFAULT NULL,
  `pauseHealth` varchar(20) DEFAULT NULL,
  `dataConsistencyHealth` varchar(20) DEFAULT NULL,
  `retentionHealth` varchar(20) DEFAULT NULL,
  `commonConsistencyHealth` varchar(20) DEFAULT NULL,
  `applicationProtectionHealth` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `planId` (`planId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `policy`
--

CREATE TABLE `policy` (
  `policyId` int(11) NOT NULL AUTO_INCREMENT,
  `policyType` tinyint(4) DEFAULT NULL,
  `policyName` varchar(256) DEFAULT NULL,
  `policyParameters` text,
  `policyScheduledType` tinyint(4) DEFAULT NULL COMMENT '1 - Run Now 2 - Run Every 0 - Not Scheduled',
  `policyRunEvery` varchar(40) DEFAULT NULL,
  `nextScheduledTime` datetime NOT NULL,
  `policyExcludeFrom` varchar(40) DEFAULT NULL,
  `policyExcludeTo` varchar(40) DEFAULT NULL,
  `applicationInstanceId` int(5) DEFAULT NULL,
  `hostId` text,
  `scenarioId` int(11) DEFAULT NULL,
  `dependentId` int(11) DEFAULT NULL,
  `policyTimestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `planId` int(11) DEFAULT NULL,
  PRIMARY KEY (`policyId`),
  KEY `policyType` (`policyType`),
  KEY `scenarioId` (`scenarioId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `policyInstance`
--

CREATE TABLE `policyInstance` (
  `policyInstanceId` int(5) NOT NULL AUTO_INCREMENT,
  `policyId` int(5) DEFAULT NULL,
  `lastUpdatedTime` datetime DEFAULT NULL,
  `policyState` tinyint(4) DEFAULT NULL,
  `executionLog` LONGTEXT,
  `outputLog` text,
  `policyStatus` enum('Active','Inactive') DEFAULT 'Active',
  `uniqueId` bigint(20) DEFAULT '0',
  `dependentInstanceId` int(11) DEFAULT '0',
  `hostId` varchar(256) DEFAULT NULL,
  PRIMARY KEY (`policyInstanceId`),
  KEY `uniqueId` (`uniqueId`),
  KEY `policyId` (`policyId`),
  KEY `policyState` (`policyState`),
  KEY `hostId` (`hostId`),
  CONSTRAINT `policy_policyInstance` FOREIGN KEY (`policyId`) REFERENCES `policy` (`policyId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `policyViolationEvents`
--
CREATE TABLE `policyViolationEvents` (
  `sourceHostId` varchar(40) NOT NULL DEFAULT '',
  `sourceDeviceName` varchar(255) NOT NULL DEFAULT '',
  `destinationHostId` varchar(40) NOT NULL DEFAULT '',
  `destinationDeviceName` varchar(255) NOT NULL DEFAULT '',
  `eventDate` date NOT NULL DEFAULT '0000-00-00',
  `startTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `duration` bigint(20) NOT NULL DEFAULT '0',
  `numOccurrence` int(11) NOT NULL DEFAULT '0',
  `eventType` enum('rpo','resync','throttle') NOT NULL DEFAULT 'rpo',
  KEY `sourceDeviceName` (`sourceDeviceName`),
  KEY `destinationHostId` (`destinationHostId`),
  KEY `destinationDeviceName` (`destinationDeviceName`),
  KEY `startTime` (`startTime`),
  KEY `eventDate` (`eventDate`),
  KEY `sourceHostId` (`sourceHostId`),
  KEY `policyViolationEvents_eventType` (`eventType`),
  CONSTRAINT `hosts_policyViolationEvents_sourceHostId` FOREIGN KEY (`sourceHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_policyViolationEvents_destinationHostId` FOREIGN KEY (`destinationHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `primarySecondaryHdsStorageArray`
--

CREATE TABLE `primarySecondaryHdsStorageArray` (
  `primarySubsystemId` varchar(40) NOT NULL,
  `secondarySubsystemId` varchar(40) NOT NULL,
  KEY `hdsStorageArray_primarySecondaryHdsStorageArrayprimary` (`primarySubsystemId`),
  KEY `hdsStorageArray_primarySecondaryHdsStorageArraysecondary` (`secondarySubsystemId`),
  CONSTRAINT `hdsStorageArray_primarySecondaryHdsStorageArraysecondary` FOREIGN KEY (`secondarySubsystemId`) REFERENCES `hdsStorageArray` (`subsystemId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hdsStorageArray_primarySecondaryHdsStorageArrayprimary` FOREIGN KEY (`primarySubsystemId`) REFERENCES `hdsStorageArray` (`subsystemId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `processServer`
--

CREATE TABLE `processServer` (
  `processServerId` varchar(40) NOT NULL,
  `ps_natip` varchar(16) NOT NULL,
  `cummulativeThrottle` tinyint(1) unsigned DEFAULT '0',
  `port` varchar(5) DEFAULT '9080',
  `sslPort` varchar(5) DEFAULT '9443',
  `ipaddress` varchar(16) DEFAULT NULL,
  `remoteTransportProtocolId` int(11) DEFAULT NULL,
  `localTransportProtocolId` int(11) DEFAULT NULL,
  `psCertExpiryDetails` text,
  PRIMARY KEY (`processServerId`),
  CONSTRAINT `hosts_processServer` FOREIGN KEY (`processServerId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `processServerDefaultTransportProtocol`
--

CREATE TABLE `processServerDefaultTransportProtocol` (
  `os` varchar(40) NOT NULL,
  `remoteTransportProtocolId` int(11) NOT NULL,
  `localTransportProtocolId` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `profilingDetails`
--

CREATE TABLE `profilingDetails` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `profilingId` bigint(20) DEFAULT '0',
  `cumulativeCompression` double DEFAULT '0',
  `cumulativeUnCompression` double DEFAULT '0',
  `forDate` varchar(15) DEFAULT '',
  `fromUpdate` tinyint(3) DEFAULT '0',
  `hourlyCumulativePeakCompression` double DEFAULT '0',
  `hourlyCumulativePeakUnCompression` double DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `profilingDetails_profilingId` (`profilingId`),
  KEY `profilingDetails_forDate` (`forDate`),
  KEY `profilingDetails_fromUpdate` (`fromUpdate`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `protectionTemplate`
--

CREATE TABLE `protectionTemplate` (
  `protectionTemplateId` int(11) NOT NULL AUTO_INCREMENT,
  `templateName` varchar(80) DEFAULT NULL,
  `configurationDetails` text NOT NULL COMMENT 'stores hash array with keys as processServer, compressionEnable etc',
  PRIMARY KEY (`protectionTemplateId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `pushProxyHosts`
--

CREATE TABLE `pushProxyHosts` (
  `id` varchar(40) NOT NULL,
  `name` varchar(255) NOT NULL,
  `osFlag` tinyint(1) NOT NULL,
  `ipaddress` varchar(16) NOT NULL,
  `isPushProxy` tinyint(1) DEFAULT '0',
  `lastHostUpdateTime` datetime DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `recoveryExecutionSteps`
--

CREATE TABLE `recoveryExecutionSteps` (
  `scenarioId` int(11) DEFAULT NULL,
  `policyId` int(11) DEFAULT NULL,
  `command` text,
  `executionStatus` int(11) DEFAULT NULL,
  `executionLog` longtext,
  `groupId` int(11) DEFAULT NULL,
  `policyInstanceId` int(11) DEFAULT '0',
  KEY `policyId` (`policyId`),
  KEY `scenarioId` (`scenarioId`),
  KEY `groupId` (`groupId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `recoveryStepsTemplate`
--
CREATE TABLE `recoveryStepsTemplate` (
  `stepId` varchar(50) NOT NULL,
  `process` varchar(100) DEFAULT NULL,
  `runAt` varchar(40) DEFAULT NULL,
  `groupId` int(11) DEFAULT NULL,
  `stepSeq` varchar(40) DEFAULT NULL,
  `applicationType` varchar(40) DEFAULT NULL,
  `recoveryType` varchar(40) DEFAULT NULL,
  PRIMARY KEY (`stepId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `replicationHistory`
--

CREATE TABLE `replicationHistory` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `ruleId` int(11) NOT NULL DEFAULT '0',
  `sourceHostId` varchar(40) NOT NULL DEFAULT '',
  `sourceDeviceName` varchar(255) NOT NULL DEFAULT '',
  `destinationHostId` varchar(40) NOT NULL DEFAULT '',
  `destinationDeviceName` varchar(255) NOT NULL DEFAULT '',
  `createdDate` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `deleted` tinyint(1) NOT NULL DEFAULT '0',
  `volumeGroupId` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  CONSTRAINT `hosts_replicationHistory` FOREIGN KEY (`sourceHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `repositoryDetails`
--
CREATE TABLE `repositoryDetails` (
  `repositoryId` int(40) NOT NULL AUTO_INCREMENT,
  `repositoryName` varchar(100) DEFAULT NULL,
  `volumeGroupName` varchar(100) DEFAULT NULL,
  `hostId` varchar(40) DEFAULT NULL,
  `policyId` int(11) DEFAULT NULL,
  `status` int(11) DEFAULT NULL,
  PRIMARY KEY (`repositoryId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `repositoryDeviceDetails`
--

CREATE TABLE `repositoryDeviceDetails` (
  `repositoryId` int(40) DEFAULT NULL,
  `deviceName` varchar(100) DEFAULT NULL,
  `role` varchar(40) DEFAULT NULL,
  `percentage` int(11) DEFAULT NULL,
  `status` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `repositoryProtection`
--

CREATE TABLE `repositoryProtection` (
  `hostId` varchar(40) DEFAULT NULL,
  `repositoryId` int(40) DEFAULT NULL,
  KEY `repositoryDetails_repositoryProtection` (`repositoryId`),
  CONSTRAINT `repositoryDetails_repositoryProtection` FOREIGN KEY (`repositoryId`) REFERENCES `repositoryDetails` (`repositoryId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `reservedPorts`
--

CREATE TABLE `reservedPorts` (
  `portWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `nodeWwn` varchar(32) DEFAULT NULL,
  `symbolicName` varchar(128) DEFAULT NULL,
  PRIMARY KEY (`portWwn`,`fabricId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `retError`
--

CREATE TABLE `retError` (
  `ErrId` bigint(11) NOT NULL AUTO_INCREMENT,
  `retErr` varchar(255) NOT NULL DEFAULT '',
  `retId` varchar(40) NOT NULL DEFAULT '',
  `ruleId` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`ErrId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `retLogHistory`
--

CREATE TABLE `retLogHistory` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `retId` bigint(20) DEFAULT NULL,
  `repId` bigint(20) DEFAULT NULL,
  `HostId` varchar(40) DEFAULT NULL,
  `logdatadir` varchar(255) NOT NULL DEFAULT '',
  `retLogsize` bigint(20) DEFAULT '0',
  `createdDate` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `modifiedDate` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `deleted` int(4) NOT NULL DEFAULT '0',
  `timebasedFlag` int(11) NOT NULL DEFAULT '0',
  `retTimeInterval` int(11) NOT NULL DEFAULT '0',
  `type_of_policy` tinyint(1) DEFAULT '0',
  `ret_logupto_days` bigint(15) DEFAULT '0',
  `ret_logupto_hrs` bigint(15) DEFAULT '0',
  `temp_logdata_dir` int(11) NOT NULL DEFAULT '0',
  `editPathFlag` int(11) NOT NULL DEFAULT '0',
  `diskspacethreshold` int(11) NOT NULL DEFAULT '80',
  `logsizethreshold` int(11) NOT NULL DEFAULT '80',
  `logPruningPolicy` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `retLogHistory_retId` (`retId`),
  CONSTRAINT `hosts_retLogHistory` FOREIGN KEY (`HostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `retLogPolicy`
--

CREATE TABLE `retLogPolicy` (
  `retId` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `ruleid` int(11) NOT NULL DEFAULT '0',
  `retPolicyType` tinyint(1) unsigned DEFAULT NULL,
  `metafilepath` varchar(255) NOT NULL DEFAULT '',
  `currLogsize` bigint(20) DEFAULT '0',
  `createdDate` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `modifiedDate` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `deleted` int(4) NOT NULL DEFAULT '0',
  `retState` tinyint(3) DEFAULT '0',
  `editPathFlag` int(11) NOT NULL DEFAULT '0',
  `diskspacethreshold` int(11) NOT NULL DEFAULT '80',
  `uniqueId` varchar(20) DEFAULT NULL,
  `sparseEnable` tinyint(1) DEFAULT '0',
  `startTimeStamp` bigint(20) DEFAULT '0',
  `endTimeStamp` bigint(20) DEFAULT '0',
  `spaceSavedBySparsePolicy` bigint(20) DEFAULT '0',
  `spaceSavedByCompression` bigint(20) DEFAULT '0',
  PRIMARY KEY (`retId`),
  KEY `ruleid` (`ruleid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `retentionEventPolicy`
--

CREATE TABLE `retentionEventPolicy` (
  `retentionEventPolicyId` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `Id` int(10) unsigned DEFAULT NULL,
  `retentionWindowId` int(10) unsigned DEFAULT NULL,
  `storagePath` text,
  `storageDeviceName` varchar(255) DEFAULT NULL,
  `storagePruningPolicy` tinyint(1) unsigned DEFAULT NULL,
  `consistencyTag` varchar(150) DEFAULT NULL,
  `alertThreshold` tinyint(2) unsigned DEFAULT NULL,
  `moveRetentionPath` text,
  `tagCount` int(11) DEFAULT '0',
  `userDefinedTag` varchar(255) DEFAULT NULL,
  `catalogPath` text,
  PRIMARY KEY (`retentionEventPolicyId`),
  KEY `retentionWindow_retentionEventPolicy` (`Id`),
  CONSTRAINT `retentionWindow_retentionEventPolicy` FOREIGN KEY (`Id`) REFERENCES `retentionWindow` (`Id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `retentionSpacePolicy`
--
CREATE TABLE `retentionSpacePolicy` (
  `retentionSpacePolicyId` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `retId` bigint(20) unsigned DEFAULT NULL,
  `storageSpace` bigint(20) unsigned DEFAULT NULL,
  `storagePath` text,
  `storageDeviceName` varchar(255) DEFAULT NULL,
  `storagePruningPolicy` tinyint(1) unsigned DEFAULT NULL,
  `alertThreshold` tinyint(2) unsigned DEFAULT NULL,
  `moveRetentionPath` text,
  `catalogPath` text,
  PRIMARY KEY (`retentionSpacePolicyId`),
  KEY `retLogPolicy_retentionSpacePolicy` (`retId`),
  CONSTRAINT `retLogPolicy_retentionSpacePolicy` FOREIGN KEY (`retId`) REFERENCES `retLogPolicy` (`retId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `retentionTag`
--
CREATE TABLE `retentionTag` (
`HostId` varchar(40) NOT NULL DEFAULT '',
`deviceName` varchar(255) DEFAULT NULL,
`tagTimeStamp` varchar(255) NOT NULL DEFAULT '',
`appName` varchar(255) NOT NULL DEFAULT '',
`userTag` text,
`ruleId` int(11) DEFAULT '0',
`paddedTagTimeStamp` varchar(255) DEFAULT '0',
`accuracy` tinyint(1) DEFAULT '0',
`tagGuid` varchar(40) NOT NULL DEFAULT '',
`comment` varchar(255) DEFAULT NULL,
`tagVerfiication` varchar(5) DEFAULT 'NO',
`tagTimeStampUTC` bigint(20) NOT NULL DEFAULT '0',
`pairId` int(11) NOT NULL DEFAULT '0',
PRIMARY KEY (`HostId`,`pairId`,`tagTimeStampUTC`,`appName`,`tagGuid`),
KEY `deviceName` (`deviceName`),
KEY `ruleId` (`ruleId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC partition by key(HostId);

--
-- Table structure for table `retentionTimeStamp`
--
CREATE TABLE `retentionTimeStamp` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `hostId` varchar(40) NOT NULL DEFAULT '',
  `deviceName` varchar(255) DEFAULT NULL,
  `StartTime` varchar(255) DEFAULT NULL,
  `EndTime` varchar(255) DEFAULT NULL,
  `StartTimeUTC` bigint(20) DEFAULT NULL,
  `EndTimeUTC` bigint(20) DEFAULT NULL,
  `ruleId` int(11) DEFAULT '0',
  `accuracy` tinyint(1) DEFAULT '0',
  `pairId` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`,`hostId`),
  UNIQUE KEY `pairid_starttimeutc_accuracy` (`hostId`,`pairId`,`StartTimeUTC`,`accuracy`),
  KEY `ruleId` (`ruleId`),
  KEY `EndTimeUTC` (`EndTimeUTC`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC partition by key(hostId);

--
-- Table structure for table `retentionWindow`
--
CREATE TABLE `retentionWindow` (
  `Id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `retId` bigint(20) unsigned NOT NULL,
  `retentionWindowId` int(10) unsigned NOT NULL,
  `startTime` bigint(20) unsigned DEFAULT NULL,
  `endTime` bigint(20) unsigned DEFAULT NULL,
  `timeGranularity` bigint(20) unsigned DEFAULT NULL,
  `timeIntervalUnit` varchar(20) DEFAULT NULL,
  `granularityUnit` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`Id`),
  KEY `retLogPolicy_retentionWindow` (`retId`),
  CONSTRAINT `retLogPolicy_retentionWindow` FOREIGN KEY (`retId`) REFERENCES `retLogPolicy` (`retId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `ruleTemplate`
--

CREATE TABLE `ruleTemplate` (
  `ruleId` int(11) NOT NULL DEFAULT '0',
  `ruleName` varchar(100) DEFAULT NULL,
  `ruleDescription` text,
  `ruleCategory` varchar(40) DEFAULT NULL COMMENT 'Category - Generic, Protection, failover, failback, Backup or data validations',
  `application` varchar(40) DEFAULT NULL COMMENT 'Applicability - system, applicationname, common to all applications',
  `runLevel` varchar(40) DEFAULT NULL COMMENT 'Run Level - At system, At application (LCR, CCR, SCR), At application metadata level( db on system driver, etc..,)',
  `runAt` varchar(40) DEFAULT NULL COMMENT 'Runnat - CX/Source/Target',
  `DrProtectionCriticalityTarget` varchar(100) DEFAULT NULL,
  `BackupProtectionCriticalityTarget` varchar(100) DEFAULT NULL,
  `DrFailoverCriticalityTarget` varchar(100) DEFAULT NULL,
  `dependency` varchar(100) DEFAULT NULL,
  `DrProtectionCriticalitySource` varchar(100) DEFAULT NULL,
  `BackupProtectionCriticalitySource` varchar(100) DEFAULT NULL,
  `DrFailoverCriticalitySource` varchar(100) DEFAULT NULL,
  PRIMARY KEY (`ruleId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `rules`
--

CREATE TABLE `rules` (
  `ruleId` int(11) NOT NULL DEFAULT '0',
  `rule` int(11) NOT NULL DEFAULT '0',
  `dayType` char(3) NOT NULL DEFAULT 'ALL',
  `perpetual` tinyint(1) NOT NULL DEFAULT '1',
  `startDate` date NOT NULL DEFAULT '0000-00-00',
  `endDate` date NOT NULL DEFAULT '0000-00-00',
  `singleDate` date NOT NULL DEFAULT '0000-00-00',
  `runType` char(6) NOT NULL DEFAULT 'ALLDAY',
  `startTime` time NOT NULL DEFAULT '00:00:00',
  `endTime` time NOT NULL DEFAULT '00:00:00',
  KEY `ruleId` (`ruleId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `rxSettings`
--
CREATE TABLE `rxSettings` (
  `ValueName` mediumtext NOT NULL,
  `ValueData` mediumtext NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `sanFabrics`
--

CREATE TABLE `sanFabrics` (
  `fabricId` varchar(40) NOT NULL COMMENT 'id of the fabric that this information is for.',
  `name` varchar(32) DEFAULT NULL,
  `zoneSetName` varchar(32) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`fabricId`),
  KEY `IDX_sanFabrics_1` (`fabricId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='holds general information about the san fabrics that are ava';


--
-- Table structure for table `sanITLNexus`
--

CREATE TABLE `sanITLNexus` (
  `sanInitiatorPortWwn` varchar(32) NOT NULL,
  `sanTargetPortWwn` varchar(32) NOT NULL,
  `sanLunId` varchar(128) NOT NULL,
  `sanLunNumber` bigint(64) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `replicationState` bigint(32) DEFAULT NULL,
  `deleteState` tinyint(8) DEFAULT NULL,
  `resyncRequired` tinyint(4) DEFAULT NULL,
  `applianceTargetPathState` tinyint(4) DEFAULT NULL,
  `applianceTargetPathStateChangeCount` bigint(32) DEFAULT NULL,
  `applianceTargetPathStateAckCount` bigint(32) DEFAULT NULL,
  `applianceTargetPathIoFailureCount` bigint(32) DEFAULT NULL,
  `applianceTargetPathIoFailureAckCount` bigint(32) DEFAULT NULL,
  `physicalTargetPathState` tinyint(4) DEFAULT NULL,
  `physicalTargetPathStatechangecount` bigint(32) DEFAULT NULL,
  `physicalTargetPathStateAckcount` bigint(32) DEFAULT NULL,
  `physicalTargetPathIoFailureCount` bigint(32) DEFAULT NULL,
  `physicalTargetPathIoFailureAckCount` bigint(32) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL COMMENT 'can be one of NO_WORK = 0,  DELETE_PENDING = 4,  NEW_ENTRY_PENDING = 17 or < 0 for error',
  PRIMARY KEY (`sanInitiatorPortWwn`,`sanTargetPortWwn`,`sanLunId`,`sanLunNumber`,`fabricId`),
  KEY `sanITNexus_sanITLNexus` (`sanInitiatorPortWwn`,`sanTargetPortWwn`,`fabricId`),
  KEY `sanLuns_sanITLNexus` (`sanLunId`),
  CONSTRAINT `sanLuns_sanITLNexus` FOREIGN KEY (`sanLunId`) REFERENCES `sanLuns` (`sanLunId`) ON UPDATE CASCADE,
  CONSTRAINT `sanITNexus_sanITLNexus` FOREIGN KEY (`sanInitiatorPortWwn`, `sanTargetPortWwn`, `fabricId`) REFERENCES `sanITNexus` (`sanInitiatorPortWwn`, `sanTargetPortWwn`, `fabricId`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='contains the san ITL nexus that can be protected';


--
-- Table structure for table `sanITNexus`
--
CREATE TABLE `sanITNexus` (
  `sanInitiatorPortWwn` varchar(32) NOT NULL,
  `sanTargetPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `switchAgentId` varchar(40) DEFAULT NULL,
  `rescanFlag` tinyint(1) DEFAULT NULL,
  `deleteFlag` tinyint(1) DEFAULT NULL,
  `frPolicy` varchar(40) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`sanInitiatorPortWwn`,`sanTargetPortWwn`,`fabricId`),
  CONSTRAINT `sanReportedITNexus_sanITNexus` FOREIGN KEY (`sanInitiatorPortWwn`, `sanTargetPortWwn`, `fabricId`) REFERENCES `sanReportedITNexus` (`sanInitiatorPortWwn`, `sanTargetPortWwn`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='conatins the san IT nexes after appling the filter rules. th';


--
-- Table structure for table `sanInitiators`
--
CREATE TABLE `sanInitiators` (
  `sanInitiatorPortWwn` varchar(255) NOT NULL COMMENT 'world wide name for this initiator. It is a key back to the hba ',
  `fabricId` varchar(40) NOT NULL,
  `ipAddress` varchar(16) NOT NULL DEFAULT '0.0.0.0',
  `deleteState` tinyint(8) DEFAULT NULL,
  PRIMARY KEY (`sanInitiatorPortWwn`,`fabricId`,`ipAddress`),
  CONSTRAINT `nports_sanInitiators` FOREIGN KEY (`sanInitiatorPortWwn`, `fabricId`) REFERENCES `nports` (`portWwn`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='san inititiators';


--
-- Table structure for table `sanLuns`
--

CREATE TABLE `sanLuns` (
  `sanLunId` varchar(128) NOT NULL,
  `capacity` bigint(20) DEFAULT NULL,
  `capacityChanged` tinyint(8) DEFAULT NULL,
  `blockSize` bigint(20) DEFAULT NULL,
  `vendorName` varchar(255) DEFAULT NULL,
  `modelNumber` varchar(255) DEFAULT NULL,
  `lunLabel` varchar(255) DEFAULT NULL,
  `resyncRequired` tinyint(2) DEFAULT '0',
  `exceptionState` tinyint(2) DEFAULT '0',
  PRIMARY KEY (`sanLunId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='general lun information';

--
-- Table structure for table `sanReportedITNexus`
--

CREATE TABLE `sanReportedITNexus` (
  `sanInitiatorPortWwn` varchar(32) NOT NULL,
  `sanTargetPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`sanInitiatorPortWwn`,`sanTargetPortWwn`,`fabricId`),
  KEY `sanInitiators_sanReportedITNexus` (`sanInitiatorPortWwn`,`fabricId`),
  KEY `sanTargets_sanReportedITNexus` (`sanTargetPortWwn`,`fabricId`),
  CONSTRAINT `sanTargets_sanReportedITNexus` FOREIGN KEY (`sanTargetPortWwn`, `fabricId`) REFERENCES `sanTargets` (`sanTargetPortWwn`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `sanInitiators_sanReportedITNexus` FOREIGN KEY (`sanInitiatorPortWwn`, `fabricId`) REFERENCES `sanInitiators` (`sanInitiatorPortWwn`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='contains all reported IT nexus ';


--
-- Table structure for table `sanSwitches`
--
CREATE TABLE `sanSwitches` (
  `switchIp` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `username` varchar(32) DEFAULT NULL,
  `password` varchar(32) DEFAULT NULL,
  `lastUpdatedTime` time DEFAULT NULL,
  `switchLookUpPreference` bigint(32) DEFAULT NULL,
  `errorMessage` varchar(40) DEFAULT NULL,
  `state` bigint(32) DEFAULT NULL,
  PRIMARY KEY (`switchIp`,`fabricId`),
  KEY `sanFabrics_sanSwitches` (`fabricId`),
  CONSTRAINT `sanFabrics_sanSwitches` FOREIGN KEY (`fabricId`) REFERENCES `sanFabrics` (`fabricId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `sanTargets`
--
CREATE TABLE `sanTargets` (
  `sanTargetPortWwn` varchar(255) NOT NULL COMMENT 'world wide name for this initiator. It is a key back to the hba ',
  `fabricId` varchar(40) NOT NULL,
  `ipAddress` varchar(16) NOT NULL DEFAULT '0.0.0.0',
  `deleteState` tinyint(8) DEFAULT NULL,
  PRIMARY KEY (`sanTargetPortWwn`,`fabricId`,`ipAddress`),
  CONSTRAINT `nports_sanTargets` FOREIGN KEY (`sanTargetPortWwn`, `fabricId`) REFERENCES `nports` (`portWwn`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC   COMMENT='san target';


--
-- Table structure for table `sharedDevices`
--
CREATE TABLE `sharedDevices` (
  `sharedDeviceId` varchar(128) NOT NULL,
  `capacity` bigint(20) DEFAULT NULL,
  `deviceLabel` varchar(255) DEFAULT NULL,
  `volumeResize` tinyint(5) DEFAULT '0',
  PRIMARY KEY (`sharedDeviceId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `snapSchedule`
--
CREATE TABLE `snapSchedule` (
  `Id` bigint(20) NOT NULL AUTO_INCREMENT,
  `startTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `repeat` int(4) NOT NULL DEFAULT '0',
  `everyDay` int(4) NOT NULL DEFAULT '0',
  `everyHour` int(4) NOT NULL DEFAULT '0',
  `everyMinute` int(4) NOT NULL DEFAULT '0',
  `everyMonth` int(4) NOT NULL DEFAULT '0',
  `everyYear` int(4) NOT NULL DEFAULT '0',
  `AtYear` int(5) NOT NULL DEFAULT '0',
  `AtMonth` int(5) NOT NULL DEFAULT '0',
  `atDay` int(4) NOT NULL DEFAULT '0',
  `atHour` int(4) NOT NULL DEFAULT '0',
  `atMinute` int(4) NOT NULL DEFAULT '0',
  `stype` int(4) DEFAULT NULL,
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `snapShots`
--

CREATE TABLE `snapShots` (
  `id` varchar(40) NOT NULL DEFAULT '',
  `srcHostid` varchar(40) NOT NULL DEFAULT '',
  `virtualsnapShotid` varchar(64) DEFAULT '0',
  `srcDeviceName` varchar(255) NOT NULL DEFAULT '',
  `destHostid` varchar(40) NOT NULL DEFAULT '',
  `destDeviceName` varchar(255) NOT NULL DEFAULT '',
  `pending` tinyint(1) NOT NULL DEFAULT '0',
  `bytesSent` bigint(20) NOT NULL DEFAULT '0',
  `status` int(11) NOT NULL DEFAULT '0',
  `snapshotId` bigint(20) DEFAULT '0',
  `executionState` int(4) DEFAULT NULL,
  `snaptype` int(4) DEFAULT '0',
  `recoveryOption` text,
  `errMessage` text,
  `actualRecoveryPoint` varchar(255) DEFAULT '',
  `startTime` varchar(255) DEFAULT '',
  `endTime` varchar(255) DEFAULT '',
  `isMounted` tinyint(1) DEFAULT '0',
  `deleteLog` tinyint(1) DEFAULT '0',
  `readWrite` tinyint(1) DEFAULT '0',
  `dataLogPath` varchar(255) DEFAULT NULL,
  `lastUpdateTime` varchar(255) NOT NULL DEFAULT '',
  `recoveryType` text NOT NULL,
  `mountedOn` text,
  `retentionlogsCleanup` tinyint(1) DEFAULT '0',
  `retentionPruning` tinyint(1) DEFAULT '0',
  `planId` int(10) DEFAULT NULL,
  `scenarioId` int(10) DEFAULT NULL,
  `applicationType` varchar(40) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `srcHostid` (`srcHostid`),
  KEY `srcDeviceName` (`srcDeviceName`),
  KEY `destHostid` (`destHostid`),
  KEY `destDeviceName` (`destDeviceName`),
  KEY `snapshotId` (`snapshotId`),
  KEY `snaptype` (`snaptype`),
  KEY `executionState` (`executionState`),
  CONSTRAINT `hosts_snapShots_srcHostid` FOREIGN KEY (`srcHostid`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_snapShots_destHostid` FOREIGN KEY (`destHostid`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `snapshotMain`
--
CREATE TABLE `snapshotMain` (
  `snapshotId` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `srcHostid` varchar(40) NOT NULL DEFAULT '',
  `srcDeviceName` varchar(255) NOT NULL DEFAULT '',
  `destHostid` varchar(40) NOT NULL DEFAULT '',
  `destDeviceName` varchar(255) NOT NULL DEFAULT '',
  `pending` tinyint(1) NOT NULL DEFAULT '0',
  `bytesSent` bigint(20) NOT NULL DEFAULT '0',
  `status` int(11) NOT NULL DEFAULT '0',
  `typeOfsnapshot` int(4) NOT NULL DEFAULT '0',
  `volumeType` int(4) NOT NULL DEFAULT '0',
  `preCommandsource` varchar(255) DEFAULT NULL,
  `postCommandsource` varchar(255) DEFAULT NULL,
  `snapScheduleId` bigint(20) unsigned DEFAULT NULL,
  `scheduledVol` varchar(255) DEFAULT NULL,
  `lastScheduledVol` varchar(255) DEFAULT NULL,
  `lastUpdateTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `indVolflag` int(4) NOT NULL DEFAULT '0',
  `indVolumes` varchar(40) NOT NULL DEFAULT '',
  `Deleted` int(4) DEFAULT NULL,
  `queueState` int(4) DEFAULT '0',
  `ruleId` bigint(11) DEFAULT '0',
  `clusterFlag` int(4) DEFAULT '0',
  `userBookmark` varchar(255) NOT NULL DEFAULT '',
  `deleteLog` tinyint(1) NOT NULL DEFAULT '0',
  `readWrite` tinyint(1) DEFAULT NULL,
  `dataLogPath` varchar(255) DEFAULT NULL,
  `ranNo` bigint(20) DEFAULT NULL,
  `mountedOn` text,
  `planId` bigint(20) DEFAULT NULL,
  `scenarioId` bigint(20) DEFAULT NULL,
  `applicationType` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`snapshotId`),
  KEY `srcHostid` (`srcHostid`),
  KEY `srcDeviceName` (`srcDeviceName`),
  KEY `destHostid` (`destHostid`),
  KEY `destDeviceName` (`destDeviceName`),
  KEY `ruleId` (`ruleId`),
  KEY `pending_indx` (`pending`),
  KEY `status_indx` (`status`),
  CONSTRAINT `hosts_snapshotMain_srcHostid` FOREIGN KEY (`srcHostid`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_snapshotMain_destHostid` FOREIGN KEY (`destHostid`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `sqlDatabaseInfo`
--

CREATE TABLE `sqlDatabaseInfo` (
  `sqlDbId` int(5) NOT NULL AUTO_INCREMENT,
  `applicationInstanceId` int(5) DEFAULT NULL,
  `dbName` varchar(200) DEFAULT NULL,
  `dbVolume` varchar(200) DEFAULT NULL,
  `dbType` enum('0','1','2','3','4','5','6') DEFAULT NULL,
  `onlineStatus` tinyint(1) DEFAULT NULL COMMENT '1 - Online 2 - Offline 3 - Removed',
  `dataRoot` text,
  `dbFiles` text,
  PRIMARY KEY (`sqlDbId`),
  KEY `dbVolume` (`dbVolume`),
  KEY `dbType` (`dbType`),
  KEY `applicationInstanceId` (`applicationInstanceId`),
  CONSTRAINT `applicationHosts_sqlDatabaseInfo` FOREIGN KEY (`applicationInstanceId`) REFERENCES `applicationHosts` (`applicationInstanceId`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `srcLogicalVolumeDestLogicalVolume`
--
CREATE TABLE `srcLogicalVolumeDestLogicalVolume` (
  `sourceHostId` varchar(40) NOT NULL DEFAULT '',
  `sourceDeviceName` varchar(255) NOT NULL DEFAULT '',
  `destinationHostId` varchar(40) NOT NULL DEFAULT '',
  `destinationDeviceName` varchar(255) NOT NULL DEFAULT '',
  `fullSyncStartTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `fullSyncEndTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `fullSyncStatus` int(11) NOT NULL DEFAULT '0',
  `fullSyncBytesSent` bigint(20) NOT NULL DEFAULT '0',
  `resyncStartTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `resyncEndTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `resyncStatus` int(11) NOT NULL DEFAULT '0',
  `lastResyncOffset` bigint(20) NOT NULL DEFAULT '0',
  `differentialStartTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `differentialEndTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `differentialStatus` int(11) NOT NULL DEFAULT '0',
  `lastDifferentialOffset` bigint(20) NOT NULL DEFAULT '0',
  `returnhomeEndTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `returnhomeStartTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `returnHomeStatus` int(11) NOT NULL DEFAULT '0',
  `lastSentinelChange` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `lastTMChange` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `lastOutpostAgentChange` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `wanLagTicks` bigint(20) NOT NULL DEFAULT '0',
  `shouldResync` int(11) NOT NULL DEFAULT '0',
  `remainingDifferentialBytes` bigint(20) NOT NULL DEFAULT '0',
  `remainingResyncBytes` bigint(20) NOT NULL DEFAULT '0',
  `currentRPOTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `statusUpdateTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `profilingMode` int(11) NOT NULL DEFAULT '0',
  `rpoSLAThreshold` int(20) NOT NULL DEFAULT '0',
  `maxResyncFilesThreshold` bigint(20) DEFAULT NULL,
  `ftpsSourceSecureMode` tinyint(1) NOT NULL DEFAULT '0',
  `ftpsDestSecureMode` tinyint(1) NOT NULL DEFAULT '0',
  `oneToManySource` tinyint(1) NOT NULL DEFAULT '0',
  `resyncVersion` tinyint(1) NOT NULL DEFAULT '0',
  `compressionEnable` tinyint(1) NOT NULL DEFAULT '0',
  `fastResyncMatched` bigint(20) NOT NULL DEFAULT '0',
  `fastResyncUnmatched` bigint(20) NOT NULL DEFAULT '0',
  `jobId` bigint(20) NOT NULL DEFAULT '0',
  `ruleId` int(11) NOT NULL DEFAULT '0',
  `mediaretent` int(4) DEFAULT '1',
  `logsFrom` varchar(255) DEFAULT '',
  `logsTo` varchar(255) DEFAULT '',
  `volumeGroupId` varchar(255) DEFAULT '',
  `preCommandsource` varchar(255) DEFAULT '',
  `postCommandsource` varchar(255) DEFAULT '',
  `isResync` tinyint(1) NOT NULL DEFAULT '0',
  `remainingQuasiDiffBytes` bigint(20) DEFAULT '0',
  `isQuasiflag` tinyint(1) DEFAULT '0',
  `resyncStartTagtime` bigint(20) NOT NULL,
  `resyncStartTagtimeSequence` bigint(20) NOT NULL DEFAULT '0',
  `resyncEndTagtime` bigint(20) NOT NULL,
  `resyncEndTagtimeSequence` bigint(20) NOT NULL DEFAULT '0',
  `quasidiffStarttime` datetime DEFAULT '0000-00-00 00:00:00',
  `quasidiffEndtime` datetime DEFAULT '0000-00-00 00:00:00',
  `ShouldResyncSetFrom` tinyint(1) DEFAULT '0',
  `resyncSettimestamp` bigint(20) DEFAULT '0',
  `resyncSetCxtimestamp` bigint(20) DEFAULT '0',
  `throttleresync` tinyint(1) DEFAULT '0',
  `throttleDifferentials` tinyint(1) DEFAULT '0',
  `autoResyncStartType` tinyint(1) NOT NULL DEFAULT '0',
  `autoResyncStartTime` bigint(20) NOT NULL DEFAULT '0',
  `autoResyncStartHours` bigint(20) NOT NULL DEFAULT '0',
  `autoResyncStartMinutes` bigint(20) NOT NULL DEFAULT '0',
  `autoResyncStopHours` bigint(20) NOT NULL DEFAULT '0',
  `autoResyncStopMinutes` bigint(20) NOT NULL DEFAULT '0',
  `maxCompression` decimal(20,4) DEFAULT '0.0000',
  `maxUnCompression` decimal(20,4) DEFAULT '0.0000',
  `profilingId` bigint(20) DEFAULT '0',
  `pairConfigureTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `isCommunicationfromSource` tinyint(1) DEFAULT '0',
  `replication_status` tinyint(2) DEFAULT '0',
  `src_replication_status` tinyint(2) DEFAULT '0',
  `tar_replication_status` tinyint(2) DEFAULT '0',
  `differentialPendingInTarget` bigint(20) DEFAULT '0',
  `differentialApplyRate` bigint(20) DEFAULT '0',
  `replicationCleanupOptions` varchar(30) DEFAULT '0',
  `logsFromUTC` bigint(20) DEFAULT NULL,
  `logsToUTC` bigint(20) DEFAULT NULL,
  `Phy_Lunid` varchar(255) DEFAULT NULL,
  `lun_state` int(5) DEFAULT NULL COMMENT 'can be one of  START_PROTECTION_PENDING = 1,  PROTECTED = 2,  STOP_PROTECTION_PENDING = 3 or < 0 for error',
  `offset` int(20) NOT NULL,
  `resyncCounter` tinyint(2) DEFAULT '0',
  `processServerId` varchar(40) NOT NULL DEFAULT '',
  `deleted` tinyint(1) NOT NULL DEFAULT '0',
  `pauseActivity` varchar(255) DEFAULT NULL,
  `autoResume` tinyint(1) DEFAULT '0',
  `restartResyncCounter` bigint(20) unsigned DEFAULT '0',
  `planId` int(10) DEFAULT NULL,
  `scenarioId` int(10) DEFAULT NULL,
  `applicationType` varchar(40) DEFAULT NULL,
  `resyncOrder` int(10) DEFAULT '0',
  `executionState` int(10) DEFAULT '0',
  `srcCapacity` bigint(20) NOT NULL DEFAULT '0',
  `syncCopy` varchar(25) DEFAULT 'full',
  `fastResyncUnused` bigint(20) NOT NULL DEFAULT '0',
  `currentRPOTimeAtSource` bigint(20) unsigned NOT NULL,
  `differentialPendingInSource` bigint(20) NOT NULL,
  `pairId` int(11) NOT NULL DEFAULT '0',
  `usePsNatIpForSource` tinyint(1) NOT NULL default '0',
  `usePsNatIpForTarget` tinyint(1) NOT NULL default '0',
  `useCfs` tinyint(1) NOT NULL DEFAULT '0',
  `TargetDataPlane` tinyint(3) not null default '1',
  `restartPause` tinyint(1) DEFAULT '0',
  `stats` text NOT NULL,
  KEY `sourceHostId` (`sourceHostId`),
  KEY `destinationHostId` (`destinationHostId`),
  KEY `sourceDeviceName` (`sourceDeviceName`),
  KEY `destinationDeviceName` (`destinationDeviceName`),
  KEY `processServerId` (`processServerId`),
  KEY `jobId` (`jobId`),
  KEY `ruleId` (`ruleId`),
  KEY `pairId` (`pairId`),
  KEY `Phy_Lunid` (`Phy_Lunid`),
  KEY `lun_state` (`lun_state`),
  KEY `executionState_indx` (`executionState`),
  KEY `replication_status_indx` (`replication_status`),
  KEY `logsFromUTC_indx` (`logsFromUTC`),
  KEY `logsToUTC_indx` (`logsToUTC`),
  KEY `restartResyncCounter_indx` (`restartResyncCounter`),
  KEY `deleted_indx` (`deleted`),
  KEY `srcLogicalVolumeDestLogicalVolume_pauseActivity` (`pauseActivity`),
  KEY `srcLogicalVolumeDestLogicalVolume_autoResume` (`autoResume`),
  KEY `srcLogicalVolumeDestLogicalVolume_planId` (`planId`),
  KEY `srcLogicalVolumeDestLogicalVolume_scenarioId` (`scenarioId`),
  KEY `srcLogicalVolumeDestLogicalVolume_srcCapacity` (`srcCapacity`),
  KEY `srcLogicalVolumeDestLogicalVolume_restartPause` (`restartPause`),
  KEY `srcLogicalVolumeDestLogicalVolume_tar_replication_status` (`tar_replication_status`),
  KEY `srcLogicalVolumeDestLogicalVolume_src_replication_status` (`src_replication_status`),
  CONSTRAINT `hosts_srcLogicalVolumeDestLogicalVolume_sourceHostId` FOREIGN KEY (`sourceHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_srcLogicalVolumeDestLogicalVolume_destinationHostId` FOREIGN KEY (`destinationHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_srcLogicalVolumeDestLogicalVolume_processServerId` FOREIGN KEY (`processServerId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `srcMtDiskMapping`
--

CREATE TABLE `srcMtDiskMapping` (
  `srcHostId` varchar(40) NOT NULL,
  `trgHostId` varchar(40) NOT NULL,
  `srcDisk` varchar(255) NOT NULL,
  `trgDisk` varchar(255) NOT NULL,
  `trgLunId` varchar(40) NOT NULL,
  `status` enum('AddDiskPending','AddDiskInprogress','AddDiskSuccess','AddDiskFailed','Disk Layout Pending','Disk Layout Complete','Delete') DEFAULT NULL,
  `scenarioId` int(11) NOT NULL,
  `planId` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `standByDetails`
--

CREATE TABLE `standByDetails` (
  `appliacneClusterIdOrHostId` varchar(40) DEFAULT NULL,
  `ip_address` varchar(15) DEFAULT NULL,
  `port_number` int(5) DEFAULT NULL,
  `role` enum('PRIMARY','STANDBY') DEFAULT NULL,
  `timeout` int(4) DEFAULT NULL,
  `nat_ip` varchar(15) DEFAULT NULL,
  `dbTimeStamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `switchAgents`
--

CREATE TABLE `switchAgents` (
  `switchAgentId` varchar(40) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `name` varchar(255) DEFAULT NULL,
  `version` varchar(255) DEFAULT NULL,
  `platformModel` varchar(255) DEFAULT NULL,
  `switchWwn` varchar(32) DEFAULT NULL,
  `physicalPortInfo` varchar(64) DEFAULT NULL,
  `dpcCount` int(11) DEFAULT NULL,
  `driverVersion` varchar(64) DEFAULT NULL,
  `osInfo` varchar(255) DEFAULT NULL,
  `currentCompatibilityNum` bigint(20) DEFAULT NULL,
  `installPath` varchar(255) DEFAULT NULL,
  `osVal` tinyint(1) DEFAULT NULL,
  `timeZone` varchar(10) DEFAULT NULL,
  `timeLocal` datetime DEFAULT NULL,
  `patchHistory` text,
  `cpIpAddressList` varchar(255) DEFAULT NULL,
  `prefCpIpAddressList` varchar(255) DEFAULT NULL,
  `bpIpAddressLIst` varchar(255) DEFAULT NULL,
  `prefBpIpAddressList` varchar(255) DEFAULT NULL,
  `sasVersion` varchar(255) DEFAULT NULL,
  `fosVersion` varchar(255) DEFAULT NULL,
  `cacheState` tinyint(1) DEFAULT '1',
  `switchAgentSpaceWarnThreshold` int(3) DEFAULT '80',
  PRIMARY KEY (`switchAgentId`,`fabricId`),
  KEY `sanFabrics_switchAgents` (`fabricId`),
  CONSTRAINT `sanFabrics_switchAgents` FOREIGN KEY (`fabricId`) REFERENCES `sanFabrics` (`fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `systemHealth`
--

CREATE TABLE `systemHealth` (
  `component` varchar(255) NOT NULL DEFAULT '',
  `healthFlag` enum('healthy','degraded') NOT NULL DEFAULT 'healthy',
  `startTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `transbandSettings`
--

CREATE TABLE `transbandSettings` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `ValueName` varchar(255) DEFAULT NULL,
  `ValueData` mediumtext NOT NULL,
  `valueType` varchar(255) DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ValueName` (`ValueName`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;



--
-- Table structure for table `transportProtocol`
--
CREATE TABLE `transportProtocol` (
  `name` varchar(40) NOT NULL,
  `id` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `trapInfo`
--
CREATE TABLE `trapInfo` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `trapCommand` text NOT NULL,
  `createdTime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `userId` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `trapCommand` (`trapCommand`(767)),
  KEY `trapInfo_userId` (`userId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `trapListeners`
--
CREATE TABLE `trapListeners` (
  `trapListenerId` bigint(20) NOT NULL AUTO_INCREMENT,
  `trapListenerIPv4` varchar(255) NOT NULL DEFAULT '-NULL-',
  `trapListenerPort` int(10) NOT NULL DEFAULT '162',
  `userId` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`trapListenerId`),
  KEY `trapListeners_userId` (`userId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `trendingData`
--
CREATE TABLE `trendingData` (
  `sourceHostId` varchar(40) NOT NULL DEFAULT '',
  `sourceDeviceName` varchar(255) NOT NULL DEFAULT '',
  `destinationHostId` varchar(40) NOT NULL DEFAULT '',
  `destinationDeviceName` varchar(255) NOT NULL DEFAULT '',
  `reportHour` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `compressed` double DEFAULT NULL,
  `unCompressed` double DEFAULT NULL,
  `maxRPO` float NOT NULL DEFAULT '0',
  `pairId` int(11) NOT NULL DEFAULT '0',
  KEY `sourceHostId_sourceDeviceName` (`sourceHostId`,`sourceDeviceName`),
  KEY `destinationHostId_destinationDeviceName` (`destinationHostId`,`destinationDeviceName`),
  KEY `pairId_reportHour` (`pairId`,`reportHour`),
  KEY `reportHour` (`reportHour`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `unattendedInstallationsPending`
--
CREATE TABLE `unattendedInstallationsPending` (
  `id` varchar(40) NOT NULL DEFAULT '',
  `filename` varchar(255) NOT NULL,
  `arguments` mediumtext NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `userErrorReceivers`
--
CREATE TABLE `userErrorReceivers` (
  `UID` int(11) NOT NULL DEFAULT '0',
  `pairId` int(11) NOT NULL DEFAULT '0',
  `sendEmail` tinyint(1) NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `users`
--
CREATE TABLE `users` (
  `UID` int(11) NOT NULL DEFAULT '0',
  `uname` varchar(20) NOT NULL DEFAULT 'NULL',
  `name` varchar(255) NOT NULL DEFAULT 'NULL',
  `passwd` varchar(120) NOT NULL DEFAULT 'NULL',
  `email` varchar(255) NOT NULL DEFAULT 'NULL',
  `mailsEnabled` tinyint(1) DEFAULT '1',
  `sessiontimeout` int(4) DEFAULT '180',
  `dispatch_interval` int(11) NOT NULL DEFAULT '30' COMMENT 'email dispatch interval',
  `last_dispatch_time` bigint(20) NOT NULL DEFAULT '0' COMMENT 'last email dispatch interval',
  `userType` tinyint(1) NOT NULL DEFAULT '0',
  `emailSubject` varchar(40) DEFAULT NULL,
  `trap_dispatch_interval` int(11) NOT NULL DEFAULT '1',
  `last_trap_dispatch_time` bigint(20) NOT NULL DEFAULT '0',
  `testEmail` enum('0','1') DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `vendorInfo`
--

CREATE TABLE `vendorInfo` (
  `type` tinyint(3) NOT NULL,
  `name` varchar(255) NOT NULL,
  PRIMARY KEY (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `virtualInitiators`
--

CREATE TABLE `virtualInitiators` (
  `virtualInitiatorPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `switchAgentId` varchar(40) NOT NULL,
  `nodeWwn` varchar(40) DEFAULT NULL,
  PRIMARY KEY (`virtualInitiatorPortWwn`,`fabricId`,`switchAgentId`),
  KEY `switchAgents_virtualInitiators` (`switchAgentId`,`fabricId`),
  KEY `IDX_virtualInitiators_1` (`virtualInitiatorPortWwn`),
  CONSTRAINT `switchAgents_virtualInitiators` FOREIGN KEY (`switchAgentId`, `fabricId`) REFERENCES `switchAgents` (`switchAgentId`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `virtualNports`
--

CREATE TABLE `virtualNports` (
  `nodeWwn` varchar(32) NOT NULL,
  `startPortWwn` varchar(32) NOT NULL,
  `endPortWwn` varchar(32) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `virtualTargetBindings`
--
CREATE TABLE `virtualTargetBindings` (
  `virtualTargetBindingPortWwn` varchar(32) NOT NULL,
  `switchAgentId` varchar(40) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `nodeWwn` varchar(32) NOT NULL,
  PRIMARY KEY (`virtualTargetBindingPortWwn`,`switchAgentId`,`fabricId`),
  KEY `switchAgents_virtualTargetBindings` (`switchAgentId`,`fabricId`),
  KEY `IDX_virtualTargetBindings_1` (`virtualTargetBindingPortWwn`),
  CONSTRAINT `switchAgents_virtualTargetBindings` FOREIGN KEY (`switchAgentId`, `fabricId`) REFERENCES `switchAgents` (`switchAgentId`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `virtualTargets`
--
CREATE TABLE `virtualTargets` (
  `virtualTargetPortWwn` varchar(32) NOT NULL,
  `fabricId` varchar(40) NOT NULL,
  `switchAgentId` varchar(40) NOT NULL,
  `nodeWwn` varchar(32) NOT NULL,
  PRIMARY KEY (`virtualTargetPortWwn`,`fabricId`,`switchAgentId`),
  KEY `switchAgents_virtualTargets` (`switchAgentId`,`fabricId`),
  KEY `IDX_virtualTargets_1` (`virtualTargetPortWwn`),
  CONSTRAINT `switchAgents_virtualTargets` FOREIGN KEY (`switchAgentId`, `fabricId`) REFERENCES `switchAgents` (`switchAgentId`, `fabricId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `volumeGroups`
--
CREATE TABLE `volumeGroups` (
  `id` int(4) NOT NULL AUTO_INCREMENT,
  `displayName` varchar(255) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `volumeResizeHistory`
--
CREATE TABLE `volumeResizeHistory` (
  `hostId` varchar(40) NOT NULL,
  `deviceName` varchar(255) NOT NULL,
  `resizeTime` datetime NOT NULL,
  `oldCapacity` bigint(20) NOT NULL,
  `newCapacity` bigint(20) NOT NULL,
  `isValid` tinyint(1) NOT NULL,
  `pairId` int(11) NOT NULL default '0',
  CONSTRAINT `hosts_volumeResizeHistory` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `volumeType`
--
CREATE TABLE `volumeType` (
  `type` tinyint(3) unsigned NOT NULL,
  `name` varchar(255) NOT NULL,
  `sourceOk` tinyint(3) unsigned NOT NULL,
  `targetOk` tinyint(3) unsigned NOT NULL,
  `retLogOk` tinyint(3) unsigned NOT NULL,
  `schedulePhysicalOk` tinyint(3) unsigned NOT NULL,
  `scheduleVirtaulOk` tinyint(3) unsigned NOT NULL,
  `recoveryPhysicalOk` tinyint(3) unsigned NOT NULL,
  `recoveryVirtaulOk` tinyint(3) unsigned NOT NULL,
  `rollbackOk` tinyint(3) unsigned NOT NULL,
  PRIMARY KEY (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `xsResourcePools`
--
CREATE TABLE `xsResourcePools` (
  `poolId` varchar(40) NOT NULL,
  `poolName` varchar(255) NOT NULL,
  PRIMARY KEY (`poolId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `db2DatabaseInstances`
--
CREATE TABLE `db2DatabaseInstances` (
  `db2DatabaseInstanceId` int(11) NOT NULL auto_increment,
  `applicationInstanceId` int(11) NOT NULL,
  `instanceName` varchar(40) default NULL,
  `instanceVersion` varchar(40) default NULL,
  `recoveryLogEnabled` int(11) NOT NULL,
  PRIMARY KEY  (`db2DatabaseInstanceId`)
) ENGINE=InnoDB  DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `db2DatabaseDeviceViews`
--
CREATE TABLE `db2DatabaseDeviceViews` (
  `db2DatabaseInstanceId` int(11) default NULL,
  `viewLevel` varchar(40) default NULL,
  `parentName` varchar(255) default NULL,
  `viewLevelName` varchar(255) default NULL,
  `viewLevelType` varchar(40) default NULL,
  `leftValue` int(10) default NULL,
  `rightValue` int(10) default NULL,
  KEY `db2DatabaseInstances_db2DatabaseDeviceViews` (`db2DatabaseInstanceId`),
    CONSTRAINT `db2DatabaseInstances_db2DatabaseDeviceViews` FOREIGN KEY (`db2DatabaseInstanceId`) REFERENCES `db2DatabaseInstances` (`db2DatabaseInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `db2DatabaseFailoverDetails`
--
CREATE TABLE `db2DatabaseFailoverDetails` (
  `hostId` varchar(40) NOT NULL,
  `applicationInstanceName` varchar(40) NOT NULL default '',
  `fileName` varchar(40) NOT NULL default '',
  `fileContents` text,
  `fileType` varchar(40) NOT NULL,
  PRIMARY KEY  (`hostId`,`applicationInstanceName`,`fileName`),
  CONSTRAINT `hosts_db2DatabaseFailoverDetails` FOREIGN KEY (`hostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `db2Devices`
--
CREATE TABLE `db2Devices` (
  `applicationInstanceId` int(11) default NULL,
  `db2DatabaseInstanceId` int(11) default NULL,
  `srcDeviceName` varchar(255) default NULL,
  `sharedDeviceId` varchar(128) NOT NULL,
  `volumeType` tinyint(1) default '0',
  `vendorOrigin` varchar(40) default NULL,
  KEY `db2Devices_sharedDeviceId` (`sharedDeviceId`),
  KEY `applicationHosts_db2Devices` (`applicationInstanceId`),
  KEY `db2DatabaseInstances_db2Devices` (`db2DatabaseInstanceId`),
  CONSTRAINT `db2DatabaseInstances_db2Devices` FOREIGN KEY (`db2DatabaseInstanceId`) REFERENCES `db2DatabaseInstances` (`db2DatabaseInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `applicationHosts_db2Devices` FOREIGN KEY (`applicationInstanceId`) REFERENCES `applicationHosts` (`applicationInstanceId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `pairCatLogDetails`
--
CREATE TABLE `pairCatLogDetails` (
  `sourceHostId` varchar(40),
  `sourceDeviceName` varchar(255),
  `destinationHostId` varchar(40),
  `destinationDeviceName` varchar(255),
  `uniqueId` varchar(20) NOT NULL,
  CONSTRAINT `catlog_id` PRIMARY KEY (`sourceHostId`,`sourceDeviceName`,`destinationHostId`,`destinationDeviceName`),
  CONSTRAINT `hosts_pairCatLogDetails_sourceHostId` FOREIGN KEY (`sourceHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_pairCatLogDetails_destinationHostId` FOREIGN KEY (`destinationHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;


--
-- Table structure for table `bandwidthReport`
--
CREATE TABLE `bandwidthReport` (
  `hostId` varchar(255) NOT NULL DEFAULT '',
  `reportDate` date NOT NULL DEFAULT '0000-00-00',
  `dataIn` double NOT NULL DEFAULT '0',
  `dataOut` double NOT NULL DEFAULT '0',
  UNIQUE KEY `hostId_reportDate` (`hostId`,`reportDate`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC  ;

--
-- Table structure for table `credentials`
--
CREATE TABLE `credentials` (
  `id` int(20) NOT NULL AUTO_INCREMENT,
  `serverId` varchar(255) DEFAULT NULL,
  `userName` varchar(255) DEFAULT NULL,
  `password` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC;

--
-- Table structure for table `eventCodes`
--
CREATE TABLE `eventCodes` (
  `errCode` varchar(255) NOT NULL,
  `apiCode` mediumint(8) NOT NULL DEFAULT '0',
  `errType` varchar(60) NOT NULL DEFAULT '',
  `errorMsg` text,
  `errPossibleCauses` text,
  `errCorrectiveAction` text,
  `errComponent` varchar(60) DEFAULT NULL,
  `errManagementFlag` int(11) NOT NULL DEFAULT '3' COMMENT '2^0-ASR UI 2^1-EMAIL 2^2-MDS',
  `errInUse` enum('Yes', 'No') DEFAULT 'Yes',
  `isEvent` tinyint(4) NOT NULL DEFAULT '0',
  `category` varchar(60) NOT NULL DEFAULT '' COMMENT 'Alerts/Events, Discovery, PushInstall, Protection, Rollback, API Errors',
  `placeHolders` text,
  `suppressTime` bigint(20) NOT NULL DEFAULT 86400,
  PRIMARY KEY (`errCode`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC;


CREATE TABLE `cfs` (
  `id` varchar(40) NOT NULL,
  `publicIpAddress` varchar(20) DEFAULT NULL,
  `publicPort` varchar(10) DEFAULT '9080',
  `publicSslPort` varchar(10) DEFAULT '9443',  
  `privateIpAddress` varchar(20) DEFAULT NULL,
  `privatePort` varchar(10) DEFAULT '9080',
  `privateSslPort` varchar(10) DEFAULT '9443',
  `connectivityType` enum('VPN','NON VPN') DEFAULT 'NON VPN',  
  `heartbeat` TIMESTAMP NOT NULL COMMENT 'This is to update timestamp in UTC format. Always timestamp should be in UTC here. All inserts and updates supply value as UTC_TIMESTAMP.',
  PRIMARY KEY (`id`),
  CONSTRAINT `hosts_cfs` FOREIGN KEY (`id`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
  ) ENGINE=INNODB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC;

--
-- Table structure for table `MDSLogging`
--  
  
CREATE TABLE `MDSLogging` (
  `logId` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `activityId` varchar(255) DEFAULT NULL,
  `clientRequestId` varchar(255) DEFAULT NULL,
  `eventName` varchar(255) DEFAULT NULL,
  `errorCode` bigint(10) DEFAULT NULL,
  `errorCodeName` varchar(255) DEFAULT NULL,
  `errorType` varchar(255) DEFAULT NULL,
  `errorDetails` longtext,
  `componentName` varchar(255) DEFAULT NULL,
  `callingFunction` varchar(255) DEFAULT NULL,
  `logCreationDate` datetime DEFAULT NULL,
  PRIMARY KEY (`logId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC;

--
-- Table structure for table `TargetDataPlanes`
-- 
 CREATE TABLE `TargetDataPlanes` (
  `type` tinyint(3) unsigned NOT NULL,
  `name` varchar(255) NOT NULL,
  PRIMARY KEY (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC;


CREATE TABLE `healthfactors` (
  `sourceHostId` varchar(40) NOT NULL DEFAULT '',
  `destinationHostId` varchar(40) NOT NULL DEFAULT '',
  `pairId` int(11) NOT NULL DEFAULT '0',
  `errCode` varchar(255) NOT NULL,
  `healthFactorCode` varchar(255) NOT NULL,
  `healthFactor` tinyint(1) default '2' NOT NULL COMMENT 'Warning=1, Critical=2',
  `priority` tinyint(1) default '0' NOT NULL COMMENT 'This is to know priority of the health factor between the health factors warning, critical for the same protection.',
  `component` varchar(100) NOT NULL,
  `healthDescription` text NOT NULL,
  `healthTime` datetime DEFAULT '0000-00-00 00:00:00',
  `updated` varchar(1) NOT NULL DEFAULT 'N',
  `healthUpdateTime` datetime DEFAULT '0000-00-00 00:00:00',
  `healthFactorType` varchar(255) NOT NULL DEFAULT 'VM',
  `healthPlaceHolders` text,
  PRIMARY KEY (`pairId`,`errCode`),
  KEY `healthfactors_sourceHostId` (`sourceHostId`),
  KEY `healthfactors_destinationHostId` (`destinationHostId`),
  KEY `healthfactors_pairId` (`pairId`),
  KEY `healthfactors_priority` (`priority`),
  CONSTRAINT `hosts_healthfactors_sourceHostId` FOREIGN KEY (`sourceHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `hosts_healthfactors_destinationHostId` FOREIGN KEY (`destinationHostId`) REFERENCES `hosts` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `srcLogicalVolumeDestLogicalVolume_healthfactors_pairId` FOREIGN KEY (`pairId`) REFERENCES `srcLogicalVolumeDestLogicalVolume` (`pairId`) ON DELETE CASCADE ON UPDATE CASCADE
  ) ENGINE=INNODB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC;


CREATE TABLE `resyncerrorcodeshistory` (
  `Id` int(11) NOT NULL AUTO_INCREMENT,
  `sourceHostId` varchar(40) NOT NULL DEFAULT '',
  `sourceDeviceName` varchar(255) NOT NULL DEFAULT '',
  `destinationHostId` varchar(40) NOT NULL DEFAULT '',
  `destinationDeviceName` varchar(255) NOT NULL DEFAULT '',
  `jobId` bigint(20) NOT NULL DEFAULT '0',
  `resyncErrorCode` varchar(255) NOT NULL DEFAULT '',
  `detectedTime` bigint(20) DEFAULT '0',
  `isActionRequired` tinyint(4) NOT NULL DEFAULT '0' COMMENT 'For manual resync error code it is 1, otherwise 0',
  `firstReportedTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `updatedTime` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
  `counter` int(11) NOT NULL DEFAULT '0',
  `reprotedComponent` varchar(60) DEFAULT NULL COMMENT 'CS/PS/SOURCE/MT',
  `priority` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'This is to identify the priority of resync error code between the list of reported error codes of same disk.',
  `placeHolders` text,
  PRIMARY KEY (`Id`),
  KEY `sourceHostId_indx` (`sourceHostId`),
  KEY `sourceDeviceName_indx` (`sourceDeviceName`),
  KEY `destinationHostId_indx` (`destinationHostId`),
  KEY `destinationDeviceName_indx` (`destinationDeviceName`),
  KEY `jobId_indx` (`jobId`),
  KEY `resyncErrorCode_indx` (`resyncErrorCode`),
  KEY `isActionRequired_indx` (`isActionRequired`),
  CONSTRAINT `srcLogicalVolumeDestLogicalVolume_resyncerrorcodeshistory` FOREIGN KEY (`sourceHostId`) REFERENCES `srcLogicalVolumeDestLogicalVolume` (`sourceHostId`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC;

-- Populate Configuration Data                                           
--
-- Dumping data for table `TargetDataPlanes`
--
INSERT INTO `TargetDataPlanes` VALUES 
(0,'UNSUPPORTED_DATA_PLANE'),
(1,'INMAGE_DATA_PLANE'),
(2,'AZURE_DATA_PLANE'); 

--
-- Dumping data for table `Applications`
--
INSERT INTO `Applications` VALUES 
(1,'Microsoft Exchange 2003','Exchange2003'),
(2,'SQL Server 2000','SQL'),
(3,'SQL Server 2005','Sql2005'),
(4,'Registry','Registry'),
(5,'System','SystemFiles'),
(6,'Event Log','EventLog'),
(7,'WMI','WMI'),
(8,'IIS Metabase','IISMETABASE'),
(9,'COM+ REGDB','COM+REGDB'),
(10,'Certificate Authority','CA'),
(11,'Active Directory','AD'),
(12,'Dhcp Jet','DHCP'),
(13,'BITS','BITS'),
(14,'WINS Jet','WINS'),
(15,'Removable Storage Manager','RSM'),
(16,'TermServLicensing','TSL'),
(17,'FRS','FRS'),
(18,'File System','FS'),
(19,'Microsoft (Bootable State)','BootableState'),
(20,'Microsoft (Service State)','ServiceState'),
(21,'Cluster Service','ClusterService'),
(22,'User Defined','USERDEFINED'),
(23,'Microsoft Exchange 2007','Exchange2007'),
(24,'SQL Server 2008','Sql2008'),
(25,'Mysql','Mysql'),
(26,'Oracle 10g','Oracle10g'),
(27,'Oracle 11g','Oracle11g'),
(28,'FileServer','FileServer'),
(29,'Microsoft Exchange 2010','Exchange2010');

--
-- Dumping data for table `applicationPlan`
--

INSERT INTO `applicationPlan` VALUES (1,'Other','OTHER','HOST','INACTIVE',0,0,NULL,0,0,'','',1);


--
-- Dumping data for table `consistencyTagList`
--

INSERT INTO `consistencyTagList` VALUES 
(1,'Microsoft Exchange','Exchange','ACM'),
(2,'SQL Server 2000','SQL','ACM'),
(3,'SQL Server 2005','Sql2005','ACM'),
(4,'Registry','Registry','ACM'),
(5,'System','SystemFiles','ACM'),
(6,'Event Log','EventLog','ACM'),
(7,'WMI','WMI','ACM'),
(8,'IIS Metabase','IISMETABASE','ACM'),
(9,'COM+ REGDB','COM+REGDB','ACM'),
(10,'Certificate Authority','CA','ACM'),
(11,'Active Directory','AD','ACM'),
(12,'Dhcp Jet','DHCP','ACM'),
(13,'BITS','BITS','ACM'),
(14,'WINS Jet','WINS','ACM'),
(15,'Removable Storage Manager','RSM','ACM'),
(16,'TermServLicensing','TSL','ACM'),
(17,'FRS','FRS','ACM'),
(18,'File System','FS','ACM'),
(19,'Microsoft (Bootable State)','BootableState','ACM'),
(20,'Microsoft (Service State)','ServiceState','ACM'),
(21,'Cluster Service','ClusterService','ACM'),
(22,'User Defined','USERDEFINED','ACM'),
(23,'Microsoft Exchange Information Store','ExchangeIS','ACM'),
(24,'Microsoft Exchange Replication','ExchangeRepl','ACM'),
(25,'SQL Server 2008','Sql2008','ACM'),
(26,'SharePoint','sharepoint','ACM'),
(27,'Office Search','OSearch','ACM'),
(28,'SharePoint Search','SPSearch','ACM'),
(29,'MS Hyper-V','HyperV','ACM'),
(30,'Shadow Copy Optimization','ShadowCopyOptimization','ACM'),
(31,'Automated System Recovery','ASR','ACM'),
(32,'MS Search','MSSearch','ACM'),
(33,'Cluster Database','ClusterDB','ACM'),
(34,'Performance Counters Writer','PerformanceCounter','ACM'),
(35,'VSS Metadata Store Writer','VSSMetaDataStore','ACM'),
(36,'Task Scheduler Writer','TaskScheduler','ACM'),
(37,'IIS Config Writer','IISConfig','ACM'),
(38,'Active Directory Lightweight Directory Services (LDS) VSS Writer','ADAM','ACM'),
(39,'File Server Resource Manager (FSRM) Writer','FSRM','ACM'),
(40,'NPS VSS Writer(Network Policy Server (NPS))','NPS','ACM'),
(41,'Remote Desktop Services (Terminal Services) Gateway VSS Writer','TSG','ACM'),
(42,'Distributed File System Replication','DFSR','ACM'),
(43,'VMWare VSS WRiter','VMWare','ACM'),
(44,'System Level','SystemLevel','ACM'),
(45,'Application VSS Writer','APPVSSWRITER','ACM'),
(46,'SQL Server 2012','Sql2012','ACM');


--
-- Dumping data for table `dpsLogicalVolumes`
--

INSERT INTO `dpsLogicalVolumes` VALUES 
('/dev/svdevice0','0',8192000),
('/dev/svdevice1','1',8192000),
('/dev/svdevice10','10',8192000),
('/dev/svdevice11','11',8192000),
('/dev/svdevice12','12',8192000),
('/dev/svdevice13','13',8192000),
('/dev/svdevice14','14',8192000),
('/dev/svdevice15','15',8192000),
('/dev/svdevice16','16',8192000),
('/dev/svdevice17','17',8192000),
('/dev/svdevice18','18',8192000),
('/dev/svdevice19','19',8192000),
('/dev/svdevice2','2',8192000),
('/dev/svdevice20','20',8192000),
('/dev/svdevice21','21',8192000),
('/dev/svdevice22','22',8192000),
('/dev/svdevice23','23',8192000),
('/dev/svdevice24','24',8192000),
('/dev/svdevice25','25',8192000),
('/dev/svdevice26','26',8192000),
('/dev/svdevice27','27',8192000),
('/dev/svdevice28','28',8192000),
('/dev/svdevice29','29',8192000),
('/dev/svdevice3','3',8192000),
('/dev/svdevice30','30',8192000),
('/dev/svdevice31','31',8192000),
('/dev/svdevice32','32',8192000),
('/dev/svdevice33','33',8192000),
('/dev/svdevice34','34',8192000),
('/dev/svdevice35','35',8192000),
('/dev/svdevice36','36',8192000),
('/dev/svdevice37','37',8192000),
('/dev/svdevice38','38',8192000),
('/dev/svdevice39','39',8192000),
('/dev/svdevice4','4',8192000),
('/dev/svdevice40','40',8192000),
('/dev/svdevice41','41',8192000),
('/dev/svdevice42','42',8192000),
('/dev/svdevice43','43',8192000),
('/dev/svdevice44','44',8192000),
('/dev/svdevice45','45',8192000),
('/dev/svdevice46','46',8192000),
('/dev/svdevice47','47',8192000),
('/dev/svdevice48','48',8192000),
('/dev/svdevice49','49',8192000),
('/dev/svdevice5','5',8192000),
('/dev/svdevice50','50',8192000),
('/dev/svdevice6','6',8192000),
('/dev/svdevice7','7',8192000),
('/dev/svdevice8','8',8192000),
('/dev/svdevice9','9',8192000);

--
-- Dumping data for table `errorCodes`
--

INSERT INTO `errorCodes` VALUES 
(1,'Exchange server name matches with one of the forbidden names','Change the Exchange server name so that it does not match with any of the following forbidden names: 1. Microsoft 2. Exchange 3. Configuration 4. Service 5. Groups 6. Administrative 7. Any names of  a. Storage groups b. Mail stores c. Information Store',1,'Exchange server failover will fail as a result of failure of migration of storage groups, mail stores and users from the primary Exchange server to the secondary.'),
(2,'Application’s volume name could not be retrieved','Please make sure the application is configured correctly',2,'This is a limitation in Scout.'),
(3,'The application uses nested mount points.','Nested mount points are not supported in Scout. Reconfigure the application without using nested mount points.',2,'This is a limitation in Scout.'),
(4,'Application data was found on the system drive.','Relocate the application data to another drive.\r\n\nTo move SQL data, use the tool MoveSQLDbs .exe, present in the agent installation directory. Run following command ,to get help on the usage of this tool:\r\n\nC:Program FilesInMage Systems>MoveSQLDbs.exe -help\r\n\r\n\nInMage Scout [Version 5.5]\r\n\nCopyright (c) 2003  InMage Systems Inc. All rights reserved.\r\n\r\n\r\n\n***************** MovingDbs Usage ******************\r\n\r\n\nMovingDbs   -Sqlserver &lt;SqlServer Name&gt;\r\n\n            -movedb  &lt;Give the names of the specific databases separated with ’;’ to move&gt;\r\n\n            -moveall &lt;To move all the databases&gt;\r\n\n            -tgtpath &lt;Target Path to move the databases&gt;\r\n\n             Ensure that this path is not ending with  character.\r\n\n            [-help/?/-h]\r\n\r\n\r\n\nNOTE: -movedb and -moveall options are mutually exclusive',3,'Scout does not permit protection of the system drive.'),
(5,'This server has multiple IP addresses.','Multiple IP address configurations are not supported at the time of failover. Please use the Scout Host Configuration tool to let the agent use only one of the available IP addresses.',4,'DNS Failover will not work as Scout does not support modification of a single server’s multiple IP addresses at the DNS server.'),
(6,'On the primary Exchange server, LCR is enabled for the following storage groups:','Disable LCR on the mentioned primary Exchange server storage groups.',5,'Scout protects only database volumes on the primary Exchange server, and not their LCR backups. If LCR is enabled for any storage group on the primary Exchange server, databases may not mount properly on the secondary Exchange server during recovery as their LCR backups will not be available.'),
(7,'CCR is configured for the primary Exchange server.','Reconfigure the primary Exchange server without CCR.',6,'During failover of the primary Exchange server, Scout does not track the active node. This may lead to data loss.'),
(8,'Application versions at primary and secondary servers do not match.','Ensure that the application versions on primary and secondary servers are exactly identical.',7,'After a failover, the application on the secondary server may not start properly,or at all.'),
(9,'Administrative Groups of primary and secondary Exchange servers do not match.','Ensure that the primary and secondary Exchange servers belong to the same administrative group in the domain.',8,'Exchange server failover will fail as a result of failure of migration of storage groups, mail stores and users from the primary Exchange server to the secondary.'),
(10,'The primary and secondary Exchange servers do not belong to the same domain.','Ensure that the secondary Exchange server resides in the same domain as that of the primary Exchange server.',9,'Exchange server failover will fail as a result of failure of migration of storage groups, mail stores and users from the primary Exchange server to the secondary.'),
(11,'The following service was not found on the primary server:','Ensure that the mentioned service is installed and running on the primary server.',10,'The mentioned services are necessary for Scout’s proper functioning. If they are not installed or not running, Scout may be unable to ensure a smooth application protection/recovery experience. For example, Scout may not be able to ensure data consistency if the Application VSS Writer Service on the primary server is not running.'),
(12,'The following service is not running on the primary server:','Ensure that the mentioned service is installed and running on the primary server.',10,'The mentioned services are necessary for Scout’s proper functioning. If they are not installed or not running, Scout may be unable to ensure a smooth application protection/recovery experience. For example, Scout may not be able to ensure data consistency if the Application VSS Writer Service on the primary server is not running.'),
(13,'The following primary server SQL databases are offline:','Bring the mentioned primary server SQL databases online to ensure their protection.',11,'Offline databases on the primary SQL server may not be protected as their volumes may not be discovered by Scout. As a result, SQL applications may be partially protected.'),
(14,'Failed to check firewall status','1. Go to Control Panel\r\n 2. Go to Administrative Tasks\r\n 3. Change the firewall status appropriately',12,'N/A'),
(15,'Failed to get domain name for the server','Ensure that the server is added to a domain.',13,'If the server is not added to a domain, DNS and Active Directory operations cannot be performed. This causes failure of failover operations.'),
(16,'Failed to query DNS server over ','DNS server could not be reached. Please check connectivity from the primary/secondary server to the DNS server.',14,'DNS and Active Directory operations cannot be performed. This causes failure of failover operations.'),
(17,'Failed to query DNS server over ','DNS server could not be reached. Please check connectivity from the primary/secondary server to the DNS server.',14,'DNS and Active Directory operations cannot be performed. This causes failure of failover operations.'),
(18,'User does not have the privilege to modify the DNS record ','Ensure that the Service Logon user has sufficient privileges to modify DNS records.',14,'DNS and Active Directory operations cannot be performed. This causes failure of failover operations.'),
(19,'Failed to check user’s AD Update privileges.','Ensure that the Service Logon user has sufficient privileges to perform AD replication.',15,'AD replication operations cannot be performed, hampering application availability across sites.'),
(20,'Unable to retrieve the IP','Failed to retrieve the configured IP for Scout.',16,'Failover may not be complete as the required IP address in the scout configuration (drscout.conf) is not found on the system.Please ignore this error if this IP is configured at the router.This is especially required for 1.A host with multiple IPs 2.Application on MS Cluster 3. Production/DR server not in the same network as CX.'),
(21,'Configured NAT IP is not present in IP address list','Configure IP address appropriately',16,'Failover may not be complete as the required IP address in the scout configuration (drscout.conf) is not found on the system.Please ignore this error if this IP is configured at the router.This is especially required for 1.A host with multiple IPs 2.Application on MS Cluster 3. Production/DR server not in the same network as CX.'),
(22,'Failed to start vacp','Ensure that the following program is available in the Scout agent installation path:\nFor Windows: vacp.exe\nFor Linux/UNIX: vacp\nIf you cannot find this program, please report the matter to Scout Support immediately.',17,'Application recovery is not guaranteed as the application bookmarking fails.'),
(23,'Failed to issue consistency bookmark','Consistency bookmark issue was not successful. The reason can be one of these: 1. The provided VSS is not compatible with Scout\r\n2. Scout filter driver is not in the data mode\r\n3. Application has too many databases and volumes\r\n4. Application VSS writer service is not running.\r\n\r\nPlease check for the above. For more help, contact Scout Support immediately.',17,'Application recovery is not guaranteed as the application bookmarking fails.'),
(24,'Consistency bookmark could not be issued due to a VSS version problem.','Please follow the below steps to resolve the issue:\n1.	Upgrade to Windows Server 2003 SP2 \n2.	Export the contents of the HKLMSoftwareMicrosoftEventSystem key to a registry file.\n3.	Delete the HKLMSoftwareMicrosoftEventSystem{26c409cc-ae86-11d1-b616-00805fc79216}Subscriptionskey. (Just delete the Subscriptions subkey; leave the EventClasses key as it is.)\n4.	Reboot the server.\n5.	Run the \"VSSADMIN LIST WRITERS\" command to check if VSS service is running.',17,'Application recovery is not guaranteed as the application bookmarking fails.'),
(25,'The primary and secondary server drive capacities do not match.','Ensure the following:\n1.	The secondary server has equal or higher number of drives than the primary server. \n2.	The secondary server drives corresponding to primary server drives should be of equal or higher size than the latter, and should have same drive letters.\n3.	There should be at least one secondary server drive reserved for storing retention logs. This drive should have capacity as per the retention requirements.',18,'The application cannot be protected.'),
(26,'The root directories for SQL instances on primary and secondary SQL servers do not have the same paths.','Reconfigure the SQL instances on the secondary SQL server to have same root path as that of SQL instances on the primary SQL server.',19,'N/A'),
(27,'SCR is configured for the primary clustered Exchange server.','Reconfigure the primary clustered Exchange server without SCR.',20,'During failover of the primary Exchange server, Scout does not track the active node. This may lead to data loss.'),
(28,'One or more NICs on the primary server have the Dynamic DNS Update property checked.','Please uncheck the Dynamic DNS Update property for all NICs on the primary server.',21,'After a failover, client programs may not continue be able to access the application on the secondary server with the primary application server’s name. This happens if the primary server comes alive and registers its IP address with the DNS server, essentially undoing the DNS failover operation.'),
(29,'DNS is not available','Ensure that the DNS is available',22,'DNS and Active Directory operations cannot be performed, causing a failure during the failover process.'),
(30,'Domain controller is not available','Ensure that the DC is available',23,'DNS and Active Directory operations cannot be performed, causing a failure during the failover process.'),
(31,'The primary/secondary server is a DC.','Shift the DC roles and responsibilities to a server other than the primary/secondary server.',24,'Application failover will fail.'),
(32,'SQL Instance names on the primary and secondary SQL server are not the same.','Reconfigure the SQL instances on the secondary SQL server for failover to work successfully.',25,'Application failover will fail.'),
(33,'All the mail stores in a DAG should reside on a single server and should not have copies on other servers in the DAG.','Ensure that all the mail stores in the DAG reside on a single server and do not have copies on any other server in the DAG.',26,'N/A Scout does not support Exchange 2010 DAG HA configuration'),
(34,'The SQL Server installation volume is used as a secondary volume for failover/failback','Move the data to other volumes so that the SQL Server Installation volume is not used as a secondary volume in replication.',27,'The application may not available after failover/failback.'),
(35,'Cache drive does not have enough free space','Ensure that the cache drive has sufficient free space available or configure the cache directory on another drive with sufficient free space (using Scout Host Configuration tool).',28,'Replication pairs will not progress.'),
(36,'Oracle Database is not running','Start the oracle database',33,'Start the oracle database.'),
(38,'The IP addresses of primary and secondary servers should be different.','Ensure that primary and secondary servers have different IP addresses.',29,'Application Failover will fail.'),
(39,'VSS rollup version should be greater or equal to last known working version','Ensure latest VSS rollups are installed',31,'Application consistency cannot be guaranteed.'),
(40,'One of the tempDB volumes are not available at the target','Ensure same tempdb volume available at the target to make sure SQL running after the failover',30,'SQL does not start after failover.'),
(41,'Application volumes should not contains Page file configuration','Ensure no Page file is configured on the primary server volumes',32,'Failback Protection would not be possible.'),
(43,' Appliance target DUMMY_LUN_ZERO is not visible on host','1. Configure appliance target ports 2. Configure zones between host initiator and appliance target ports 3. Rediscover the ports on the host',34,'Mirror lun configuarion will fail and the application/luns cannot be protected.'),
(44,'Oracle database is not in archive log mode','Enable archive log mode for database',17,'Application recovery is not guaranteed as the application bookmarking fails.'),
(45,'Oracle database is not shutdown','Shutdown the database',35,'Oracle application failover will fail'),
(46,'Secondary server volume is in use','Unmount file system, remove from volume manager the secondary volume',36,'Resync will be stuck in case the secondary server volume is in use by filesystems or volume managers'),
(47,'Secondary server configuration does not match the Primary server','Verify and fix any system configuration issues reported in the log',37,'Oracle application failover will fail'),
(48,'The minimum space required per pair is not available in cache memory','Free some space in cache volume so that pair will get the minimum space in cache memory',38,'Pair Will Not Progress'),
(49, 'Protected DB2 Database is not active', 'Activate the Db2 Database', 39, 'Activate the db2 database'),
(50, 'DB2 database instance is not shutdown ', 'Deactivate the DB2 database', 40, 'DB2 Application failover will fail'),
(51, 'DB2 Consistency validation is failed', 'vacp Cannot freeze vxfs filesystem to issue tags', 42, 'Application recovery is not guaranteed as the application bookmarking fails');


--
-- Dumping data for table `error_policy`
--

INSERT INTO `error_policy` 
(error_template_id,send_mail,user_id,dispatch_interval,last_dispatch_time,send_trap,monitor_display,schecule_time) 
VALUES 
('AGENT_ERROR',1,1000000000,0,0,1,0,''),
('PROTECTION_ALERT',1,1000000000,0,0,1,1,''),
('CS_FAILOVER',0,1000000000,0,0,0,0,''),
('CXBPM_ALERT',1,1000000000,0,0,1,0,''),
('CXDEBUG_INF',1,1000000000,0,0,0,0,''),
('CXSTOR_WARN',1,1000000000,0,0,1,1,''),
('FXJOB_ERROR',1,1000000000,0,0,1,1,''),
('INSUFFICIENT_RET_SPACE',1,1000000000,0,0,1,1,''),
('MOVE_RETLOG',1,1000000000,0,0,1,0,''),
('PROTECT_REP',1,1000000000,1,0,0,0,''),
('PS_FAILOVER',1,1000000000,0,0,1,1,''),
('PS_UNINSTALL',1,1000000000,0,0,1,1,''),
('RESYNC_REQD',1,1000000000,0,0,1,1,''),
('RPOSLA_EXCD',1,1000000000,0,0,1,1,''),
('SWITCH_USAGE',0,1000000000,0,0,0,0,''),
('VOL_RESIZE',1,1000000000,0,0,1,1,''),
('VXAGENT_DWN',1,1000000000,0,0,1,1,''),
('FXJOB_ALERT',1,1000000000,0,0,1,1,''),
('CX_HEARTBEAT',0,1000000000,5,0,1,0,''),
('SSL_CERT_ALERTS',0,1000000000,0,0,1,0,''),
('PROTECTION_STUCK',1,1000000000,0,0,1,1,''),
('VERSION_MISMATCH',1,1000000000,0,0,1,1,'');

--
-- Dumping data for table `error_template`
--

INSERT INTO `error_template` VALUES 
('AGENT_ERROR','NOTICE','Agent Has Logged Alerts'),
('CS_FAILOVER','FATAL','CS Node Failover Alert'),
('CXBPM_ALERT','NOTICE','Bandwidth Shaping Alerts'),
('CXDEBUG_INF','NOTICE','CX Debug Info'),
('CXSTOR_WARN','WARNING','CX Secondary Storage Warning'),
('CX_HEARTBEAT','NOTICE','Cx is up and running notification'),
('FXJOB_ALERT','NOTICE','FX Agent Job Alerts'),
('FXJOB_ERROR','ERROR','FX Agent Job Error'),
('INSUFFICIENT_RET_SPACE','FATAL','Insufficient Retention Space'),
('MOVE_RETLOG','NOTICE','Retention Log Moved'),
('PROTECTION_ALERT','WARNING','Protection Alerts'),
('PROTECT_REP','NOTICE','Daily Protection Health Report'),
('PS_FAILOVER','FATAL','PS Node Failover Alert'),
('PS_UNINSTALL','FATAL','Process Server Uninstalled'),
('RESYNC_REQD','FATAL','Resync Required'),
('RPOSLA_EXCD','WARNING','RPO exceeded SLA Threshold'),
('SWITCH_USAGE','NOTICE','Disk Space Threshold on Switch-BP Exceeded'),
('VOL_RESIZE','FATAL','Source Volume Resized'),
('VXAGENT_DWN','FATAL','Agent Not Responding'),
('VCON_ERROR','NOTICE','vCon Has Logged Alerts'),
("SSL_CERT_ALERTS","ERROR", "CX SSL Cert Alerts"),
('PROTECTION_STUCK','ERROR','Protection Alerts'),
('VERSION_MISMATCH','FATAL','Version Mismatch Alert'),
('PS_ERROR', 'NOTICE', 'Process Server Has Logged Alerts');


--
-- Dumping data for table `frbGlobalSettings`
--

INSERT INTO `frbGlobalSettings` VALUES (0,0,2,0);


--
-- Dumping data for table `frbJobTemplates`
--


INSERT INTO `frbJobTemplates` (`id`, `templateName`, `templateString`, `templateType`) VALUES 
(7, 'Exchange Consistency Validation', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|N;job_checksum_size|s:4:"8192";job_whole|N;job_recursive|N;job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|N;job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|s:1:"1";job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"3";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"1200";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:0:"";job_post_command_source|s:0:"";job_pre_command_target|s:66:"application.exe -verifyconsistency -app exchange -s -t -tag LATEST";job_post_command_target|s:36:"ExchangeValidation.exe -app exchange";job_catch_all|s:7:"--super";gsched_mode|s:4:"once";gsched_once|s:11:"once_demand";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:""', 1),
(8, 'Exchange Log Rotation', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|N;job_checksum_size|s:4:"8192";job_whole|N;job_recursive|N;job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|N;job_update|N;job_existing|N;job_no_existing|s:2:"on";job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|s:2:"on";job_exclude_pattern|s:30:"*.bat;*.vbs;*.log;*.txt;*.conf";job_include|s:2:"on";job_include_pattern|s:7:".status";job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|s:2:"on";job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"2";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:0:"";job_post_command_source|s:0:"";job_pre_command_target|s:0:"";job_post_command_target|s:20:"ExchangeLogFlush.bat";job_catch_all|s:7:"--super";gsched_mode|s:4:"once";gsched_once|s:8:"once_now";gsched_once_at_year|s:4:"2006";gsched_once_at_month|s:1:"8";gsched_once_at_day|s:2:"25";gsched_once_at_hour|s:2:"18";gsched_once_at_min|s:1:"0";gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:"";', 2),
(10, 'Exchange Discovery', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|N;job_checksum_size|s:4:"8192";job_whole|N;job_recursive|N;job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|N;job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|s:1:"1";job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"2";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:0:"";job_post_command_source|s:0:"";job_pre_command_target|s:0:"";job_post_command_target|s:46:"Application.exe -discover -app Exchange -host ";job_catch_all|s:7:"--super";gsched_mode|s:5:"sched";gsched_once|N;gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"1";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:"";', 1),
(13, 'DNS Failover', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|N;job_checksum_size|s:4:"8192";job_whole|N;job_recursive|N;job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|N;job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|s:1:"1";job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"2";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:0:"";job_post_command_source|s:0:"";job_pre_command_target|s:0:"";job_post_command_target|s:23:"dns.exe -s -t -failover";job_catch_all|s:7:"--super";gsched_mode|s:4:"once";gsched_once|s:11:"once_demand";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:"";', 1),
(14, 'DNS Failback', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|N;job_checksum_size|s:4:"8192";job_whole|N;job_recursive|N;job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|N;job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|s:1:"1";job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"2";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:0:"";job_post_command_source|s:0:"";job_pre_command_target|s:0:"";job_post_command_target|s:42:"dns.exe -failback -host -ip 192.168.80.240";job_catch_all|s:7:"--super";gsched_mode|s:4:"once";gsched_once|s:11:"once_demand";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:"";', 1),
(24, 'Exchange 2007 Consistency Validation', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|N;job_checksum_size|s:4:"8192";job_whole|N;job_recursive|N;job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|N;job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|s:1:"1";job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"3";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:0:"";job_post_command_source|s:0:"";job_pre_command_target|s:70:"application.exe -verifyconsistency -app exchange2007 -s -t -tag LATEST";job_post_command_target|s:40:"ExchangeValidation.exe -app exchange2007";job_catch_all|s:7:"--super";gsched_mode|s:4:"once";gsched_once|s:11:"once_demand";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:""', 1),
(31, 'Oracle(Unix/Linux) Consistency','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:2069022279;template_name|s:30:\"Save_Application_Filestructure\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|s:1:\"0\";job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:79:\"application.sh  -a oracle -h <ORACLE_HOME> -c <ORACLE_SID>  -u <ORACLE_OS_USER>\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:0:\"\";job_catch_all|s:0:\"\";gsched_mode|s:5:\"sched\";gsched_once|N;gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:\"sched_freq\";gsched_sched_freq_days|N;gsched_sched_freq_hours|N;gsched_sched_freq_mins|s:2:\"15\";gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5),
(32, 'Oracle(Unix/Linux) Unplanned Failover','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:1883778651;template_name|s:25:\"Oracle Unplanned Failover\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|s:1:\"0\";job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:0:\"\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:98:\"application.sh -a oracle -h <ORACLE_HOME> -c <ORACLE_SID> -u <ORACLE_OS_USER> -unplanned -failover\";job_catch_all|s:0:\"\";gsched_mode|s:4:\"once\";gsched_once|s:11:\"once_demand\";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|N;gsched_sched_freq_days|N;gsched_sched_freq_hours|N;gsched_sched_freq_mins|N;gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5),
(33, 'Oracle(Unix/Linux) Planned Failover','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:882077499;template_name|s:23:\"Oracle Planned Failover\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|s:1:\"0\";job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:96:\"application.sh -a oracle -h <ORACLE_HOME> -c <ORACLE_SID>  -u <ORACLE_OS_USER> -t <tag> -planned\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:106:\"application.sh -a oracle -h <ORACLE_HOME> -c <ORACLE_SID>  -u <ORACLE_OS_USER> -t <tag> -planned -failover\";job_catch_all|s:0:\"\";gsched_mode|s:4:\"once\";gsched_once|s:11:\"once_demand\";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|N;gsched_sched_freq_days|N;gsched_sched_freq_hours|N;gsched_sched_freq_mins|N;gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5),
(34, 'MySQL Planned Failover','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:270552651;template_name|s:22:\"MySQL Planned Failover\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:110:\"application.sh  -a  mysql -c default -h <MySQL_BASEDIR>  -u <MySQL_USER> -p <MySQL_PASSWD>  -t <tag>  -planned\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:121:\"application.sh  -a  mysql -c default -h <MySQL_BASEDIR>  -u <MySQL_USER> -p <MySQL_PASSWD>  -t <tag>  -planned  -failover\";job_catch_all|s:0:\"\";gsched_mode|s:4:\"once\";gsched_once|s:11:\"once_demand\";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|N;gsched_sched_freq_days|N;gsched_sched_freq_hours|N;gsched_sched_freq_mins|N;gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5),
(35, 'MySQL Unplanned Failover','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:920311192;template_name|s:24:\"MySQL Unplanned Failover\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:0:\"\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:112:\"application.sh  -a  mysql -c default -h <MySQL_BASEDIR>  -u <MySQL_USER> -p <MySQL_PASSWD>  -unplanned -failover\";job_catch_all|s:0:\"\";gsched_mode|s:4:\"once\";gsched_once|s:11:\"once_demand\";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|N;gsched_sched_freq_days|N;gsched_sched_freq_hours|N;gsched_sched_freq_mins|N;gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5),
(38, 'Oracle_Consistency_Windows', 'valid_user|b:1;username|s:5:"admin";job_nonce|i:1230142946;template_name|s:26:"Oracle_Consistency_Windows";sessiontimeout|i:180;firstseen|i:1207687598;job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|s:2:"on";job_checksum_size|s:4:"8192";job_whole|N;job_recursive|s:0:"";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:"on";job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|s:2:"on";job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"3";job_stats|s:2:"on";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:70:"oracle.bat <oracle_home> <oracle_sid> <inmage_install_dir> consistency";job_post_command_source|s:0:"";job_pre_command_target|s:0:"";job_post_command_target|s:0:"";job_catch_all|s:7:"--super";gsched_mode|s:5:"sched";gsched_once|N;gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"6";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:13:"failoverdata";job_tarDir|s:13:"failoverdata";', 1),
(47, 'MySQL Consistency','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:2069022279;template_name|s:30:\"Save_Application_Filestructure\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|s:1:\"0\";job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:89:\"application.sh  -a mysql -c default -h <MySQL_BASEDIR>  -u <MySQL_USER> -p <MySQL_PASSWD>\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:0:\"\";job_catch_all|s:0:\"\";gsched_mode|s:5:\"sched\";gsched_once|N;gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:\"sched_freq\";gsched_sched_freq_days|N;gsched_sched_freq_hours|N;gsched_sched_freq_mins|s:2:\"15\";gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5),
(65, 'Oracle(Unix/Linux) Discovery','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:2069022279;template_name|s:30:\"Save_Application_Filestructure\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|s:1:\"0\";job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:89:\"application.sh -a oracle -discover -u <ORACLE_USER_NAME> -h <ORACLE_HOME> -c <ORACLE_SID>\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:37:\"oracletargetdiscovery.sh <ORACLE_SID>\";job_catch_all|s:0:\"\";gsched_mode|s:5:\"sched\";gsched_once|N;gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:\"sched_freq\";gsched_sched_freq_days|s:1:\"1\";gsched_sched_freq_hours|N;gsched_sched_freq_mins|N;gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5),
(66, 'MySQL Discovery','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:2069022279;template_name|s:30:\"Save_Application_Filestructure\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|s:1:\"0\";job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:41:\"application.sh -a mysql -discover -u root\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:23:\"mysqltargetdiscovery.sh\";job_catch_all|s:0:\"\";gsched_mode|s:5:\"sched\";gsched_once|N;gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:\"sched_freq\";gsched_sched_freq_days|s:1:\"1\";gsched_sched_freq_hours|N;gsched_sched_freq_mins|N;gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5),
(78, 'Exchange 2010 Planned Failover', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|s:2:"on";job_checksum_size|s:4:"8192";job_whole|N;job_recursive|s:2:"on";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:"on";job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"2";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:77:"application.exe -failover -planned -app Exchange2010 -s -t -builtIn -tag NONE";job_post_command_source|s:0:"";job_pre_command_target|s:0:"";job_post_command_target|s:80:"application.exe -failover -planned -app Exchange2010 -tag PLANNED -s -t -builtIn";job_catch_all|s:7:"--super";gsched_mode|s:4:"once";gsched_once|s:11:"once_demand";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:"";', 1),
(79, 'Exchange 2010 Discovery', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|N;job_checksum_size|s:4:"8192";job_whole|N;job_recursive|N;job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|N;job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|s:1:"1";job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"2";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:55:"Application.exe -discover -app Exchange2010 -host -Reg ";job_post_command_source|s:0:"";job_pre_command_target|s:0:"";job_post_command_target|s:50:"Application.exe -discover -app Exchange2010 -host ";job_catch_all|s:7:"--super";gsched_mode|s:5:"sched";gsched_once|N;gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"1";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:"";', 1),
(81, 'Exchange 2010 Unplanned Failover', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|N;job_checksum_size|s:4:"8192";job_whole|N;job_recursive|N;job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|N;job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|s:1:"1";job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"2";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:0:"";job_post_command_source|s:0:"";job_pre_command_target|s:0:"";job_post_command_target|s:81:"application.exe -failover -unplanned -app Exchange2010 -s -t -builtIn -tag LATEST";job_catch_all|s:7:"--super";gsched_mode|s:4:"once";gsched_once|s:11:"once_demand";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:"";', 1),
(82, 'Exchange 2010 Planned Failback', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|s:2:"on";job_checksum_size|s:4:"8192";job_whole|N;job_recursive|s:2:"on";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:"on";job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"2";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:77:"application.exe -failback -planned -app Exchange2010 -s -t -builtIn -tag NONE";job_post_command_source|s:0:"";job_pre_command_target|s:0:"";job_post_command_target|s:80:"application.exe -failback -planned -app Exchange2010 -tag PLANNED -s -t -builtIn";job_catch_all|s:7:"--super";gsched_mode|s:4:"once";gsched_once|s:11:"once_demand";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:"";', 1),
(83, 'Exchange 2010 Consistency Validation', 'valid_user|b:1;username|s:5:"admin";job_no_default|i:1;job_directory_contents|s:18:"directory_contents";job_checksum|N;job_checksum_size|s:4:"8192";job_whole|N;job_recursive|N;job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|N;job_update|s:2:"on";job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|s:1:"1";job_ignore_time|s:1:"0";job_ignore_time_window|N;job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|N;job_delete_nonexisting|N;job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:"links";job_unsafe_links|s:2:"no";job_permissions|N;job_owner|N;job_group|N;job_devices|N;job_times|s:2:"on";job_verbosity|s:1:"3";job_stats|s:2:"on";job_progress|s:2:"on";job_temp|N;job_temp_dir|N;job_one_fs|s:2:"on";job_timeout|s:2:"on";job_timeout_sec|s:4:"9600";job_port|s:2:"on";job_port_num|s:3:"874";job_bwidth|N;job_bw_limit|N;job_direction|s:1:"0";job_cpu_throttle|s:1:"0";job_rpo_threshold|s:1:"0";job_progress_threshold|s:1:"5";job_pre_command_source|s:0:"";job_post_command_source|s:0:"";job_pre_command_target|s:70:"application.exe -verifyconsistency -app exchange2010 -s -t -tag LATEST";job_post_command_target|s:40:"ExchangeValidation.exe -app exchange2010";job_catch_all|s:7:"--super";gsched_mode|s:4:"once";gsched_once|s:11:"once_demand";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:"sched_freq";gsched_sched_freq_days|s:1:"0";gsched_sched_freq_hours|s:1:"0";gsched_sched_freq_mins|s:1:"0";gsched_sched_daily_hour|s:2:"00";gsched_sched_daily_min|s:2:"00";gsched_sched_weekly_day|s:1:"0";gsched_sched_weekly_hour|s:2:"00";gsched_sched_weekly_min|s:2:"00";job_srcDir|s:0:"";job_tarDir|s:0:""', 1),
(86, 'DB2 Consistency','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:2069022279;template_name|s:30:\"Save_Application_Filestructure\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|s:1:\"0\";job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:43:\"db2.sh -user <username> -action consistency\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:0:\"\";job_catch_all|s:0:\"\";gsched_mode|s:5:\"sched\";gsched_once|N;gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|s:10:\"sched_freq\";gsched_sched_freq_days|N;gsched_sched_freq_hours|N;gsched_sched_freq_mins|s:2:\"15\";gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5),
(87, 'DB2 Failover','valid_user|b:1;username|s:5:\"admin\";job_nonce|i:882077499;template_name|s:20:\"DB2 Planned Failover\";job_no_default|i:1;job_directory_contents|s:18:\"directory_contents\";job_checksum|N;job_checksum_size|N;job_whole|N;job_recursive|s:0:\"\";job_backup|N;job_backup_dir|N;job_backup_suffix|N;job_compress|s:2:\"on\";job_update|N;job_existing|N;job_no_existing|N;job_ignore_match|N;job_ignore_size|N;job_ignore_time|s:1:\"0\";job_ignore_time_window|s:1:\"0\";job_exclude|N;job_exclude_pattern|N;job_include|N;job_include_pattern|N;job_enable_delete|s:2:\"on\";job_delete_nonexisting|s:2:\"on\";job_delete_excluded|N;job_delete_after|N;job_partial|N;job_links|s:5:\"links\";job_unsafe_links|s:2:\"no\";job_permissions|s:2:\"on\";job_owner|N;job_group|s:2:\"on\";job_devices|N;job_times|s:2:\"on\";job_verbosity|s:1:\"3\";job_stats|s:2:\"on\";job_progress|N;job_temp|N;job_temp_dir|N;job_one_fs|s:2:\"on\";job_timeout|s:2:\"on\";job_timeout_sec|s:4:\"8192\";job_port|s:2:\"on\";job_port_num|s:3:\"874\";job_bwidth|N;job_bw_limit|N;job_direction|s:1:\"0\";job_cpu_throttle|s:1:\"0\";job_rpo_threshold|s:1:\"0\";job_progress_threshold|s:1:\"5\";job_pre_command_source|s:0:\"\";job_post_command_source|s:0:\"\";job_pre_command_target|s:0:\"\";job_post_command_target|s:74:\"db2.sh -user <username> -group <groupname> -action failover -tag <tagname>\";job_catch_all|s:0:\"\";gsched_mode|s:4:\"once\";gsched_once|s:11:\"once_demand\";gsched_once_at_year|N;gsched_once_at_month|N;gsched_once_at_day|N;gsched_once_at_hour|N;gsched_once_at_min|N;gsched_sched|N;gsched_sched_freq_days|N;gsched_sched_freq_hours|N;gsched_sched_freq_mins|N;gsched_sched_daily_hour|N;gsched_sched_daily_min|N;gsched_sched_weekly_day|N;gsched_sched_weekly_hour|N;gsched_sched_weekly_min|N;job_srcDir|s:13:\"failover_data\";job_tarDir|s:13:\"failover_data\";', 5);

--
-- Dumping data for table `globalHealthSettings`
--
INSERT INTO `globalHealthSettings` VALUES (1,'GlobalPlanHealthSettings','Warning','Warning','Critical','Critical','Warning','Critical','Critical','Critical','Warning','Critical');

--
-- Dumping data for table `hosts`
--
INSERT INTO `hosts` (`id`, `name`, `ipaddress`, `sentinelEnabled`, `outpostAgentEnabled`, `filereplicationAgentEnabled`, `sentinel_version`, `outpost_version`, `fr_version`, `involflt_version`, `osType`, `vxTimeout`, `permissionState`, `lastHostUpdateTime`, `vxAgentPath`, `fxAgentPath`, `compatibilityNo`, `usecxnatip`, `cx_natip`, `osFlag`, `lastHostUpdateTimeFx`, `sshPort`, `sshVersion`, `sshUser`, `fos_version`, `type`, `hardware_configuration`, `extended_version`, `time_zone`, `PatchHistoryVX`, `PatchHistoryFX`, `agentTimeStamp`, `agentTimeZone`, `InVolCapacity`, `InVolFreeSpace`, `SysVolPath`, `SysVolCap`, `SysVolFreeSpace`, `agent_state`, `alias`, `usepsnatip`, `processServerEnabled`, `prod_version`) VALUES 
('5C1DAEF0-9386-44a5-B92A-31FE2C79236A', 'InMageProfiler', '', 0, 1, 0, '', '', '', '', '', 0, 0, 0, NULL, NULL, 0, 0, '', 1, '0000-00-00 00:00:00', 0, 'Unknown', 'Unknown', '', '', '', '', '', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL, NULL);


--
-- Dumping data for table `logRotationList`
--

INSERT INTO `logRotationList` VALUES 
(1,'xferlog','/var/log/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\log\\',0,10,0,'0000-00-00 00:00:00'),
(2,'tls.log','/var/log/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\log\\',0,10,0,'0000-00-00 00:00:00'),
(3,'boot.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(4,'wtmp','/var/log/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\log\\',0,10,0,'0000-00-00 00:00:00'),
(5,'mysqld.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(6,'secure','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(7,'spooler','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(8,'maillog','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(9,'snmpd','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(10,'messages','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(11,'rsyncd.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(12,'cdpcli.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(13,'vxlogs','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(14,'axiom_pcli.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(15,'s2.xfer.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(16,'CacheMgr.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(17,'vacps.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(18,'s2.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(19,'CDPMgr.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(20,'appservice.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(21,'svagents.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(22,'fabricagent.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(23,'read-only-mount.log','/var/log/',NULL,0,10,0,'0000-00-00 00:00:00'),
(24,'ssl_access_log','/var/log/httpd/',NULL,0,10,0,'0000-00-00 00:00:00'),
(25,'access_log','/var/log/httpd/',NULL,0,10,0,'0000-00-00 00:00:00'),
(26,'error_log','/var/log/httpd/',NULL,0,10,0,'0000-00-00 00:00:00'),
(27,'ssl_error_log','/var/log/httpd/',NULL,0,10,0,'0000-00-00 00:00:00'),
(28,'ssl_request_log','/var/log/httpd/',NULL,0,10,0,'0000-00-00 00:00:00'),
(29,'cxserver.err','/home/.install/',NULL,0,10,0,'0000-00-00 00:00:00'),
(30,'cxserver.log','/home/.install/',NULL,0,10,0,'0000-00-00 00:00:00'),
(32,'PushInstallService.log','/home/svsystems/bin/',NULL,0,10,0,'0000-00-00 00:00:00'),
(36,'host.log','/home/svsystems/var/hosts/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\hosts\\',1,10,0,'0000-00-00 00:00:00'),
(37,'is.log','/home/svsystems/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\',1,10,0,'0000-00-00 00:00:00'),
(38,'Message.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
-- (39,'<HOST_NAME>.err',NULL,'CX_INSTALL_PATH_BACKSLASH\\Program Files (x86)\\MySQL\\MySQL Server 5.1\\data\\',0,10,0,'0000-00-00 00:00:00'),
(40,'perf.log',NULL,NULL,0,10,0,'0000-00-00 00:00:00'),
(41,'php_sql_error.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(42,'perl_sql_error.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(43,'clusterlog.txt','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(44,'volume_register.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(45,'xsinfo.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(46,'unmarshal.txt','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(47,'requests.csv','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(48,'VolumeProtection.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(49,'cxcli_log.txt','/home/svsystems/var/cli/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\cli\\',0,10,0,'0000-00-00 00:00:00'),
(50,'ha_failover.log','/home/svsystems/var/',NULL,0,10,0,'0000-00-00 00:00:00'),
(51,'phpdebug.txt','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',1,10,0,'0000-00-00 00:00:00'),
(52,'perldebug.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',1,10,0,'0000-00-00 00:00:00'),
(53,'bpmtrace.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(54,'rx.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(55,'systemmonitor.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(63,'tmanager.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(64,'gentrends.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(65,'monitor.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(66,'monitor_disks.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(67,'monitor_ps.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(68,'monitor_protection.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(69,'volsync.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(70,'volsync1.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(71,'volsync2.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(72,'volsync3.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(73,'volsync4.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(74,'volsync5.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(75,'volsync6.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(76,'volsync7.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(77,'volsync8.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(78,'volsync9.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(79,'volsync10.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(80,'volsync11.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(81,'volsync12.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(82,'volsync13.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(83,'volsync14.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(84,'volsync15.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(85,'volsync16.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(86,'volsync17.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(87,'volsync18.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(88,'volsync19.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(89,'volsync20.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(90,'volsync21.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(91,'volsync22.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(92,'volsync23.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(93,'volsync24.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(94,'vxstub.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(95,'vxstub_get_initial_settings.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(96,'vxstub_get_application_settings.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(97,'vxstub_update_discovery_info.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(98,'vxstub_update_policy.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(99,'vxstub_update_cdp_information_v2.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(100,'vxstub_update_cdp_information.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(101,'vxstub_failover_commands_update.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(102,'vxstub_register_cluster_info.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(103,'configurator.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(104,'configurator_register_host_static_info.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(105,'configurator_register_host_dynamic_info.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(106,'configurator_register_host.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(107,'configurator_unregister_host.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(108,'fabric.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(109,'array.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(110,'prism.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(111,'psapitest.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,20,0,'0000-00-00 00:00:00'),
(112,'cxapi.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,20,0,'0000-00-00 00:00:00'),
(113,'file_download.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(114,'file_upload.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(115,'eventmanager.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(116,'rx_cx_call.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(117,'system_job.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(118,'esx.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(119,'schedulerapi.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(120,'apimanager.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,20,0,'0000-00-00 00:00:00'),
(121,'asrapi_ux_events.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,20,0,'0000-00-00 00:00:00'),
(122,'asrapi_reporting.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,20,0,'0000-00-00 00:00:00'),
(123,'authentication.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(124,'api_error.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,20,0,'0000-00-00 00:00:00'),
(125,'InMageAPILibrary.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,20,0,'0000-00-00 00:00:00'),
(126,'cspsconfigtool.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(127,'VCenter.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(128,'vxstub_update_agent_installation_status.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,20,0,'0000-00-00 00:00:00'),
(129,'clear_replication.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(130,'passphrase_update.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,10,0,'0000-00-00 00:00:00'),
(131,'asrapi_status.log','/home/svsystems/var/','CX_INSTALL_PATH_BACKSLASH\\home\\svsystems\\var\\',0,20,0,'0000-00-00 00:00:00');


--
-- Dumping data for table `logicalVolumes`
--
INSERT INTO `logicalVolumes` VALUES ('5C1DAEF0-9386-44a5-B92A-31FE2C79236A','P','',81920000,0,0,0,'0000-00-00 00:00:00','0000-00-00 00:00:00',0,'',0,0,0,0,0,0,0,0,0,0,0,0,0,0,'',NULL,'',NULL,NULL,0,'P',0,0,0,NULL,NULL,0,NULL,0,0,0,NULL,'1',0);


--
-- Dumping data for table `nextApplianceLunIdLunNumber`
--
INSERT INTO `nextApplianceLunIdLunNumber` VALUES (0,0,'inmage');



--
-- Dumping data for table `processServerDefaultTransportProtocol`
--
INSERT INTO `processServerDefaultTransportProtocol` VALUES ('linux',1,2),('windows',1,2);

--
-- Dumping data for table `protectionTemplate`
--
INSERT INTO `protectionTemplate` VALUES 
(1,'Replication Policy with compression (DR / Migration)','a:10:{s:13:\"processServer\";N;s:15:\"processServerId\";N;s:17:\"compressionEnable\";s:1:\"1\";s:20:\"ftpsSourceSecureMode\";i:0;s:18:\"ftpsDestSecureMode\";i:1;s:11:\"batchResync\";s:1:\"5\";s:15:\"rpoSLAThreshold\";s:2:\"30\";s:19:\"consistencyInterval\";s:2:\"15\";s:4:\"vacp\";s:11:\"Distributed\";s:9:\"retention\";a:8:{s:12:\"targetServer\";N;s:14:\"targetServerId\";N;s:16:\"targetServerCSId\";N;s:11:\"storagePath\";N;s:12:\"storageSpace\";s:0:\"\";s:20:\"storagePruningPolicy\";s:1:\"0\";s:14:\"alertThreshold\";s:2:\"80\";s:6:\"window\";a:5:{s:7:\"default\";a:4:{s:8:\"duration\";s:1:\"3\";s:16:\"timeIntervalUnit\";s:3:\"day\";s:10:\"filesystem\";s:11:\"File System\";s:15:\"filesystemCount\";s:1:\"1\";}s:4:\"hour\";N;s:3:\"day\";N;s:4:\"week\";N;s:5:\"month\";N;}}}');


--
-- Dumping data for table `recoveryStepsTemplate`
--

INSERT INTO `recoveryStepsTemplate` VALUES 
('AD_REPLICATION','Do AD replication','Target',5,'6','ALL','PLANNED or UNPLANNED or FAILBACK'),
('CHANGE_SPN','Change SPN entry','Target',5,'3','EXCHANGE or FILESERVER','PLANNED or UNPLANNED or FAILBACK'),
('CLUSTER_RECONSTRUCTION','Reconstructing cluster at failover site','Target',8,'1','ALL','ALL'),
('DNS_FAILOVER','Do DNS failover','Target',5,'5','ALL','PLANNED or UNPLANNED or FAILBACK'),
('EXPORT_FS_REGISTRY','Update failover server registry with share information','Target',5,'8','FILESERVER','PLANNED or UNPLANNED or FAILBACK'),
('FAST_FAILABACK_EXFAILOVER','Update active directory','Target',11,'1','EXCHANGE','FASTFAILBACK'),
('FAST_FAILBACK_DNS_FAILOVER','Do DNS failover','Target',12,'1','EXCHANGE or SQL','FASTFAILBACK'),
('FAST_FAILBACK_WINOP','Push DNS updates and  active directory changes across the domain','Target',14,'1','EXCHANGE or SQL','FASTFAILBACK'),
('FILESERVER_AD_REPLICATION','Do AD replication','Target',16,'5','FILESERVER','FASTFAILBACK'),
('FILESERVER_CHANGE_SPN','Change SPN entry','Source',15,'1','FILESERVER','FASTFAILBACK'),
('FILESERVER_DNS_FAILOVER','Do DNS failover','Target',16,'3','FILESERVER','FASTFAILBACK'),
('FILESERVER_START_APPLICATION','Start the application services','Target',16,'4','FILESERVER','FASTFAILBACK'),
('FILESERVER_UPDATE_NETBIOS','Update NetBIOS name of failover server','Source',15,'2','FILESERVER','FASTFAILBACK'),
('ISSUE_CONSISTENCY','Issue a consistency bookmark at the primary server','Source',2,'2','ALL','PLANNED_FAILOVER or FAILBACK'),
('MIGRATE_USERS','Migrate Exchange users','Target',5,'2','EXCHANGE','PLANNED or UNPLANNED or FAILBACK'),
('MOVE_PUBLIC_FOLDER','Move the public folder','Target',5,'4','EXCHANGE','ALL'),
('POSTSCRIPT_SOURCE','Post-script at primary server','Source',3,'','ALL','ALL'),
('POSTSCRIPT_TARGET','Post-script at failover server','Target',7,'','ALL','ALL'),
('PRESCRIPT_SOURCE','Pre-script at primary server','Source',1,'','ALL','ALL'),
('PRESCRIPT_TARGET','Pre-script at failover server','Target',4,'','ALL','ALL'),
('REPOINT_PRIVATE_STORES','Point all privates stores to the failed over  public store','Target',6,'1','EXCHANGE','ALL'),
('ROLLBACK_VOLUMES','1) Rollback all the application volumes <br> 2) Start the application','Target',5,'1','ALL','PLANNED or UNPLANNED or FAILBACK'),
('SCRIPT_AT_SOURCE','Custom script at primary server','Source',9,'','ALL','ALL'),
('SCRIPT_AT_TARGET','Custom script at failover server','Target',10,'','ALL','ALL'),
('SETUPAPPLICATIONENVIRONMENT','Setting up Application Environment','Target',5,'2','ORACLE or DB2','PLANNED or UNPLANNED or FAILBACK'),
('STARTAPPLICATION','Starting up Application','Target',5,'3','ORACLE or DB2','PLANNED or UNPLANNED or FAILBACK'),
('START_APPLICATION','Start Application Services','Target',5,'9','ALL','PLANNED or UNPLANNED or FAILBACK'),
('START_APPLICATION_ON_FAILBACK','Start Application Services','Target',13,'1','ALL','FASTFAILBACK'),
('STOP_APP_SERVICES','Stop application services for planned failover/failback','Source',2,'1','ALL','PLANNED_FAILOVER or FAILBACK'),
('UPDATE_NETBIOS','Update NetBIOS name of failover server','Target',5,'7','FILESERVER','PLANNED or UNPLANNED or FAILBACK');


--
-- Dumping data for table `ruleTemplate`
--

INSERT INTO `ruleTemplate` VALUES (1, 'Host Name', 'The Exchange server name should not match with any of the forbidden names', 'HA-DR', 'EXCHANGE', 'METADATA', 'SOURCE/TARGET', 'WARNING', 'N/A', 'CRITICAL', 'N/A', 'WARNING', 'N/A', 'CRITICAL'),
(2, 'Nested Mount Point Rule', 'Nested mount points should not be used.', 'HA-DR', 'SYSTEM', 'METADATA', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(3, 'Application Data Drives', 'Application data should not reside on the system drive.', 'HA-DR', 'SYSTEM', 'METADATA', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(4, 'Multiple IP Addresses', 'Checks if the server has more than one IP address due to the configuration of multiple NICs.', 'HA-DR', 'SYSTEM', 'SYSTEM', 'SOURCE/TARGET', 'WARNING', 'N/A', 'CRITICAL', 'N/A', 'WARNING', 'N/A', 'CRITICAL'),
(5, 'Local Continuous Replication (LCR)', 'LCR should not be enabled for any storage group on the primary Exchange server.', 'HA-DR', 'EXCHANGE2007', 'APPLICATION', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(6, 'Cluster Continuous Replication (CCR)', 'CCR should not be configured for the primary Exchange server.', 'HA-DR', 'EXCHANGE2007', 'APPLICATION', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(7, 'Application Version Match', 'Application versions at primary and secondary servers should match exactly.', 'HA-DR', 'COMMON', 'APPLICATION', 'TARGET', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(8, 'Administrative Group Match', 'The primary and secondary Exchange servers should belong to the same administrative group in the domain.', 'HA-DR', 'EXCHANGE', 'APPLICATION', 'TARGET', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(9, 'Domain Match', 'The primary and secondary Exchange servers should belong to the same domain.', 'HA-DR', 'COMMON', 'APPLICATION', 'TARGET', 'CRITICAL', 'N/A', 'CRITICAL', 'WARNING', 'CRITICAL', 'N/A', 'CRITICAL'),
(10, 'Required Services Running', 'Services required by Scout on the primary server should be running.', 'HA-DR', 'SYSTEM/COMMON', 'SYSTEM/APPLICATION', 'SOURCE', 'WARNING', 'WARNING', 'CRITICAL', 'SQLSQL_DB_STATUS_CHECK, SYSREM_DRIVE_DATA_CHECK, CONSISTENCY_TAG_CHECK', 'WARNING', 'WARNING', 'CRITICAL'),
(11, 'SQL Database Status', 'Primary server SQL databases requiring protection should be online.', 'HA-DR/BACKUP', 'SQL', 'APPLICATION', 'SOURCE', 'CRITICAL', 'CRITICAL', 'CRITICAL', 'SYSTEM_DRIVE_DATA_CHECK, CONSISTENCY_TAG_CHECK', 'CRITICAL', 'CRITICAL', 'CRITICAL'),
(12, 'Firewall Status Check', 'firewall status', 'HA-DR', 'SYSTEM', 'SYSTEM', 'SOURCE/TARGET', 'WARNING', 'N/A', 'WARNING', 'N/A', 'WARNING', 'N/A', 'WARNING'),
(13, 'Domain Name', 'Checking domain name', 'HA-DR', 'SYSTEM', 'SYSTEM', 'SOURCE/TARGET', 'WARNING', 'N/A', 'CRITICAL', 'N/A', 'WARNING', 'N/A', 'WARNING'),
(14, 'DNS Update Privilege', 'Validating User’s DNS Update Privileges', 'HA-DR', 'SYSTEM', 'SYSTEM', 'SOURCE/TARGET', 'WARNING', 'N/A', 'CRITICAL', 'N/A', 'WARNING', 'N/A', 'WARNING'),
(15, 'Active Directory (AD) Update Privilege ', 'Validating User’s AD Update Privileges', 'HA-DR', 'SYSTEM', 'SYSTEM', 'SOURCE/TARGET', 'WARNING', 'N/A', 'CRITICAL', 'N/A', 'WARNING', 'N/A', 'WARNING'),
(16, 'NAT IP', 'Validating NAT IP Status', 'HA-DR', 'SYSTEM', 'SYSTEM', 'SOURCE', 'WARNING', 'N/A', 'WARNING', 'N/A', 'WARNING', 'N/A', 'WARNING'),
(17, 'Ability to issue consistency bookmarks on application volumes', 'Checks if Scout can issue consistency bookmarks on application volumes', 'HA-DR/BACKUP', 'COMMON', 'METADATA', 'SOURCE', 'CRITICAL', 'CRITICAL', 'CRITICAL', 'N/A', 'CRITICAL', 'CRITICAL', 'CRITICAL'),
(18, 'Secondary Server Drive Sizes', 'Secondary server should have an equal or higher number of drives than the primary server. The secondary server drives corresponding to primary server drives should be of equal or higher size than the latter, and should have same drive letters. The', 'HA-DR/BACKUP', 'SYSTEM/COMMON', 'METADATA', 'TARGET', 'CRITICAL', 'CRITICAL', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(19, 'SQL Instance Root Directories', 'The root directories for SQL instances on primary and secondary SQL servers should be same.', 'HA-DR', 'SQL', 'METADATA', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(20, 'Stand-by Cluster Replication (SCR)', 'Stand-by Cluster Replication (SCR) should not be configured for the primary Exchange server.', 'HA-DR', 'EXCHANGE2007', 'APPLICATION', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(21, 'Dynamic DNS update', 'The Dynamic DNS Update property should be unchecked for all network interfaces (NICs) on primary server.', 'HA-DR', 'SYSTEM', 'SYSTEM', 'TARGET', 'WARNING', 'N/A', 'CRITICAL', 'N/A', 'WARNING', 'N/A', 'CRITICAL'),
(22, 'DNS Availability', 'DNS should be available.', 'HA-DR', 'SYSTEM', 'SYSTEM', 'TARGET', 'WARNING', 'N/A', 'CRITICAL', 'DNS_UPDATE_CHECK, AD_UPDATE_CHECK', 'WARNING', 'N/A', 'WARNING'),
(23, 'Domain Controller (DC) Availability', 'The DC should be available', 'HA-DR', 'SYSTEM', 'SYSTEM', 'TARGET', 'WARNING', 'N/A', 'CRITICAL', 'HOST_DC_CHECK', 'WARNING', 'N/A', 'WARNING'),
(24, 'Domain Controller Role on Primary/Secondary Server', 'The primary/secondary server should not be a DC.', 'HA-DR', 'SYSTEM', 'SYSTEM', 'TARGET', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'WARNING', 'N/A', 'WARNING'),
(25, 'SQL Instance Names', 'SQL instance names should be similar on primary and secondary SQL servers.', 'HA-DR', 'SQL', 'APPLICATION', 'TARGET', 'CRITICAL', 'N/A', 'CRITICAL', 'VERSION_MISMATCH_CHECK,SQL_DATAROOT_CHECK,VOLUME_CAPACITY_CHECK', 'CRITICAL', 'N/A', 'CRITICAL'),
(26, 'Single MailStore Copy', 'Checks if a mail store has a copy on another server in the DAG.', 'HA-DR', 'EXCHANGE', 'METADATA', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL'),
(27, 'SQL Server Install Path', 'The SQL Server installation volume on secondary server should not be used a secondary volume for replication.', 'HA-DR', 'SQL', 'APPLICATION', 'SOURCE/TARGET', 'WARNING', 'N/A', 'WARNING', 'N/A', 'WARNING', 'N/A', 'WARNING'),
(28, 'Cache Drive Free Space', 'Checks if the cache drive has enough free space', 'HA-DR/BACKUP', 'SYSTEM', 'SYSTEM', 'TARGET', 'WARNING', 'WARNING', 'WARNING', 'N/A', 'WARNING', 'WARNING', 'WARNING'),
(29, 'IP Comparison Check', 'Checks if primary and secondary servers have the same IP address', 'HA-DR', 'SYSTEM', 'SYSTEM', 'TARGET', 'WARING', 'N/A', 'CRITICAL', 'N/A', 'WARING', 'N/A', 'CRITICAL'),
(30,'TempDB Volume availability check','Checks if SQL temp db volume available at target','HA-DR','MSSQL','SYSTEM','TARGET','CRITICAL','N/A','CRITICAL','N/A','CRITICAL','N/A','CRITICAL'),
(31, 'VSS Rollup Check', 'Checks if VSS rollup version matches with last working version', 'HA-DR/BACKUP', 'SYSTEM', 'SYSTEM', 'SOURCE', 'CRITICAL', 'CRITICAL', 'CRITICAL', 'N/A', 'CRITICAL', 'CRITICAL', 'CRITICAL'),
(32, 'Page File Volume Check', 'Check application volumes contains page file configuration', 'HA-DR', 'SYSTEM', 'APPLICATION', 'SOURCE', 'WARING', 'N/A', 'CRITICAL', 'N/A', 'WARING', 'N/A', 'CRITICAL'),
(33, 'Database Installation Verification Check', 'Checking whether Oracle database is installed or not', 'HA-DR', 'ORACLE', 'APPLICATION', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'CRITICAL', 'CRITICAL'),
(34, 'Appliance target DUMMY_LUN_ZERO check', ' The host should see DUMMY_LUN_ZERO for setting up mirroring', 'HA-DR/BACKUP', 'BULK/ORACLE', 'APPLICATION', 'SOURCE', 'N/A', 'N/A', 'N/A', 'N/A', 'CRITICAL', 'CRITICAL', 'CRITICAL'),
(35, 'Oracle database shutdown check', 'Verify if secondary server has any database running on the secondary volumes', 'HA-DR/BACKUP', 'ORACLE', 'APPLICATION', 'TARGET', 'WARNING', 'WARNING', 'WARNING', 'N/A', 'N/A', 'N/A', 'N/A'),
(36, 'Secondary server volume busy check', 'Verify if secondary server volumes are in use by filesystems or volume managers', 'HA-DR/BACKUP', 'SYSTEM/COMMON', 'SYSTEM', 'TARGET', 'WARNING', 'CRITICAL', 'WARNING', 'N/A', 'N/A', 'N/A', 'N/A'),
(37, 'Secondary server configuration check', 'Verify if secondary server supports the primary server Oracle configurations', 'HA-DR', 'ORACLE', 'APPLICATION', 'TARGET', 'CRITICAL', 'CRITICAL', 'CRITICAL', 'N/A', 'N/A', 'N/A', 'N/A'),
(38, 'Cache Memory Minimum Space Per Pair Check', 'Cache memory should have minimum space per pair', 'HA-DR/BACKUP', 'SYSTEM', 'SYSTEM', 'TARGET', 'CRITICAL', 'CRITICAL', 'CRITICAL', 'N/A', 'N/A', 'N/A', 'N/A'),
(39, 'DB2 Instance and Database Installation Verification', 'Checks whether DB2 instance is installed and running or not and DB2 database is active or not', 'HA-DR', 'DB2', 'APPLICATION', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'CRITICAL', 'CRITICAL'),
(40, 'DB2 Database Shutdown Verification', 'Checks whether DB2 database is deactivated or not', 'HA-DR', 'DB2', 'APPLICATION', 'TARGET', 'WARNING', 'WARNING', 'WARNING', 'N/A', 'N/A', 'N/A', 'N/A'),
(41, 'Secondary server configuration check for DB2', 'Verify if secondary server supports the primary server DB2 configurations', 'HA-DR', 'DB2', 'APPLICATION', 'TARGET', 'CRITICAL', 'CRITICAL', 'CRITICAL', 'N/A', 'N/A', 'N/A', 'N/A'),
(42, 'DB2 Consistency Tag On Volumes Rule', 'Checks whether a Tag can be issued on database Volumes.', 'HA-DR', 'DB2', 'APPLICATION', 'SOURCE', 'CRITICAL', 'N/A', 'CRITICAL', 'N/A', 'CRITICAL', 'CRITICAL', 'CRITICAL');

--
-- Dumping data for table `rules`
--
INSERT INTO `rules` VALUES (0,1,'ALL',1,'0000-00-00','0000-00-00','0000-00-00','ALLDAY','00:00:00','00:00:00');



--
-- Dumping data for table `rxSettings`
--
INSERT INTO `rxSettings` VALUES 
('HOST_NAME',''),
('IP_ADDRESS',''),
('HTTP_PORT',''),
('VERSION_TAG',''),
('RX_KEY',''),
('RESPONSE_INTERVAL','900'),
('RESPONSE_TYPE',''),
('LAST_RESPONSE_TIME',''),
('RRD_SYNCHRONISED_TIME',''),
('ALIAS_NAME',''),
('SECURED_COMMUNICATION',''),
('RX_COMPATIBILITY_NUMBER',''),
('AUTO_UPDATE_CX','1'),
('UPDATE_DOWNLOAD_URL',''),
('UPDATE_STATUS','0'),
('DOWNLOAD_FILE_NAME',''),
('DOWNLOAD_FILE_CHECKSUM',''),
('CX_COMPATIBILITY_NUMBER',''),
('AUTO_UPDATE_CHECK_INTERVAL','3600'),
('LAST_AUTO_UPDATE_CHECK_TIME',''),
('RRD_SYNC_HOSTS',''),
('SYNC_WITH_RX_NOW','0'),
('RX_CS_IP',''),
('RX_CS_PORT',''),
('RX_CS_SECURED_COMMUNICATION',''),
('CUSOTMER_ACCOUNT_ID',''),
('CUSTOMER_REFERENCE_ID','');

-- 
-- Dumping data for table `sanFabrics`
-- 
INSERT INTO `sanFabrics` (`fabricId`, `name`, `zoneSetName`, `state`) VALUES 
('0', 'generic_fabric', 'generic_fabric', 0),
('1', 'default_fabric', 'default_fabric', 0);

--
-- Dumping data for table `systemHealth`
--
INSERT INTO `systemHealth` VALUES ('Space Constraint','healthy','0000-00-00 00:00:00');


--
-- Dumping data for table `transbandSettings`
--
INSERT INTO `transbandSettings` VALUES (1,'ACTIVE POLICY','',''),(2,'DISK_SPACE_WARN_THRESHOLD','80',''),(3,'PATH','CX_INSTALL_PATH_FORWARDSLASH/home/svsystems',''),(4,'Microsoft Exchange','Exchange','ACM'),(5,'SQL Server 2000','SQL','ACM'),(6,'SQL Server 2005','Sql2005','ACM'),(7,'Registry','Registry','ACM'),(8,'System','SystemFiles','ACM'),(9,'Event Log','EventLog','ACM'),(10,'WMI','WMI','ACM'),(11,'IIS Metabase','IISMETABASE','ACM'),(12,'COM+ REGDB','COM+REGDB','ACM'),(13,'Certificate Authority','CA','ACM'),(14,'Active Directory','AD','ACM'),(15,'Dhcp Jet','DHCP','ACM'),(16,'BITS','BITS','ACM'),(17,'WINS Jet','WINS','ACM'),(18,'Removable Storage Manager','RSM','ACM'),(19,'TermServLicensing','TSL','ACM'),(20,'FRS','FRS','ACM'),(21,'File System','FS','ACM'),(22,'Microsoft (Bootable State)','BootableState','ACM'),(23,'Microsoft (Service State)','ServiceState','ACM'),(24,'Cluster Service','ClusterService','ACM'),(25,'User Defined','USERDEFINED','ACM'),(26,'disktotalspace','10735918240',''),(27,'diskfreespace','2147183648',''),(28,'isEnabledLogRotation','1','LogRotation'),(29,'logRotationTimeInt','daily','LogRotation'),(30,'logRotationBkUp','2','LogRotation'),(31,'RETENTION_RESERVE_SPACE','268435456',''),(33,'EMAIL_SERVER','127.0.0.1','mailSettings'),(34,'CX_SERVER_NAME','CX','mailSettings'),(35,'MIRRORLUN_PORTWWN','1',''),(36,'VI_PORTWWN','1',''),(37,'VT_PORTWWN','1',''),(38,'VTB_PORTWWN','1',''),(39,'MIRRORLUN_NODEWWN','2100000000000000',''),(40,'manual_discovery_processing_in_progress','0',''),(41,'manual_discovery_processing_needed','0',''),(42,'GENTRENDS_LAST_PROCESSING_TIME','',''),(43,'EMAIL_SERVER_DOMAIN','localhost.localdomain','mailSettings'),(44,'NTFS_AWARE_RESYNC','Y','Replication'),(45,'DISPLAY_APP_VOLUMES','0',''),(46,'trapDispatchInterval','60',''),(47,'lastTrapDispatchTime','',''),(48,'NEXTTMID','1',''),(49,'FROM_EMAIL','CX@localhost.localdomain','mailSettings'),(50,'HTTPS_PORT','443',''),(51,'CONNECTIVITY_TYPE','NON VPN',''),(52,'MDS_LOG_LIMIT','50',''),(53,'CLEANUP_TIME_LIMIT','7776000','CLEANUP_DATA'),(54,'CLEANUP_RECORD_LIMIT','10000','CLEANUP_DATA'),(55,'MDSAlertSyncCounter','0',''),(56,'ALERTS_MAX_TOKEN_ID','0',''),(57,'SSL_ACKNOWLEDGEMENT','1','');


--
-- Dumping data for table `transportProtocol`
--
INSERT INTO `transportProtocol` VALUES ('FTP',0),('HTTP',1),('FILE',2);

--
-- Dumping data for table `vendorInfo`
--

INSERT INTO `vendorInfo` VALUES 
(0,'Unknown Vendor'),
(1,'Native'),
(2,'Devmapper'),
(3,'Mpxio'),
(4,'Emc'),
(5,'Hdlm'),
(6,'Devdid'),
(7,'Devglobal'),
(8,'Vxdmp'),
(9,'Svm'),
(10,'Vxvm'),
(11,'Lvm'),
(12,'Inmvolpack'),
(13,'Inmvsnap'),
(14,'Customevendor'),
(15,'Asm'),
(16,'Vcs');


--
-- Dumping data for table `volumeType`
--

INSERT INTO `volumeType` VALUES 
(0,'Physical Disk',1,1,1,1,0,1,0,1),
(1,'Vsnap Mounted',0,0,0,0,1,0,1,0),
(2,'Vsnap UnMounted',0,0,0,0,1,0,1,0),
(3,'Unknown',0,0,0,0,0,0,0,0),
(4,'Partition',1,1,1,1,0,1,0,1),
(5,'Logical Volume',1,1,1,1,0,1,0,1),
(6,'VolPack',0,1,0,0,0,0,0,0),
(7,'Custom Disk',1,0,0,0,0,0,0,0),
(8,'Extended Partition',0,0,0,0,0,0,0,0),
(9,'Disk Partition',1,1,1,1,0,1,0,1);

--
-- Dumping data for table `ipType`
--

INSERT INTO `ipType`(type) VALUES('physical'),('unknown');

--
-- Dumping data for table `eventCodes`
--

INSERT INTO `eventCodes` (`errCode`, `apiCode`, `errType`, `errorMsg`, `errPossibleCauses`, `errCorrectiveAction`, `errComponent`, `errManagementFlag`, `errInUse`, `isEvent`, `category`, `placeHolders`, `suppressTime`) VALUES
('AA0305',0,'VM health','','','Please extend the retention drive to increase its capacity or free up the space by deleting unwanted files or stale retention files, if any. Otherwise, re-protect the source server with lower retention window or to a different Master Target server.','MT',3,'Yes',1,'Alerts/Events','', 86400),
('AC0010',0,'','Install of the mobility service to the source machine <SourceIP> failed.','Installation of the the mobility service is already in progress.','Please wait for some time and retry the operation.','',0,'Yes',0,'PushInstall','SourceIP => 10.0.1.152 (string)', 86400),
('EA0401',0,'Agent health','','','Make additional space available, configure a different cache directory path. Create new disk or move cache directory to a different volume. 1. Stop services 2. Move the Cache Directory to the new location 3. Use the Agent config wizard to change the config location. Start services.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0402',0,'Agent health','','','Make Sure retention volume exists and proper permissions are available. If antivirus is installed, ensure that the Mobility Service is excluded from the Antivirus scan.','MT',3,'Yes',0,'Alerts/Events','', 86400),
('EA0406',0,'Agent health','Differential file : <DiffFileName> contains an IO change beyond source volume capacity.\r\nPlease do a resync on <HostName>(<IPAddress>)','','Contact support to find root cause.','Agent',3,'Yes',0,'Alerts/Events','DiffFileName, HostName,  IPAddress', 86400),
('EA0408',0,'Agent health','','','If these alerts are continuous, contact support to find root cause.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0409',0,'Agent health','','','Contact support to find root cause.','MT',3,'Yes',0,'Alerts/Events','', 86400),
('EA0411',0,'Agent health','','','1. Ensure that the network connectivity exists between the mobility service on the master target server <HostName> and the process server.\r\n2. Ensure the following services are running on the master target server - InMage Scout Application Service, InMage Scout VX Agent Sentinel/Outpost(svagents), InMage Scout FX Agent(frsvc) and cxprocessserver.\r\n3. Ensure the following services are running on the process server - cxprocessserver and tmansvc.\r\n4. Add cxprocesserver service to the firewall exceptions. Under Windows Firewall settings, select the option "Allow an app or feature through Firewall" and add "<SystemDrive>\home\svsystems\transport\csps.exe" using "add another app" option.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0427',0,'Agent health','','','1. Ensure that the network connectivity exists between the mobility service on the source <HostName> and the process server.\r\n2. Ensure the following services are running on the source - InMage Scout Application Service, InMage Scout VX Agent Sentinel/Outpost(svagents), InMage Scout FX Agent(frsvc).','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0428',0,'Agent health','Replication for the source machine %SourceName is not progressing due to an authentication issue.','Registration of the Configuration server to the Recovery services vault is incomplete.','Perform the following actions in the specified sequence:\r\n1. Register the Configuration server %CSName to the Recovery services vault by running the cspsconfig utility ([Install Location]\\home\\svsystems\\bin\\cspsconfigtool).\r\n2. If you have additional process servers deployed, run the cdpcli utility ([Install Location]\\agent>cdpcli.exe) on the process server as follows: \"cdpcli -registermt\".\r\n3. Restart the \"Microsoft Azure Recovery Services Agent\" and \"InMage Scout VX Agent - Sentinel/Outpost\" services on the process server managing the replication for the source machine %SourceName.','Agent',3,'Yes',0,'Alerts/Events','CSName\r\SourceName\r\n', 86400),
('EA0429',0,'Agent health','Consistency failed on the source machine.','Failed to issue a recovery tag as replication for the disk is in non-data mode.','If this event occurs very frequently for a replicating machine, it may be indicative of insufficient network bandwidth or compute capacity to process the change rate at the source.','Agent',3,'Yes',0,'Alerts/Events','CSName\r\SourceName\r\n', 900),
('EA0430',0,'Agent health','Consistency failed on the source machine.','Failed to issue a recovery tag.','This may be a transient failure. Check back after sometime.','Agent',3,'Yes',0,'Alerts/Events','CSName\r\SourceName\r\n', 900),
('EA0431',0,'Agent health','App-consistent recovery point tagging failed ','App-consistent recovery tag generation for virtual  machines has failed due to errors while generating the tag on the disks','This may be a transient failure. Repeated and multiple app-consistent tag generation failures on a replicating machine may be indicative of other issues, such as VSS snapshot failures on Windows due to multiple VSS applications issuing snapshot requests.','Agent',3,'Yes',0,'Alerts/Events','CSName\r\SourceName\r\n', 900),
('EA0432',0,'Agent health','Critical error in replication','Replication health of virtual machine is critical as the target storage account or log storage account is not accessible.','','Agent',3,'Yes',0,'Alerts/Events','CSName\r\SourceName\r\n', 86400),
('EA0433',0,'Agent health','Critical error in replication','Replication health of virtual machine is critical.','','Agent',3,'Yes',0,'Alerts/Events','CSName\r\SourceName\r\n', 86400),
('EA0434',0,'Agent health','Server <HostName> has come up after a reboot or shutdown.','-','-','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0435',0,'Agent health','Mobility service upgraded to version <AgentVersion> on server <HostName>.','-','-','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0412',0,'Agent health','','','Make sure the volume is online and readable. Below are some helpful checks: On unix: 1. Use dd in read mode to see if volume is readable. 2. Check /var/log/messages to see if any volume related errors are present. On windows: 1. Use chkdsk in read only mode to see if it reports any errors. 2. Check eventvwr logs to see if any volume related errors are present. Verify backend luns (if storage is from luns).','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0414',0,'Agent health','','','Make sure the volume is online and readable. Below are some helpful checks: On unix: 1. Use dd in read mode to see if volume is readable. 2. Check /var/log/messages to see if any volume related errors are present. On windows: 1. Use chkdsk in read only mode to see if it reports any errors. 2. Check eventvwr logs to see if any volume related errors are present. Verify backend luns (if storage is from luns).','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0415',0,'Agent health','','','Make sure the volume is online and readable. Below are some helpful checks: 1. Use chkdsk in read only mode to see if it reports any errors. 2. Check eventvwr logs to see if any volume related errors are present. Verify backend luns (if storage is from luns).','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0420',0,'Agent health','','','Make sure the volume is online and readable. Below are some helpful checks: On unix: 1. Use dd in read mode to see if volume is readable. 2. Check /var/log/messages to see if any volume related errors are present. 3. Ensure that no other process is using this volume. On windows: 1. Use chkdsk in read only mode to see if it reports any errors. 2. Check eventvwr logs to see if any volume related errors are present. 3. Ensure that no other process is using this volume. Verify backend luns (if storage is from luns).','MT',3,'Yes',0,'Alerts/Events','', 86400),
('EA0421',0,'Agent health','','','Check the disk sub system for any errors. Reboot of server may solve this.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0501',0,'','Protection could not be enabled.','Enable protection failed  at the master target server <MTHostName> with the error code <ErrorCode>.','\r\nEnsure that the drscout.conf file exists at <InstalledPath>Application Dataetc on the master target server and has the appropriate write permissions. Restart the mobility services (InMage Scout Application Service, InMage Scout VX Agent - Sentinel/Outpost, cxprocessserver) on the master target server and retry the operation.\r\n\r\n      If there are existing replications, please contact Microsoft support.\r\n\r\n      If there are no existing replications, disable the protection and manually uninstall the mobility service. Retry the operation.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\nInstalledPath\r\n', 86400),
('EA0502',0,'','Protection could not be enabled.','Unable to find the mobility service installation path on the master target server <MTHostName>. Operation failed with error code <ErrorCode>.','\r\n      Ensure that the Key <RegistryKey> exists and has a valid installation path for the mobility service on the master target server <MTHostName>. Restart the mobility services (InMage Scout Application Service, InMage Scout VX Agent - Sentinel/Outpost, cxprocessserver) on the master target server and retry the operation.\r\n\r\n      If there are existing replications, please contact Microsoft support.\r\n\r\n      If the <RegisteryKey> does not exist  and there are no existing replications, disable the protection and manually uninstall the mobility service. Retry the operation.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\nRegistryKey\r\nMTHostName\r\nRegisteryKey\r\n', 86400),
('EA0503',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with error code <ErrorCode>.','Disable the protection and re-enable. If the issue persists, please contact Microsoft Support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\n', 86400),
('EA0504',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with error code <ErrorCode>.','Ensure that the configuration server is upgraded to the latest version. Disable the protection and retry the operation. If the issue persists, please contact Microsoft Support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\n', 86400),
('EA0505',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with error code <ErrorCode>.','Disable the protection, manually uninstall the mobility service for source machine (<SourceIP>) and retry the operation.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\nSourceIP\r\n', 86400),
('EA0506',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with error code <ErrorCode>.','Ensure that the configuration server  <CsIP> is accessible from the master target server <MTHostName> (<MTIP>).    Resolve the issue, disable the protection and retry the operation. ','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\nCsIP\r\nMTHostName\r\nMTIP\r\n', 86400),
('EA0507',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with error code <ErrorCode>.','Ensure that the source machine  <SourceIP> and the master target server <MTHostName> are running the latest version of the mobility Service. If either of them is not running the latest version, disable the protection, manually uninstall the mobility service and retry the operation.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\nSourceIP\r\nMTHostName\r\n', 86400),
('EA0508',0,'','Protection could not be enabled.','Enable protection failed on the master target server <MTHostName> with error code <ErrorCode> while detecting the newly added disks.','Disable the protection and restart the master target server <MTHostName>.  Retry the operation. If the issue persists, please contact the Microsoft support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\nMTHostName\r\n', 86400),
('EA0509',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with error code <ErrorCode>.','Disable the protection and retry the operation. If the issue persists, please contact Microsoft support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\n', 86400),
('EA0510',0,'','Protection could not be enabled.','Enable protection failed with error code <ErrorCode> at the master target server <MTHostName>. Disk <Diskname> is not accessible or is locked by a different process. ','Ensure the disk <DiskName> status is healthy, is accessible from the master target server and is not locked by any process . Clear SCSI reservations, if any, on the disk. Disable the protection and retry the operation. ','Agent',0,'Yes',0,'Protection','ErrorCode\r\nMTHostName\r\nDiskname\r\nDiskName\r\n', 86400),
('EA0511',0,'','Protection could not be enabled.','Enable protection failed with error code <ErrorCode> at the master target server <MTHostName> while enumerating the volumes associated. ','Ensure that all the volumes are accessible. To verify, use tools like mountvol or diskpart on the master target server to ensure the volumes can be discovered without any errors. Retry the operation after ensuring the above recommendations. If the issue persists, restart the master target server. ','Agent',0,'Yes',0,'Protection','ErrorCode\r\nMTHostName\r\n', 86400),
('EA0512',0,'','Protection could not be enabled.','Enable protection failed with error code <ErrorCode> at the master target server <MTHostName> (<MTIP>).','Disable the protection for the source machine  <SourceIP> and retry the operation. If the issue persists, please contact Microsoft support.','Agent',0,'Yes',0,'Protection','ErrorCode\r\nMTHostName\r\nMTIP\r\nSourceIP\r\n', 86400),
('EA0513',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with error code <ErrorCode>.','Restart VDS service on the master target server <MTHostName> (<MTIP>). Disable the protection and retry the operation. ','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\nMTHostName\r\nMTIP\r\n', 86400),
('EA0514',0,'','Protection could not be enabled.','Enable protection failed on the master target server <MTHostName> (<MTIP>) with error code <ErrorCode> while enumerating the newly added disks.','Disable the protection and restart the master target server <MTHostName> (<MTIP>).  Retry the operation. If the issue persists, please contact Microsoft support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nMTIP\r\nErrorCode\r\nMTHostName\r\nMTIP\r\n', 86400),
('EA0515',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with the error code <ErrorCode>.','Ensure the source mount points on the source machine  <SourceIP> are in English. Disable the protection and retry the operation. If the issue persists, please contact Microsoft support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\nSourceIP\r\n', 86400),
('EA0516',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with the error code <ErrorCode>.','Disable the protection for the source and retry the operation. If the issue persists, please contact Microsoft support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\n', 86400),
('EA0517',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> while preparing the target and  failed with error code <ErrorCode>.','Disable the protection for the source and retry the operation. If the issue persists, please contact Microsoft support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nErrorCode\r\n', 86400),
('EA0518',0,'','Protection could not be enabled.',' Enable protection failed on the master target server <MTHostName> (<MTIP>) with error code <ErrorCode>. Either the disks are not attached or not identified by the master target server or the LUN information passed is incorrect. ','Ensure that the disks are properly attached to the master target server and reboot the master target server. If the issue persists, disable the protection and retry the operation. ','Agent',0,'Yes',0,'Protection','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0519',0,'','Protection could not be enabled.','Enable protection failed on the master target server <MTHostName> (<MTIP>) with error code <ErrorCode>. There might be some required RPMs missing on the master target server. ','Install required RPMs by following the Linux Master Target preparation guide at http://go.microsoft.com/fwlink/?LinkId=613319 .','Agent',0,'Yes',0,'Protection','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0520',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName> (<MTIP>) while preparing the target and  failed with error code <ErrorCode>.','Disable the protection and retry the operation. If the issue persists, please contact Microsoft support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0521',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) while preparing the target and  failed with error code <ErrorCode>.','Disable the protection and retry the operation. If the issue persists, please contact Microsoft support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0522',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) while preparing the target and  failed with error code <ErrorCode>.','Delete the folders under the directory <DirectoryPath> on the master target server <MTHostName> (<MTIP>).  Disable the protection and retry the operation. If the issue persists, please contact Microsoft Support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nMTIP\r\nErrorCode\r\nMTHostName\r\nMTIP\r\n', 86400),
('EA0523',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) while preparing the target and  failed with error code <ErrorCode>.','If there is any Anti-Virus running, add an exclusion for Microsoft Azure Site Recovery installation folder in the Anti-Virus rules. Disable the protection and retry the operation. If the issue persists, please contact Microsoft Support.  ','Agent',0,'Yes',0,'Protection','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0524',0,'','Protection could not be enabled.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) while preparing the target and  failed with error code <ErrorCode>.','If there is any Anti-Virus running, add an exclusion for Microsoft Azure Site Recovery installation folder in the Anti-Virus rules. Disable the protection and retry the operation. If the issue persists, please contact Microsoft Support.','Agent',0,'Yes',0,'Protection','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0525',0,'','Protection could not be enabled.','Enable protection failed on the master target server <MTHostName> (<MTIP>) with error code <ErrorCode>. Either the master target server is rebooted or the services are manually stopped. ','Restart the AppService service on the master target server <MTHostName>.  Disable the protection and retry the operation.','Agent',0,'Yes',0,'Protection','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0526',0,'','Protection could not be enabled.','Volume enumeration for <VolumeList> of the source machine <SourceIP> failed with error code <ErrorCode>. One or more of these system volumes/mount points are invalid or the user does not have sufficient privileges to access these volumes.','Ensure that the all the volumes are valid and that the user has sufficient privileges (system user) to access the volumes.','Agent',0,'Yes',0,'Protection','VolumeList\r\SourceIP\r\ErrorCode\r\n', 86400),
('EA0601',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with error code <ErrorCode>.','Retry the failover operation. If issue persists contact Microsoft support.  ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0602',0,'','Failover failed.','An internal error occurred with the  master target server <MTHostName>  (<MTIP>) during recovery and  failed with error code <ErrorCode>.','Retry the failover operation. If issue persists contact Microsoft support. ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0603',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with error code <ErrorCode>.','Please contact Microsoft support for guidance on recovering the protected source.    ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0604',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with error code <ErrorCode>. Unable to find the common consistency recovery point across volumes of the protected source. ','Select a different recovery point and retry the failover operation. ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0605',0,'','Failover failed.','Recovery on the master target server <MTHostName> (<MTIP>) failed with the error code <ErrorCode>. Retention volume is not accessible. ','Ensure that the retention volume is accessible and retry the operation. If this issue persists, contact Microsoft Support.  ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0606',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with the error code <ErrorCode>.','Retry the failover operation after sometime. If issue persists, contact Microsoft support.  ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0607',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with the error code <ErrorCode>.','Retry the failover operation after sometime. If issue persists contact support. ','Agent',0,'Yes',0,'Recovery','ErrorCode\r\n', 86400),
('EA0608',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with the error code <ErrorCode>.','Please contact support for guidance on recovering this virtual machine.   ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0609',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with the error code <ErrorCode>.','Please contact Microsoft support for guidance on recovering this virtual machine.   ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0610',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with error code <ErrorCode>.','Please retry the operation. If the issue persists, please contact Support.','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0611',0,'','Failover failed.','Failover failed at the master target server <MTHostName>  (<MTIP>) with the error code <ErrorCode>. Either the master target server is down or service is manually stopped. ','Restart the Appservice service on the master target server <MTHostName> and retry the operation.  ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\nMTHostName\r\n', 86400),
('EA0612',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with error code <ErrorCode>.','If there is any Anti-Virus running, add an exclusion for the folder Microsoft Azure Site Recovery installation folder  in the Anti-Virus rules. Disable the protection and retry the operation. If the issue persists, please contact Microsoft Support.','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0613',0,'','Failover failed.','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with error code <ErrorCode>.','If there is any Anti-Virus running, add an exclusion for the Microsoft Azure Site Recovery installation folder in the Anti-Virus rules. Disable the protection and retry the operation. If the issue persists, please contact Microsoft Support.  ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EA0614',0,'','Failover operation failed. ','An internal error occurred with the master target server <MTHostName>  (<MTIP>) during recovery and  failed with error code <ErrorCode>.','Please retry the operation. If the issue persists, please contact Microsoft Support. ','Agent',0,'Yes',0,'Recovery','MTHostName\r\nMTIP\r\nErrorCode\r\n', 86400),
('EC0101',0,'VM health','','','Ensure that the replication is not stopped and source, process server, Target are available, connected to network. Also, ensure that sufficient bandwidth is provisioned for replication. Volume replication will progress automatically if the target is available.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0103',0,'VM health','','','Check the network connectivity between source and the PS server. Ensure that Source Mobility agent service and the transport service are running. Contact support if issue persists.','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0104',0,'VM health','','','Restart the resync for the replication pair. Contact support if the resync required flag is set frequently even without a obvious reason.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0105',0,'Agent health','','','Start agent VX agent service if stopped. Check network connectivity. Contact support if issue persists.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0108',0,'PS health','','','Start the InMage application agent service on the mentioned host. Ignore the trap if you are using vContinuum for protection.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0109',0,'PS health','','','Check the network connectivity between CS and the PS server. Boot the PS server if it is shutdown. Ensure that the managed service is running on the PS server. Contact support if issue persists.','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0110',0,'VM health','','','Check if there is enough bandwidth to drain the resync files. You can choose to increase the threshold valule if there is enough free space on the cache disk on PS server.','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0111',0,'VM health','','','Check if there is enough bandwidth to drain the differential files. You can choose to increase the threshold valule if there is enough free space on the cache disk on PS server which will ensure the driver to be in data mode.','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0112',0,'PS health','','','Check if there is enough bandwidth and check the network connectivity between PS and target server. Increase the disk space on the PS server / or \"/var/log\" or \"/home\" partitions','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0113',0,'PS health','','','Check the storage and fix the issue with the storage.','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0114',0,'PS health','Server <HostName> has come up after a reboot or shutdown.','-','-','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0115',0,'PS health','PS upgraded to version <PSVersion> on server <HostName>.','-','-','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0116',0,'VM health','','','Please contact support for investigation.','Agent',4,'Yes',1,'Alerts/Events','', 86400),
('EC0117',0,'VM health','','','Please contact support for investigation.','Agent',4,'Yes',1,'Alerts/Events','', 86400),
('EC0118',0,'VM health','','','Please contact support for investigation.','Agent',4,'Yes',1,'Alerts/Events','', 86400),
('EC0121',0,'VM health','','','Extend the disk to increase its size or free up the space on volume by deleting unwanted files or stale retention files (if any).','MT',3,'Yes',0,'Alerts/Events','', 86400),
('EC0122',0,'VM health','','','Informative message about the resize operation and CX rescan operation.','Agent',3,'Yes',1,'Alerts/Events','', 86400),
('EC0123',0,'VM health','','','Please resize your target LUN to greater than or equal to source LUN and resume the replication pair (Not applicable for profiler targets).','Agent',3,'Yes',1,'Alerts/Events','', 86400),
('EC0124',0,'VM health','','','Please resize your target volume (<DestVolume>) to greater than or equal to source volume and resume the replication pair.','MT',3,'Yes',1,'Alerts/Events','', 86400),
('EC0125',0,'VM health','','','To account for the change in disk size, resync has been triggered automatically.','CS',3,'Yes',1,'Alerts/Events','', 86400),
('EC0126',0,'VM health','','','Resync would be automatically triggered after sometime. If you want to trigger the resync immediately, select the resynchronization option for the virtual machine with the resized disk under Protected Items > Machines > Properties.','CS',3,'Yes',1,'Alerts/Events','', 86400),
('EC0127',0,'VM health','Resync progress stuck','Replication for virtual machine has not progressed','Connectivity issues between the process server and the target storage account or master target server.The Azure subscription of the target storage account has been disabled.Transaction limits were hit on the target storage account.','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0128',0,'VM health','','','Please upgrade the Mobility Service to the latest version supported by the configuration server.','CS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0129',0,'VM health','Resync progress stuck','Replication for virtual machine has not progressed','Connectivity issues between the process server and the target storage account or master target server.The Azure subscription of the target storage account has been disabled.Transaction limits were hit on the target storage account.','PS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0130',0,'PS health','','','Ensure that the Mobility Services (InMage Scout VX Agent - Sentinel/Outpost, InMage Scout Application Service) are running on <HostName> and that the host is connected to network.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0131',0,'Agent health','','','Ensure that the Mobility Services (InMage Scout VX Agent - Sentinel/Outpost, InMage Scout Application Service) are running on <HostName> and that the host is connected to network.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0132',0,'VM health','','','Please upgrade the Process Server to the latest version supported by the configuration server.','CS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0134',0,'VM health','','','Please upgrade the Mobility Service to the latest version supported by the configuration server.','CS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0135',0,'VM health','','','Please upgrade the Mobility Service and the Process Server to the latest version supported by the configuration server.','CS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0136',0,'VM health','','','Please upgrade the Mobility Service and the Process Server to the latest version supported by the configuration server.','CS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0137',0,'VM health','','','Please upgrade the Mobility Service and the Process Server to the latest version supported by the configuration server.','CS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0138',0,'VM health','','','Please check if the reported volumes are online and accessible. Also, check if they were mounted earlier and now have been unmounted.','CS',3,'Yes',0,'Alerts/Events','', 86400),
('EC0139',0,'Agent health','','','Ensure that the Mobility Services (InMage Scout VX Agent - Sentinel/Outpost, InMage Scout FX Agent) are running on <HostName> and that the host is connected to network.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0140',0,'Agent health','','','Ensure that the Mobility Services (InMage Scout VX Agent - Sentinel/Outpost, InMage Scout FX Agent, InMage Scout Application Service) are running on <HostName> and that the host is connected to network.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0141',0,'Agent health','','','Ensure that the Mobility Services (InMage Scout FX Agent, InMage Scout Application Service) are running on <HostName> and that the host is connected to network.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0146',0,'VM health','','','Ensure that Application Agents are running on all the protected VMs in the plan. If this issue persists, contact Support.','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EC0401',0,'','Discovery operation failed.  ','Discovery of the vCenter server <vCenterIP>:<vCenterPort> failed with error code  <ErrorCode>. The vCenter server is not accessible from the process server <PsName> (<PsIP>)','\r\n      1. Ensure that the vCenter server is connected to the network. This can be verified by connecting to the vCenter Server using the vCenter client.\r\n      2.Ensure the vCenter server is accessible by the process server.\r\n\r\n      In addition, ensure the following for discovery operations to complete.\r\n      1. Ensure the login credentials provided for the vCenter server is correct.\r\n    See http://go.microsoft.com/fwlink/?LinkId=613315 for guidance on discovery.\r\n    ','PS',0,'Yes',0,'Discovery','vCenterIP\r\nvCenterPort\r\nErrorCode\r\nPsName\r\nPsIP\r\n', 86400),
('EC0402',0,'','Discovery operation failed. ','Discovery of the vCenter server <vCenterIP>:<vCenterPort> failed with the error code  <ErrorCode> due to invalid login credentials. ','Ensure the login credentials provided for the vCenter server is correct.  See http://go.microsoft.com/fwlink/?LinkId=613315 for guidance on discovery.','PS',0,'Yes',0,'Discovery','vCenterIP\r\nvCenterPort\r\nErrorCode\r\n', 86400),
('EC0403',0,'','Discovery operation failed.  ',' Discovery of the vCenter server <vCenterIP>:<vCenterPort> failed with the error code  <ErrorCode>. Either the vSphere CLI is not installed on the process server <PsName> (<PsIP>) or the vSphere CLI version installed is incompatible.. ','Ensure vSphere CLI 5.5 is installed on the process server. Restart the process server and retry the operation.   See http://go.microsoft.com/fwlink/?LinkId=613315  for guidance on discovery.','PS',0,'Yes',0,'Discovery','vCenterIP\r\nvCenterPort\r\nErrorCode\r\nPsName\r\nPsIP\r\n', 86400),
('EC0404',0,'','Discovery operation failed.',' Discovery of the vCenter server <vCenterIP>:<vCenterPort> failed with error code  <ErrorCode>. Operation failed with an error from vSphere SDK. ','\r\n      Refer to the  error string <ErrorString> in VMware documentation to fix the issue.\r\n\r\n.\r\n\r\n      See http://go.microsoft.com/fwlink/?LinkId=613315 for guidance on discovery.\r\n    ','PS',0,'Yes',0,'Discovery','vCenterIP\r\nvCenterPort\r\nErrorCode\r\nErrorString\r\n', 86400),
('ECA0000',0,'EC_SUCCESS','Success','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0001',-1,'EC_INVALID_FORMAT','Format Error','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0002',-2,'EC_AUTH_FAILURE','Authentication Failure','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0003',-5,'EC_INTERNAL_ERROR','','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0004',-6,'EC_UNKNWN','','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0011',-3,'EC_ATHRZTN_FAILURE','Authorization Failure','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0012',-4,'EC_FUNC_NOT_SUPPORTED','Function Not Supported','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0013',1,'EC_INSFFCNT_PRMTRS','Missing <Parameter> Parameter','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0014',3,'EC_NO_DATA','No Value for <Parameter>','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0015',-7,'EC_INVALID_DATA','Invalid Data for <Parameter> Parameter','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0016',6,'EC_INVALID_PRM','Invalid parameter name <Parameter>','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0017',4,'EC_MTAL_EXCLSIVE_ARGS','','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0018',2,'EC_WRNG_PRMTR_TYPE','','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0020',5042,'EC_NO_RECORD_INFO','','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0101',5001,'EC_NO_PS_HEART_BEAT','The process server is not accessible.','The process server <PsName> (<PsIP>)  is not accessible and the operation failed. There is no heartbeat from the process server in the last <Interval> minutes. ','\r\n      1.Ensure that the process server is accessible from the configuration server.\r\n      2. Ensure that the process server services - cxprocessserver, tmansvc - are running.   If the issue persists, please contact Microsoft support.\r\n    ','',0,'Yes',0,'API Errors','PsName\r\nPsIP\r\nInterval\r\n', 86400),
('ECA0102',5011,'EC_NO_INVENTORY_FOUND','No vCenter Server found with the requested Id. ','The requested entry for vCenter server does not exist or is an old entry, and the operation failed.','Use purge scripts to clean up the outdated entries. See http://go.microsoft.com/fwlink/?LinkId=613316 for guidance on purge scripts. If the issue persists, please contact Microsoft support. ','',0,'Yes',0,'API Errors','', 86400),
('ECA0103',5081,'EC_NO_HOST_FOUND_SETVPN','VPN configuration for the requested host failed.  ','There is no host associated with the requested Id and the operation failed.','Use purge scripts to clean up the outdated entries. See http://go.microsoft.com/fwlink/?LinkId=613316 for guidance on purge scripts. If the issue persists, please contact Microsoft support. ','',0,'Yes',0,'API Errors','', 86400),
('ECA0104',5091,'EC_NO_AGENT_FOUND','<OperationName> Operation could not complete. ','The requested host could not be identified and the operation failed with the error code <ErrorCode>. Either the source machine is deleted or the mobility service might have been uninstalled manually.','Disable protection for the source and retry the operation. If the source is deleted, use the purge scripts to clean up any outdated entry. See http://go.microsoft.com/fwlink/?LinkId=613316 for guidance on purge scripts. If the issue persists, please contact Microsoft support.','',0,'Yes',0,'API Errors','OperationName\r\n', 86400),
('ECA0105',5091,'EC_NO_MT_FOUND','<OperationName> could not complete.','The requested operation on the master target server could not be performed and failed with error code <ErrorCode>. The mobility service on the master target server might have been uninstalled.','Use the purge scripts to clean up outdated entries. See http://go.microsoft.com/fwlink/?LinkId=613316 for guidance on purge scripts. If the issue persists, please contact Microsoft support.','',0,'Yes',0,'API Errors','OperationName\r\n', 86400),
('ECA0106',5092,'EC_NO_PS_FOUND','<OperationName> Operation could not complete. ','The requested operation failed with the error code <ErrorCode>. The process server does not exist. It might have been uninstalled manually.','Use the purge scripts to clean up outdated entries. See http://go.microsoft.com/fwlink/?LinkId=613316 for guidance on purge scripts. If the issue persists, please contact Microsoft support.','',0,'Yes',0,'API Errors','OperationName\r\nErrorCode\r\n', 86400),
('ECA0107',5093,'EC_NO_MBR_FOUND','Protection could not be enabled.','Enable protection on the source machine <SourceHostName> (<SourceIP>) failed. Key information pertaining to the source is missing. Source information sync might be in-progress or not completed.','Restart the mobility service (InMage Scout VXAgent - Sentinel/Outpost) on the source machine and ensure network connectivity exists between the source machine and the configuration server.  If the issue persists, disable the protection and retry the operation. ','',0,'Yes',0,'API Errors','SourceHostName\r\nSourceIP\r\n', 86400),
('ECA0108',5101,'EC_NO_RETENTION_VOLUMES','The requested operation could not complete.','The requested operation failed. The retention volume <RetentionDrive> on the master target server is not accessible. ','Ensure that the retention volume is available and accessible, and retry the operation. If the issue persists, please contact Microsoft support.','',0,'Yes',0,'API Errors','RetentionDrive\r\n', 86400),
('ECA0109',5103,'EC_NO_SUFFICIENT_SPACE_FOR_RETENTION','Protection operation failed.','There is no sufficient space on the master target server retention volume <RetentionDrive>  and the requested operation failed.','If the size of the retention disk is less than 1 TB, it can be extended up to 1 TB or protect the source to a different master target server. See http://go.microsoft.com/fwlink/?LinkID=613317 for capacity planning.  If the issue persists, contact Microsoft support. .','',0,'Yes',0,'API Errors','RetentionDrive\r\n', 86400),
('ECA0110',5104,'EC_OS_CHECK','Protection operation failed.','Operating system mismatch between the source machine and the target. The requested operation failed with error code <ErrorCode>. ','Ensure that the source and target have the same type of Operating systems (Windows or Linux) running. ','',0,'Yes',0,'API Errors','', 86400),
('ECA0111',5105,'EC_PLAN_EXISTS','Protection could not be enabled. ','Protection group name already exists.','Choose a different protection group name or delete the existing and recreate the protection group.','',0,'Yes',0,'API Errors','', 86400),
('ECA0112',5107,'EC_IS_SOURCE_VOLUME_ELIGIBLE','The requested operation failed. ',' The volumes <ListVolumes> on the source machine <SourceHostName> (<SourceIP>) are not accessible. ','Ensure all the volumes on the source machine are available including  <ListVolumes> and retry the operation.','',0,'Yes',0,'API Errors','ListVolumes\r\nSourceHostName\r\nSourceIP\r\nListVolumes\r\n', 86400),
('ECA0113',5108,'EC_INVALID_PLAN_STATE','Retry of the Enable protection is not applicable for requested protected item as the job is <ProtectionState>.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0114',5109,'EC_SRC_TGT_PROTECTED','Protection could not be enabled on the source machine','The source machine  <SourceHostName> (<SourceIP>) is already protected. ','The source is already protected. If the intent is to reconfigure to a different target, disable the existing protection and configure it as needed while enabling protection. ','',0,'Yes',0,'API Errors','SourceHostName\r\n', 86400),
('ECA0115',5113,'EC_NO_MT_HEART_BEAT','The master target server is not accessible. <OperationName> could not complete.','No heart beat from the master target server <MTHostName>(<MTIP>) in the last <Interval> minutes. ','\r\n      1. Ensure the master target server is available and is connected to the network.\r\n      2. Ensure that the mobility services (InMage Scout Application Service, InMage Scout VX Agent - Sentinel/Outpost) on the master target server are running. If the issue persists, please contact Microsoft support.\r\n    ','',0,'Yes',0,'API Errors','OperationName\r\nMTHostName\r\nMTIP\r\nInterval\r\n', 86400),
('ECA0116',5114,'EC_NO_PUBLIC_IP','VPN details are not configured for MasterTarget <MTHostName>','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0117',5115,'EC_DISK_HAS_UEFI','Protection could not be enabled. ','Enable protection failed for the source disks <ListVolumes> on the source machine <SourceHostName> (<SourceIP>). UEFI partitions are not supported.  ','UEFI partitions are not supported. See http://go.microsoft.com/fwlink/?LinkId=613318 for the pre-requisites. ','',0,'Yes',0,'API Errors','ListVolumes\r\nSourceHostName\r\nSourceIP\r\n', 86400),
('ECA0118',5116,'EC_DISK_IS_DYNAMIC','Protection could not be enabled. ','Enable protection failed for the volumes <ListVolumes> on the source machine <SourceHostName> (<SourceIP>). Dynamic disks are not supported. ','Dynamic disks are not supported. See http://go.microsoft.com/fwlink/?LinkId=613318 for the pre-requisites. ','',0,'Yes',0,'API Errors','ListVolumes\r\nSourceHostName\r\nSourceIP\r\n', 86400),
('ECA0119',5132,'EC_NO_PLAN_FOUND','No protection group found with the requested plan Id <PlanID>.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0120',5133,'EC_NO_GUID_EXIST_IN_PLAN','Protection not created for the requested source host(s) <SourceHostGUID>.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0121',5142,'EC_NO_PLAN_MATCH','Given source servers <SourceHostGUID> does not exists with the requested protection plan id <PlanID>.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0122',5182,'EC_NO_SCENARIO_EXIST','No protected servers configured for the requested plan id <PlanID>','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0123',5203,'EC_PLAN_IN_RESTART_RESYNC','Resynchronization is already in progress for the machine.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0124',5206,'EC_NOT_PART_OF_PLAN','Given source servers are not part of the same plan.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0125',5212,'EC_NO_PROTECTION_SERVER','Protection for source server <SourceHostName> couldn\'t be found.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0126',5213,'EC_TAG_TIME_GREATER','Difference between Tag dates is more than 5 days','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0127',5214,'EC_NOT_IN_DIFF_SYNC','Protection Group - Replication for <SourceHostGUID> is not set to Differential Sync','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0128',5231,'EC_NO_RECOVERY_OPTION','Invalid recovery option (Allowed only MultiVMLatestTag/LatestTag/LatestTime/Custom) passed to the configuration server.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0129',5232,'EC_NO_PAIRS','Virtual /Physical machine <ProtectionPlanId>/<SourceHostID> is already failed over. You can ignore this error if you are retrying the failover of a recovery plan.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0130',5233,'EC_ROLLBACK_EXISTS','Recovery plan already exists for the source server <SourceHostGUID>.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0131',5234,'EC_NO_RECOVERY_POINT','Recovery point <RecoveryPoint> does not exist. Please select a valid recovery point or try again after some time.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0132',5237,'EC_PLAN_EXISTS','Recovery <PlanName> already exists.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0133',5251,'EC_NO_RLBK_DATA','No Recovery job found for source servers <SourceHostGUID>.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0135',5262,'EC_ROLLBACK_INPROGRESS','The requested operation could not complete. ','The requested operation failed with the error code <ErrorCode>. Rollback is in progress. ','','',0,'Yes',0,'API Errors','ErrorCode\r\n', 86400),
('ECA0136',5281,'EC_NO_DATA_FOUND','Health information is not received for the process server and the configuration server. ','The process server and the configuration server could be down. ','Ensure that the process server and the configuration server are configured and available. ','',0,'Yes',0,'API Errors','', 86400),
('ECA0137',5292,'EC_INVALID_PLAN_TYPE','Invalid plan type (Allowed only Protection/Rollback) found.','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0138',5301,'EC_NO_RQUSTID_FOND','No request found for this request Id <RequestId>','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0139',5021,'EC_PROTECTION_EXIST','Delete vCenter Server operation failed.','One or more of the source machines associated with the vCenter Server are protected. ','Ensure that all the protected items are removed or migrated from the vCenter Server and retry the delete operation.','',0,'Yes',0,'API Errors','', 86400),
('ECA0140',5042,'EC_IPV6_DATA','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>.  The provided IP address is of the type IPV6, which is not currently supported.','\r\n      Ensure that at least one IPv4 IP address is assigned to the source machine.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'API Errors','SourceIP\r\nErrorCode\r\n', 86400),
('ECA0141',5331,'EC_ACCOUNT_EXIST','Account already exist with the requested account name <AccountName>','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0142',5341,'EC_NO_ACCOUNT_EXIST','No account found with the requested account id <AccountId>','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0143',5351,'EC_DATA_EXIST','One or more inventory/agent installers are discovered using this account id <AccountId>. Please disable protection on them before removing the account','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0144',5117,'EC_CLUSTER_SERVER','Proteciton on this virtual machine <SourceHostName> (<SourceIP>) is not supported as the server contains shared storage','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0145',5043,'EC_NO_UPGRADES','No Upgrade builds are available for the Push server <SourceHostName> (<SourceIP>).','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0146',5044,'EC_NO_UPDATES','No Updates are available for the Push Server <SourceHostName> (<SourceIP>).','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0147',5045,'EC_LATEST_VERSION','Source Server is already on latest version','','','',0,'Yes',0,'API Errors','', 86400),
('ECA0148',5002,'EC_VM_EXISTS','Discovery operation failed.','There is already a source machine discovered through either vCenter/ESX server with the specified IP Address <ServerIP>','Either the source can be protected as a VM or a physical server. \r\n 1. Either continue the protection as VM discovered through vCenter/ESX. \r\n 2. To protect as physical server, disconnect the source from the vCenter/ESX or delete the vCenter/ESX from the discovery. ','',0,'Yes',0,'API Errors','', 86400),
('ECA0149',5003,'EC_OS_WINDOWS','Protection operation failed.','Master Target operating system not supported for this operation. The requested operation failed with error code <ErrorCode>.','Ensure that the master target Operating systems (Windows) running.','',0,'Yes',0,'API Errors','', 86400),
('ECA0150',5004,'EC_DISK_SIGNATURE_INPUT_INVALID','Protection operation failed.','Invalid Data input of <ListVolumes> for SourceDiskName Parameter for host <SourceHostName> ( <SourceIP> )','Ensure that the source disk name parameter data is disk signature.','',0,'Yes',0,'API Errors','', 86400),
('ECA0151',5005,'EC_DISK_NAME_INPUT_INVALID','Protection operation failed.','Invalid Data input of <ListVolumes> for SourceDiskName Parameter for host <SourceHostName> ( <SourceIP> )','Ensure that the source disk name parameter data is disk name.','',0,'Yes',0,'API Errors','', 86400),
('ECA0152',5006,'EC_NO_HOST_FOUND','Discovery failed on source machine.','Discovery failed as source machine with IP address <ServerIP>  (host Id: <HostIdHolder> ) was not able to reach the configuration server.','1. Ensure the source machine is present, powered on and is connected to the network on which configuration server is reachable.\r\n 2. Check that the mobility services (InMage Scout Application Service, InMage Scout VX Agent - Sentinel/Outpost) on the source machine are running. If not, please contact support.\r\n 3.Check that the correct IP address for the configuration server is configured for mobility services. \r\n ','',0,'Yes',0,'API Errors','HostIdHolder\r\nServerIP\r\n', 86400),
('ECA0153',5007,'EC_MULTI_PATH_FOUND','Protection could not be enabled. ','Enable protection failed for the disks <ListVolumes> on the source machine <SourceHostName> (<SourceIP>). System identified multiple I/O paths from server to storage device. Multi path configuration is not supported.','Select a server which doesnt have multi path configuration for protection.','',0,'Yes',0,'API Errors','ListVolumes\r\nSourceHostName\r\nSourceIP\r\n', 86400),
('ECA0154',5008,'EC_DIFFERENT_MT_FOUND','Protection could not be enabled on the requested plan Id.','Add protection failed on the requested plan Id <PlanID> with the selected Master Target.','As multi VM protection consistency is on, select the same Master Target across VM protections in the same plan.','',0,'Yes',0,'API Errors','PlanID\r\n', 86400),
('EP0851',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. The configuration server <CsIP> is not accessible. ','\r\n      Ensure that the process server <PsName> (<PsIP>) and the configuration server  <CsIP> are connected to the network, and the configuration server is accessible from the process server.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nCsIP\r\nPsName\r\nPsIP\r\nCsIP\r\n', 86400),
('EP0852',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. The configuration server <CsIP> is not accessible from the process server <PsName> (<PsIP>). ','\r\n      Ensure that the process server <PsName> (<PsIP>) and the configuration server <CsIP> are connected to the network, and the configuration server is accessible from the process server.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nCsIP\r\nPsName\r\nPsIP\r\nPsName\r\nPsIP\r\nCsIP\r\n', 86400),
('EP0853',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. Signature validation of the mobility service installer failed on the process server <PsName> (<PsIP>).','Signature verification is a security measure and internet connectivity is required for the same.\r\n\t1.Ensure that the process server <PsName> (<PsIP>) has internet connectivity.\r\n\t2.Alternately, the signature validation on the mobility service installer package can be disabled to install the mobility service without the signature validation.\r\n\ta.Run the cspsconfigtool.exe from the installation folder on the process server (<Installpath>/bin) to launch Microsoft Azure Site Recovery Process Server UI.\r\nb.Uncheck the "Verify Mobility Service software signature before installing on source machines" option and save.\r\n\r\nDisable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n\r\n\tSee http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nPsName\r\nPsIP\r\nPsName\r\Installpath\r\n', 86400),
('EP0854',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. Either the source machine is not running, or there are network connectivity issues between the process server <PsName> (<PsIP>) and the source machine.','\r\n      1. Ensure that the source machine is accessible from the process server <PsName> (<PsIP>).\r\n      2. Allow Windows Management Instrumentation (WMI)  in the Windows Firewall. Under Windows Firewall settings, select the option “Allow an app or feature through Firewall” and select WMI for all profiles.\r\n\r\n      In addition, the following is needed for push installation to complete successfully:\n      1. Allow File and Printer Sharing in the Windows Firewall. Under Windows Firewall settings, select the option “Allow an app or feature through Firewall” and select File and Printer Sharing for all profiles.\r\n      2. Ensure that the user account has administrator rights on the source machine.\r\n      3. Disable remote User Account Control (UAC) if you are using local administrator account to install the mobility service.   Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nPsName\r\nPsIP\r\nPsName\r\nPsIP\r\n', 86400),
('EP0855',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. Either the source machine is not running, or there are network connectivity issues between the process server <PsName> (<PsIP>) and the source machine.     ','\r\n      1. Ensure that the source machine  is accessible from the process server <PsName> (<PsIP>).\r\n      2. Ensure SSH service is running on the source machine.\r\n\r\n      In addition, the following is needed for push installation to complete successfully:\n      1. Root credentials should be provided.\r\n      2. SFTP service should be running.\r\n      3. /etc/hosts on the source machine must contain entries for all IP addresses of the source machine.\r\n      4. Ensure the source machine Linux variant is supported.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nPsName\r\nPsIP\r\nPsName\r\nPsIP\r\n', 86400),
('EP0856',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. Either the source machine is not running, or there are network connectivity issues between the process server <PsName> (<PsIP>) and the source machine.','\r\n      1. Ensure the source machine  is accessible from the process server <PsName> (<PsIP>).\r\n      2. Allow File and Printer Sharing in the Windows Firewall. Under Windows Firewall settings, select the option “Allow an app or feature through Firewall” and select File and Printer Sharing for all profiles.\r\n\r\n      In addition, the following is needed for push installation to complete successfully:\n      1. Allow Windows Management Instrumentation (WMI)  in the Windows Firewall. Under Windows Firewall settings, select the option “Allow an app or feature through Firewall” and select WMI for all profiles.\r\n      2. Ensure that the user account has administrator rights on the source machine.\r\n      3. Disable remote User Account Control (UAC) if you are using local administrator account to install the mobility service.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nPsName\r\nPsIP\r\nPsName\r\nPsIP\r\n', 86400),
('EP0857',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. Either the source machine is not running, or there are network connectivity issues between the process server <PsName> (<PsIP>) and the source machine.','\r\n      1. Ensure the source machine  is accessible from the process server <PsName> (<PsIP>).\r\n      2. Ensure SFTP service is running on the source machine.\r\n\r\n      In addition, the following is needed for push installation to complete successfully:\n      1.  Root credentials should be provided.\r\n      2.  SSH service should be running.\r\n      3.  /etc/hosts on the source machine must contain entries for all IP addresses of the source machine.\r\n      4. Ensure the source machine Linux variant is supported.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nPsName\r\nPsIP\r\nPsName\r\nPsIP\r\n', 86400),
('EP0858',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. Either that the credentials provided to install mobility service is incorrect or the user account has insufficient privileges. ','\r\n      Ensure the user credentials provided for the source machine are correct.\r\n      In addition, the following is needed for push installation to complete successfully:\n      1. Ensure that the user account has administrator rights on the source machine.\r\n      2. Disable remote User Account Control (UAC) if you are using a local administrator account to push the mobility service.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0859',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. Either that the credentials provided to install mobility service is incorrect or the user account has insufficient privileges. ','\r\n      Ensure that the root credentials are provided for the source machine.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0860',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. The process server <PsName> (<PsIP>) could not identify the Linux version of the source machine.','\r\n      Ensure the following for push installation to complete successfully:\n      1. The source machine <SourceIP> is accessible from the process server <PsName> (<PsIP>)\n      2. Root credentials should be provided.\r\n      3. SSH and SFTP services should be running.\r\n      4.  /etc/hosts on the source machine must contain entries for all IP addresses of the source machine.\r\n      5. Ensure the source machine Linux variant is supported.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nPsName\r\nPsIP\r\nSourceIP\r\nPsName\r\nPsIP\r\n', 86400),
('EP0861',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>.','\r\n      Ensure network connectivity exists between the source machine <SourceIP> and the configuration server <CsIP>.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. If the issue persists, contact support with the logs located on the source machine at <LogPath>.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nSourceIP\r\nCsIP\r\nLogPath\r\n', 86400),
('EP0862',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>.','\r\n      1.Ensure network connectivity exists between source <SourceIP> and the configuration server <CsIP>.\r\n      2. /etc/hosts on the source machine must contain entries for all IP addresses of the source machine.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. If the issue persists, contact support with the logs located on the source machine at <LogPath>.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nSourceIP\r\nCsIP\r\nLogPath\r\n', 86400),
('EP0863',0,'','The mobility service is already installed.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>.  The mobility service is already installed.. ','The mobility service is already installed on  <SourceIP>. No action required.  ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nSourceIP\r\n', 86400),
('EP0864',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine  <SourceIP> failed with error code  <ErrorCode>. ','\r\n      A different version of the mobility service is already installed on the source machine. To install the latest version of mobility service, manually uninstall the older version, disable the protection and retry the operation.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0865',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine  <SourceIP> failed with error code  <ErrorCode>. Either the source machine is not running, or there are network connectivity issues between the process server <PsName> (<PsIP>) and the source machine. ','\r\n      Ensure the source machine <SourceIP> is accessible from the process server <PsName> (<PsIP>).\r\n\r\n      In addition, the following is needed for push installation to complete successfully:\n      1. Allow “File and Printer Sharing” and “Windows Management Instrumentation (WMI)” in the Windows Firewall.  Under Windows Firewall settings, select the option “Allow an app or feature through Firewall”. Select File and Printer Sharing and WMI options for all profiles.\r\n      2. Ensure that the user account has administrator rights on the source machine.\r\n      3. Disable remote User Account Control (UAC) if you are using local administrator account to install the mobility service.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nPsName\r\nPsIP\r\nSourceIP\r\nPsName\r\nPsIP\r\n', 86400),
('EP0866',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. Either the source machine is not running, or there are network connectivity issues between the process server <PsName> (<PsIP>) and the source machine.','\r\n      Ensure the source machine  <SourceIP> is accessible from the process server <PsName> (<PsIP>).\r\n\r\n      In addition, the following is needed for push installation to complete successfully:\n      1. Root credentials should be provided.\r\n      2. SSH and SFTP services should be running.\r\n      3.  /etc/hosts on the source machine must contain entries for all IP addresses of the the source machine.\r\n      4. Ensure the the source machine Linux variant is supported.\r\n\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nPsName\r\nPsIP\r\nSourceIP\r\nPsName\r\nPsIP\r\n', 86400),
('EP0867',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. The mobility service is already installed on the source machine and is registered to a different configuration server <CsIP>.','\r\n      Uninstall the mobility service manually on the source machine, disable protection and retry the operation.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nCsIP\r\n', 86400),
('EP0868',0,'','Uninstallation failed. ','Uninstallation of the mobility service failed on the source machine  <SourceIP>. One or more of the relevant services failed to shut down.','Uninstall the mobility service manually on the source machine, disable protection and retry the operation  after ensuring that all the services pertaining to the mobility service are stopped. ','',0,'Yes',0,'PushInstall','SourceIP\r\n', 86400),
('EP0869',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>.','\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0870',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>.','\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0871',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>.','\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0872',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>. Reboot of the source machine is required.','\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0873',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>. Either the configuration server IP address or HTTPS port is invalid.','\r\n      1. Ensure the configuration server IP address and HTTPS port number are correct.\r\n      2. Ensure the configuration server is accessible from source machine <SourceIP> using the configuration server IP <CsIP> and HTTPS port <CsPort>.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nSourceIP\r\nCsIP\r\nCsPort\r\n', 86400),
('EP0874',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> failed with error code <ErrorCode>. Operating System version on the source machine is not supported.','\r\n      Ensure that the source machine OS version is supported.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0875',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>. The mobility service cannot be installed on Itanium based machines.','\r\n      Itanium based systems are not supported.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0876',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>. Required directories to install mobility service could not be created on the source machine.','\r\n      Ensure the user has administrator rights and write privileges to the system directory on source machine.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0877',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>. There is no valid host entry in /etc/hosts file.','\r\n      /etc/hosts file on the source machine must contain entries for all IP addresses of the source machine.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0878',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>. Not enough free space available on the disk.','\r\n      Ensure at least 100 MB of space is available on the disk for installation.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0879',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>. Installer could not create temporary directory.','\r\n      Ensure the user has administrator rights and write privileges to the system directory on the source machine.\r\n      Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n      See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\n', 86400),
('EP0880',0,'','Protection could not be enabled.','Installation of the mobility service failed on the source machine <SourceIP> with error code <ErrorCode>. Files required for installation are missing on the source machine.','\r\n  If there is any Anti-Virus running, add an exclusion for Microsoft Azure Site Recovery installation folder in the Anti-Virus rules.\r\nDisable the protection and retry the operation after ensuring that all prerequisites for push installation of the motility service are complete. If the issue persists, contact support with the logs located on the source machine at <LogPath>.\r\nSee http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n   ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nSourceOs\r\n', 86400),
('EP0881',0,'','Protection operation failed.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. The Linux version of the source machine <SourceOs> is unsupported.','\r\n      Ensure the following for push installation to complete successfully:\r\n  1. Root credentials should be provided.\r\n  2. SSH and SFTP services should be running. \r\n 3.  /etc/hosts on the source machine must contain entries for all IP addresses of the source machine. \r\n 4. Ensure the source machine Linux variant is supported. \r\n   Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\n  See http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.\r\n    ','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nSourceOs\r\n', 86400),
('VA0601',0,'VM health','','','No action is required.','Agent',4,'Yes',0,'Alerts/Events','', 86400),
('VA0602',0,'VM health','','','No action is required.','Agent',4,'Yes',0,'Alerts/Events','', 86400),
('VE0701',0,'VM health','','','Please contact support for investigation.','Agent',4,'Yes',1,'Alerts/Events','', 86400),
('VE0702',0,'VM health','','','Please contact support for investigation.','Agent',4,'Yes',1,'Alerts/Events','', 86400),
('VE0703',0,'VM health','','','Check the network connectivity between CS and the MT server. Please contact support if issue persists.','Agent',4,'Yes',0,'Alerts/Events','', 86400),
('VE0704',0,'VM health','','','Check the network connectivity between CS and the MT server. Please contact support if issue persists.','Agent',4,'Yes',0,'Alerts/Events','', 86400),
('EP0882',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. Operation on the installation directory did not succeed.','Delete the installation directory <InstallPath> on the source machine.\r\nDisable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\nSee http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nInstallPath', 86400),
('EP0883',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> failed with error code  <ErrorCode>. A system restart from a previous installation / update is pending.','Restart the source machine, disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.\r\nSee http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0884',0,'','Protection could not be enabled.','Push installation of the mobility service to the source machine <SourceIP> succeeded but source machine requires a restart before proceeding to protection.','Restart the source machine, Disable protection and retry the operation after ensuring that restart of the source mahcine <SourceIP> done successfully.\r\nSee http://go.microsoft.com/fwlink/?LinkId=525131 for push installation guidance.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0886',0,'','Mobility Service Agent Installation could not be completed for source machine as the registration with the Configuration Server failed.','The Configuration Server is not reachable from the source machine.','1. Ensure that the Configuration server specified is correct and is reachable from the source machine. \r\n 2. Retry enable protection for the source machine.','',0,'Yes',0,'PushInstall','', 86400),
('EP0888',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service Setup failed to complete installation due to an internal error.','\r\nReview the installation logs at <LogPath> for more details. If the issue persists, contact support with the logs located on the source machine at <LogPath>.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode\r\nLogPath', 86400),
('EP0889',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service Setup failed to complete as incorrect parameters were passed to the setup.','\r\nReview the command-line arguments for the installer at https://aka.ms/installmobsvc and re - run installer again.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0890',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nAnother instance of Installer is already running.','\r\nWait for the other instance to complete before you try to launch the installer again.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0891',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service Setup is not compatible with the current operating system. Please retry installation after picking up the correct installer.','\r\nUse the Mobility Service version table under the section "Install the Mobility Service manually" in https://aka.ms/installmobsvc to identify the right version of the installer for the current operating system.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0892',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service Setup does not have all the required installation arguments.','\r\nReview the command line install syntax from https://aka.ms/installmobsvc and retry installation.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0893',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nParameters passed to Mobility Service Setup are incorrect.','\r\nOne of the following parameters are wrong:\r\n1. The -t parameter can take only values: FX|VX|Both.\r\n2. The -R parameter can take only values: Agent|MasterTarget.\r\n3. The -S parameter can take only values: Y|y|N|n.\r\n4. The -c parameter can take only values http|https.\r\n5. The -A parameter can take only values U|u.\r\nRetry installation with the correct parameters.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0894',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service Setup is unable to find passphrase file.','\r\nEnsure that the\r\n1. The file exists at the given location.\r\n2. Your account has access to the file location.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0895',0,'','Update of mobility service in the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service Setup is unable to proceed as the InMage Scout VX Agent - Sentinel/Outpost service could not be stopped. Update requires InMage Scout VX Agent - Sentinel/Outpost service to be in stopped state.','\r\nStop the InMage Scout VX Agent - Sentinel/Outpost service manually using Microsoft Management Console and retry the update.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0896',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service  Setup was unable to procced as the logged in
 user does not have privileges to perform this installation.','\r\nPlease use credentials with administrator privileges and retry enable protection.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0897',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service Update failed as the software to be updated could not be found.','\r\nCheck Add/Remove programs to ensure that the Azure Site Recovery Master Target/Mobility Service is installed before running the upgrade.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0898',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service Setup failed as the kernel version is not supported.','\r\nPlease make sure that the supported kernel version is available before installing the software.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0899',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nMobility Service Setup is not supported on this version of Linux yet.','\r\nPlease check the list of supported Linux versions at https ://aka.ms/vmprereq','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0900',0,'','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nSome of the drivers / services required by the mobility service are missing.','\r\nPlease ensure that the drivers / services mentioned at https://aka.ms/asr-boot-drivers-list are installed.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EP0902',0,'','Push installation of the update mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','\r\nPush installation of the mobility service to the source machine succeeded but source machine requires a restart for some system changes to take effect.','\r\nReplication of the virtual machine will continue seamlessly. Reboot the server during your next maintenance window.','',0,'Yes',0,'PushInstall','SourceIP\r\nErrorCode', 86400),
('EA0615',0,'CS health','The SSL certificate on <CsName> has expired.','The SSL certificate on <CsName> has expired.','Please renew the certificate by logging on to <CsName> and executing the RenewCerts.exe utility.','CS',3,'Yes','0','Alerts/Events','CsName\r\nCsName\r\nCsName\r\n', 86400),
('EA0616',0,'CS health','The SSL certificate on <CsName> is going to expire in <NoOfDays> days.','The SSL certificate on <CsName> is going to expire in <NoOfDays> days.','Please renew the certificate by going to the Site Recovery Infrastructure->Configuration Server-> <CsName> on the Azure Portal and launching the Renew Certificate Job.','CS',3,'Yes','0','Alerts/Events','CsName\r\NoOfDays\r\nCsName\r\nNoOfDays\r\n', 86400),
('ECA0162',0,'','SSL certificate renewal failed.','Azure Site Recovery was unable to retrieve  the old certificate’s thumbprint.','Please resolve the issue and retry the operation.','CS',3,'Yes','0','RenewCertificate','', 86400),
('ECA0163',0,'','SSL certificate renewal failed on server <CsName>.','Azure Site Recovery was unable to generate a new SSL certificate.','Please resolve the issue and retry the operation.','CS',3,'Yes','0','RenewCertificate','CsName\r\n', 86400),
('ECA0164',0,'','SSL certificate renewal failed on server <CsName>.','Azure Site Recovery failed to remove bindings of the old certificate.','Please retry the operation to resolve the issue.','CS',3,'Yes','0','RenewCertificate','CsName\r\n', 86400),
('ECA0165',0,'','SSL certificate renewal failed on server <CsName>.','Binding the SSL certificate with Port 443 on <CsName> failed.','Please retry the operation to resolve the issue.','CS',3,'Yes','0','RenewCertificate','CsName\r\nCsName\r\n', 86400),
('ECA0166',0,'','Unable to start Service <ServiceName> on server <Name>.','The <ServiceName> service might went to not responding state.','Please resolve the issue and retry the operation.','CS',3,'Yes','0','RenewCertificate','ServiceName\r\nName\r\nServiceName\r\n', 86400),
('ECA0167',0,'','SSL certificate renewal failed on server <CsName>.','One or more files do not exist in the <FolderName> (<FileNames>)','Please resolve the issue and retry the operation.','CS',3,'Yes','0','RenewCertificate','CsName\r\FolderName\r\FileNames\r\n', 86400),
('ECA0168',0,'','Unable to stop Service <ServiceName> on server <Name>.','The <ServiceName> service might went to not responding state.','Please resolve the issue and retry the operation.','CS',3,'Yes','0','RenewCertificate','ServiceName\r\nName\r\nServiceName\r\n', 86400),
('ECA0169',0,'','SSL certificate renewal failed on server <PsName>','Azure Site Recovery was unable to generate a new SSL certificate','Please resolve the issue and retry the operation.','CS',3,'Yes','0','RenewCertificate','PsName\r\n', 86400),
('ECA0170',0,'','SSL certificate renewal failed on server <PsName>.','One or more files do not exist in the <FolderName> (<FileNames>)','Please resolve the issue and retry the operation.','CS',3,'Yes','0','RenewCertificate','PsName\r\FolderName\r\FileNames\r\n', 86400),
('ECA0171',5180,'EC_AGENT_VERSION_MISMATCH','The Server <HostName> is on a version <ServerVersion> , minimum version required to renew certificate is 9.4.0.0.','Certificates for one or more hosts could not be renewed.','Upgrade <HostName> server to 9.4.0.0 version and re-try the operation','',0,'Yes',0,'API Errors','HostName\r\ServerVersion\r\HostName\r\n', 86400),
('ECA0172',5181,'EC_NO_ASSOCIATED_PS','Configuration Server <HostName> does not have associated Process Server','PS registration was not successful','Please check if the OS language is no-english or not','',0,'Yes',1,'API Errors','HostName', 86400),
('ECA0173',5182,'EC_NO_SERVER_HEARTBEAT','The server(s) <HostName> are not accessible.','The server <HostName> is not accessible and the operation failed. There is no heartbeat from the server in the last 15 minutes.','1. Ensure that the server is accessible from the configuration server.2. Ensure that the server services - cxprocessserver, tmansvc, svagents - are running. If the issue persists, please contact Microsoft support. 3. If the server is deleted/uninstalled manually, please use the purge scripts to clean up outdated entries. See http://go.microsoft.com/fwlink/?LinkId=613316 for guidance on purge scripts. If the issue persists, please contact Microsoft support.','',0,'Yes',1,'API Errors','HostName', 86400),
('ECA0174',5183,'EC_DISK_IS_ENCRYPTED','Protection could not be enabled.','Enable protection failed for the source volumes <ListVolumes> on the source machine <SourceHostName> (<SourceIP>) as these volumes are encrypted. Protection of encrypted OS volumes is not supported.','Refer to https://aka.ms/azure-site-recovery-matrix for pre-requisites for machines to be protected.','',0,'Yes',0,'API Errors','ListVolumes\r\nSourceHostName\r\nSourceIP\r\n', 86400),
('ECA0175',0,'EC_MULTIPLE_CS_FOUND','Operation could not be completed because multiple configuration servers were detected.','This error can happen when you have earlier backed up a configuration server’s  MySQL database and restored it to a new server. Only backup and restore of the complete CS server is supported. Selective restore of MySQL DB alone is not supported.','If you intend to setup Configuration server through the restore of existing configuration server database backup then please contact Microsoft support.','',0,'Yes',0,'API Errors','', 86400),
('ECD001',78145,'Discovery validation','VM is Powered off','VM is Powered off','Power on the on-prem VM','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD002',78146,'Discovery validation','VMware tools are not running','Either VMware tools are not installed or stopped the VMware tools service inside Guest VM','Install the VMware tools and make sure that VMware tools service is running','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD003',78147,'Discovery validation','IP address not set','There could be MAC address mismatch inside Guest OS and at VMware VM level','Please fix the MAC address mismatch, and re-try','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD004',78148,'Discovery validation','VM having the UEFI boot','VM is firmware boot is of UEFI type','UEFI boot VMs are not supported, Contact the Microsoft support','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD005',78149,'Discovery validation','VM got deleted from the vCenter','VM is not found from the vCenter discovery','Please check the vCenter, if VM is removed perform the Failover and Re-protect','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD006',78150,'Discovery validation','Mastertarget having different SCSI controller types.','Master Target having different SCSI controller types.','Please change the controller type to be same controller types.','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD007',78151,'Discovery validation','Linux Mastertarget having unsupported SCSI controller type','Linux MT having the SCSI controller other than LsiLogicParallelController','Change the controller types to LsiLogicParallelController','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD008',78152,'Discovery validation','VM property Disk enable uuid is not set to True','disk.enableUUID not set True for VMware Master target','Set the disk.enableUUID property to True in VM advanced configuration settings','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD010',78156,'Discovery validation','VM Role can not be protected.','VM contains role as Master target/Process Server that can not be protected.','Uninstall the mobility agent on VM and retry the operation.','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD011',78158,'Discovery validation','VM having the snapshots.','VM contains snapshots may leads to issues.','Consolidate and delete all the snapshots on the VM.','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD012',78187,'Discovery validation','VM having the vVol datastore.','VM contains vVol datastore is not supported.','vVol datastore is not supported.','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECD013',78190,'Discovery validation','VM having the RDM disks.','VM is configured with Raw Device Mapping disks.','During failback, a new VMDK will be created on the on-premises machine, instead of reusing the on-premises RDM disk.','Discovery',0,'Yes',0,'Discovery','', 86400),
('ECA0155',5009,'EC_PS_MT_CAN_NOT_BE_PROTECTED','Discovery of physical machine failed.','Machine with IP address <ServerIP>, is already registered as Process server/Master target and cannot be protected','If you intent to protect this machine, please uninstall Process server/Master target from the source machine <ServerIP> and retry the operation.','',0,'Yes',0,'API Errors','',86400),
('EP0937',0,'EC_PUSH_INSTALL_TIMED_OUT','Installation of the mobility services on  server <SourceIP> has failed due to timeout.','Mobility service installation did not complete within a given time frame.','Please retry the operation. If the problem persists, contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('EC0161',0,'EC_RENEW_CERT_TIMED_OUT','Renewing the certificate of configuration server <CsName> has timed out.','Certificate renewal at configuration server has timed out due to internal error.','Please retry the operation. If the problem persists, contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('ECA0176',-8,'EC_DATABASE_ERROR','Database error occurred on configuration server.','Database error occurred on configuration server.','Please retry the operation. If the problem persists, contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('ECA0177',0,'EC_PASSPHRASE_READ_FAILED','Authentication Failure.','Unable to read passphrase.','Please contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('ECA0178',0,'EC_SIGNATURE_PASSWORD_MISMATCH','Authentication Failure.','Signature/Password mismatch.','Please contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('ECA0179',0,'EC_ACCESS_KEY_NOT_FOUND','Authentication Failure.','Access Key Not Found.','Please contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('ECA0180',0,'EC_FINGERPRINT_READ_FAILED','Authentication Failure.','Fingerprint read failed - Internal Error.','Please contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('ECA0181',0,'EC_INVALID_AUTHENTICATION_METHOD','Authentication Failure.','Invalid Authentication Method.','Please contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('EP0924',0,'AGENT_INSTALL_RPM_COMMAND_NOT_AVAILABLE','Installation  of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','Mobility Service Setup was unable to proceed as the rpm installation utility is missing.RPM database is in an inconsistent state.','Install rpm package manager from your Linux software repository before re-trying the operation.Rebuild RPM database before retrying the operation.','',0,'Yes',0,'API Errors','',86400),
('EP0915',0,'AGENT_INSTALL_PREREQS_FAIL','Enable Protection failed as the target machine did not meet one or more pre-requisites for installing Mobility Service.','Internal Error occurred.','For troubleshooting, please go to https://aka.ms/asr-installation-prerequisites-windows.','',0,'Yes',0,'API Errors','',86400),
('EP0936',0,'AGENT_INSTALL_CERT_GEN_FAIL','Installation of the mobility services on  server <SourceIP> failed with error code <ErrorCode>','Generation of a self-signed certificate failed.','Use an account with local administrator privileges when you perform install on Windows Server.Use root account when you perform install on a Linux Server.','',0,'Yes',0,'API Errors','',86400),
('EP0916',0,'AGENT_INSTALL_DISABLE_SERVICE_FAIL','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','One or more required Linux services is not running or disabled.','Ensure the LVM2-lvmetad services is not in a disabled state.','',0,'Yes',0,'API Errors','',86400),
('EP0921',0,'AGENT_INSTALL_OPERATING_SYSTEM_NOT_SUPPORTED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','The Operating system version <SourceOs> of the source machine is currently unsupported.','The operating system running on virtual machine <SourceIP> is currently unsupported. For more details on operating systems supported by Azure Site Recovery and other pre-requisites go to https://aka.ms/asr-os-support.','',0,'Yes',0,'API Errors','',86400),
('EP0929',0,'AGENT_INSTALL_DRIVER_INSTALLATION_FAILED','Installation of the mobility services on  server <SourceIP> failed with error code <ErrorCode>.','This version of mobility service doesnt support the operating system kernel version running on the source machine.','Please refer the list of operating systems supported by Azure Site Recovery : https://aka.ms/asr-os-support.','',0,'Yes',0,'API Errors','',86400),
('EP0925',0,'AGENT_INSTALL_CONFIG_DIRECTORY_CREATION_FAILED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Installer was unable to create directory /usr/local/InMage/config.','Ensure the root account has permission to create the directory.','',0,'Yes',0,'API Errors','',86400),
('EP0926',0,'AGENT_INSTALL_CONF_FILE_COPY_FAILED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Install was unable to copy required files into /usr/local/Inmage/config folder.','Ensure the root account has permission to create files under the directory.','',0,'Yes',0,'API Errors','',86400),
('EP0919',0,'AGENT_INSTALL_INSTALLATION_DIRECTORY_ABS_PATH_NEEDED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Installation directory path should be an absolute path.','Try installing with the installation directory parameter -d value as /usr/local/ASR.','',0,'Yes',0,'API Errors','',86400),
('EP0928',0,'AGENT_INSTALL_TEMP_FOLDER_CREATION_FAILED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Installer was unable to create a directory on the source machine.','Ensure the root account has permission to create the directory.','',0,'Yes',0,'API Errors','',86400),
('EP0927',0,'AGENT_INSTALL_RPM_INSTALLATION_FAILED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Installing one or more dependency packages failed.','Retry the operation after some time.','',0,'Yes',0,'API Errors','',86400),
('EP0931',0,'AGENT_INSTALL_SNAPSHOT_DRIVER_IN_USE','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Setup was unable to unload vsnap driver as it is in use.','Please re-try protection and contact support if issue persists.','',0,'Yes',0,'API Errors','',86400),
('EP0932',0,'AGENT_INSTALL_SNAPSHOT_DRIVER_UNLOAD_FAILED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Setup was unable to unload vsnap driver.','Please re-try protection and contact support if issue persists.','',0,'Yes',0,'API Errors','',86400),
('EP0906',0,'AGENT_INSTALL_INSUFFICIENT_SPACE_IN_ROOT','Enable Protection failed for <SourceIP>.','Insufficient free space available on the root partition/.','Ensure that there is at least 2 GB free space available on the root partition before you try to protect the virtual machine again.','',0,'Yes',0,'API Errors','',86400),
('EP0920',0,'AGENT_INSTALL_PARTITION_SPACE_NOT_AVAILABLE','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','The folder <InstallLocation> does not have sufficient free space.','Ensure that the installation directory has atleast 250 MB of free space.','',0,'Yes',0,'API Errors','',86400),
('EP0933',0,'AGENT_INSTALL_INVALID_VX_INSTALLATION_OPTIONS','Installation of the master target server on  server <SourceIP> failed with error code <ErrorCode>.','The operating system is not supported for a Master Target server installation.','Only UBUNTU-16.04-64 is supported  for Master Target server installation.','',0,'Yes',0,'API Errors','',86400),
('EP0930',0,'AGENT_INSTALL_CACHE_FOLDER_CREATION_FAILED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Installer was unable to create a directory on the source machine.','Ensure the root account has permission to create the directory.','',0,'Yes',0,'API Errors','',86400),
('EP0934',0,'AGENT_INSTALL_MUTEX_ACQUISITION_FAILED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Another instance of installer is already running.','Please re-try installation after some time.','',0,'Yes',0,'API Errors','',86400),
('EP0903',0,'AGENT_INSTALL_COMPLETED_BUT_RECOMMENDS_REBOOT_LINUX','Update Mobility Services completed with warnings.','Push installation of the mobility service to the source machine succeeded but source machine <SourceIP> requires a restart for some system changes to take effect.','Replication of the virtual machine will continue seamlessly. Reboot the server during your next maintenance window to get benefits of the new enhancements in mobility service.','',0,'Yes',0,'API Errors','',86400),
('EP0904',0,'AGENT_INSTALL_CURRENT_KERNEL_NOT_SUPPORTED','Installation of Mobility Service failed with an error.','Push installation of the mobility service to the source machine <SourceIP> failed. This version of mobility service doesnt support the operating system kernel version running on the source machine.','Please refer the list of operating systems supported by Azure Site Recovery : https://aka.ms/asr-os-support','',0,'Yes',0,'API Errors','',86400),
('EP0905',0,'AGENT_INSTALL_LIS_COMPONENTS_NOT_AVAILABLE','Enable Protection failed for <SourceIP>','The necessary Hyper-V Linux Integration Services components (hv_vmbus, hv_netvsc, hv_storvsc) are not installed on this machine.','Install the Linux Integration service components on this machine and retry the operation. Read more about Linux Integration services https://blogs.technet.microsoft.com/virtualization/2016/07/12/which-linux-integration-services-should-i-use-in-my-linux-vms/','',0,'Yes',0,'API Errors','',86400),
('EP0913',0,'AGENT_INSTALL_INVALID_COMMANDLINE_ARGUMENTS','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>','Mobility Service setup failed to complete as incorrect values were passed as parameters during the setup.','Review the command line arguments for the installer at https://aka.ms/installmobsvc and re-run the setup again.','',0,'Yes',0,'API Errors','',86400),
('EP0907',0,'AGENT_INSTALL_SPACE_NOT_AVAILABLE','Enable Protection failed to install mobility service on <SourceIP> with error code <ErrorCode>. Insufficient free space available on disk','1. Less than 1 GB of free space is available on <InstallLocation>. 2. The <InstallLocation> selected is not a Fixed disk.','1. Install mobility service on a disk that has more than 1 GB Free space. 2. Ensure the disk on which the mobility service is getting installed is a Fixed disk.','',0,'Yes',0,'API Errors','',86400),
('EP0885',0,'AGENT_INSTALL_DOTNET35_MISSING','Protection could not be enabled.','Installation on Win2k8-r2 requires .Net 3.5.1 to be present as pre-requisite.','You can install it using AddRoles and Features in Server Manager.','',0,'Yes',0,'API Errors','',86400),
('EP0908',0,'AGENT_INSTALL_VMWARE_TOOLS_NOT_RUNNING','Enable Protection failed to install mobility service on <SourceIP> with error code <ErrorCode>. VMware tools not installed on server.','VMware tools is not installed on this server.','Install the latest version of VMware Tools and try installation again.','',0,'Yes',0,'API Errors','',86400),
('EP0909',0,'AGENT_INSTALL_VSS_INSTALLATION_FAILED','Enable Protection failed to Configure mobility service on <SourceIP> with error code <ErrorCode>. Install VSS Provider failed.','The computer is running into a low memory condition or the Remote procedure call (RPC) service is not running or is in disabled state.','1. Ensure the RPC Server is in running state. 2. The available memory on the computer is more than 500 MB.','',0,'Yes',0,'API Errors','',86400),
('EP0910',0,'AGENT_INSTALL_DRIVER_MANIFEST_INSTALLATION_FAILED','Enable Protection failed to Configure mobility service on <SourceIP> with error code <ErrorCode>. Install driver manifest file failed.','Windows subsystem returned an error while trying to install driver manifest files.','Contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('EP0914',0,'AGENT_INSTALL_DRIVER_ENTRY_REGISTRY_UPDATE_FAILED','Push installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>.','Updating driver entry in the registry failed.','Retry the operation after some time.','',0,'Yes',0,'API Errors','',86400),
('EP0943',0,'AGENT_INSTALL_VSS_INSTALLATION_FAILED_OUT_OF_MEMORY','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Memory required for the installation of VSS provider is not available.','Azure Site Recovery installs VSS provider on the machine as a part of mobility agent installation. It might be possible that the system doesnt have required available memory for the installation due to heavy usage. Please retry again once the system memory is available.','',0,'Yes',0,'API Errors','',86400),
('EP0938',0,'AGENT_INSTALL_VSS_INSTALLATION_FAILED_DATABASE_LOCKED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','VSS provider installation failed as service database is locked.','Contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('EP0939',0,'AGENT_INSTALL_VSS_INSTALLATION_FAILED_SERVICE_ALREADY_EXISTS','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','VSS provider installation failed as old stale installation already exists.','Contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('EP0940',0,'AGENT_INSTALL_VSS_INSTALLATION_FAILED_SERVICE_MARKED_FOR_DELETION','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','VSS provider installation failed.','Contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('EP0941',0,'AGENT_INSTALL_VSS_INSTALLATION_FAILED_CSSCRIPT_ACCESS_DENIED','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Access denied as user account doesn’t have required permissions.','Please make sure that user account used for the replication has permission to execute script on the machine.','',0,'Yes',0,'API Errors','',86400),
('EP0942',0,'AGENT_INSTALL_VSS_INSTALLATION_FAILED_PATH_NOT_FOUND','Installation of the mobility services on server <SourceIP> failed with error code <ErrorCode>.','Windows System32 path is not set in system variables.','Please set the Windows/System32 path in PATH environment variable of the machine.','',0,'Yes',0,'API Errors','',86400),
('EP0887',0,'AGENT_INSTALL_FAILED_WITH_VSS_ISSUE','Protection could not be enabled for source machine.','This could be due to one of the of the below reasons. 1. Volume Shadow Copy VSS service Startup Type is set to Disabled on the source machine. 2. COM+ System Application COMSysApp service Startup Type is set to Disabled on the source machine. 3. Microsoft Distributed Transaction Coordinator Service MSDTC Startup Type is set to Disabled on the source machine. 4. COM+ enumeration failed on the source machine.',' 1. If Volume Shadow Copy VSS service Startup Type is set to Disabled, change it to Automatic. 2. If COM+ System Application COMSysApp service Startup Type is to Disabled, change it to Automatic. 3. If the Microsoft Distributed Transaction Coordinator Service MSDTC Startup Type is set to Disabled, change it to Automatic. 4. If the service Startup Types were already set to Automatic, check if COM+ enumeration succeeds. a. You check COM+ enumeration by Component Services comexp.msc b. Browse to Component Service -> Computers -> My Computer -> COM+ Application. You should be able to expand the System Application node and see the contents under that node. c. Ensure that Volume Shadow Copy Service is listed under Component Service -> Computers -> My Computer -> DCOM config 5. If any of the sub-steps in Step 4 fail please contact your System Administrator to resolve the issue.','',0,'Yes',0,'API Errors','',86400),
('EP0901',0,'AGENT_INSTALL_FAILED_ON_UNIFIED_SETUP','Protection could not be enabled.','The server is already running one of the Azure Site Recovery infrastructure Server role Configuration Server / Scaleout Process Server, Master Target Server.','Azure Site Recovery Infrastructure Servers cannot be protected. Please try to protect a different server.','',0,'Yes',0,'API Errors','',86400),
('EP0911',0,'AGENT_INSTALL_MASTER_TARGET_EXISTS','Enable Protection failed to install mobility service on <SourceIP> with error code <ErrorCode>. Master Target Server role is already installed.','This server is already configured as a Master Target server.','Master Target server cannot be protected using Azure Site Recovery.','',0,'Yes',0,'API Errors','',86400),
('EP0912',0,'AGENT_INSTALL_PLATFORM_MISMATCH','Enable Protection failed to install mobility service on <SourceIP> with error code <ErrorCode>.Virtualization platform mismatch detected.','The mobility service is installed on a Azure IaaS VM and you are trying to replicate it using a Configuration server.','You may uninstall mobility service and re-install it using the /Platform VMware flag.','',0,'Yes',0,'API Errors','',86400),
('EP0918',0,'AGENT_INSTALL_HOST_NAME_NOT_FOUND','Installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>.','The hostname is not set for this virtual machine.','Update the hostname of the Linux server and retry the operation.','',0,'Yes',0,'API Errors','',86400),
('EP0922',0,'AGENT_INSTALL_THIRDPARTY_NOTICES_NOT_AVAILABLE','Installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>.','The installer file seems to be corrupted.','Contact Microsoft support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('EP0923',0,'AGENT_INSTALL_LOCALE_NOT_AVAILABLE','Installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>.','Mobility Service Setup was unable to proceed as the English locale is not available.','Enable enUS.utf8 locale on this computer before re-trying the operation.','',0,'Yes',0,'API Errors','',86400),
('EP0944',0,'AGENT_INSTALL_MSI_INSTALLATION_FAILED','Installation of the mobility service to the source machine <SourceIP> failed with error code <ErrorCode>.','Installation of Mobility service ran into an internal error.','Retry the operation. If the problem persists, contact support.','',0,'Yes',0,'API Errors','',86400),
('EP0935',0,'AGENT_INSTALL_LINUX_INSUFFICIENT_DISK_SPACE','','','','',0,'Yes',0,'API Errors','',86400),
('EP0917',0,'AGENT_INSTALL_UPGRADE_PLATFORM_CHANGE_NOT_SUPPORTED','','','','',0,'Yes',0,'API Errors','',86400),
('EP0946',0,'Unable to backup initrd images.','Unable to backup initrd images.','Insufficient free space in the /etc volume to store the backups.','Ensure that there is free space on /etc volume and retry the operation.','',0,'Yes',0,'API Errors','',86400),
('EP0947',0,'Failed to restore original initrd images.','Critical error: Failed to restore original initrd images.','Insufficient free space in the installation directory.','Installer ran into a critical error: failed to restore original initrd images. Restore the images manually from the /etc/vxagent/orig_initrd_images folder. Failing to restore the original initrd images may cause the system not to boot. Contact support for further assistance.','',0,'Yes',0,'API Errors','',86400),
('EP0948',0,'Failed to generate an updated initrd image.','Failed to generate an updated initrd image.','Setup ran into an internal error.','Please contact support.','',0,'Yes',0,'API Errors','',86400),
('EP0949',0,'EC_PUSH_INSTALL_TIMED_OUT_WITHOUT_HEARTBEAT','Protection couldnt be enabled for the machine %SourceIP;.','InMage PushInstall service is not running on the process server %PsName;.','Start the service on the process server and retry the protection again.','',0,'Yes',0,'API Errors','',86400),
('EP0950',0,'EC_PUSH_INSTALL_TIMED_OUT_WITH_HEARTBEAT','Protection couldnt be enabled for the machine %SourceIP;.','Process server %PsName; might be busy due to which mobility service installation is taking more time.','Please wait for some time and retry the protection again. If the issue persists, please contact Microsoft support.','',0,'Yes',0,'API Errors','',86400),
('EP0951',0,'EC_PUSH_UPGRADE_TIMED_OUT_WITHOUT_HEARTBEAT','Mobility service upgrade failed on the machine %SourceIP;.','InMage PushInstall service is not running on the process server %PsName;.','Start the service on the process server and retry the operation again.  If the issue persists, please contact Microsoft support.','',0,'Yes',0,'API Errors','',86400),
('EP0952',0,'EC_PUSH_UPGRADE_TIMED_OUT_WITH_HEARTBEAT','Mobility service upgrade failed on the machine %SourceIP;.','Process server %PsName; might be busy due to which agent upgrade is taking more time.','Please wait for some time and retry the operation again. If the issue persists, please contact Microsoft support.','',0,'Yes',0,'API Errors','',86400),
('EA0436',0,'Agent health','Recovery tag generation for <FailedNodes> has failed 3 times consecutively.','-','-','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0437',0,'Agent health','Recovery tag generation for <FailedNodes> has failed 3 times consecutively.','-','-','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0438',0,'Agent health','Recovery tag generation for <FailedNodes> has failed 3 times consecutively.','-','-','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0440',0,'Agent health','The system clock on the machine <VMName> moved forward by <TimeJump> seconds at <TimeJumpedAt>. Recovery point labels and RPO measurements are based on the system time of the protected machine, and an inaccurate system clock will result in inaccurate RPO values and recovery point labels.','-','-','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0441',0,'Agent health','The system clock on the machine <VMName> moved behind by <TimeJump> seconds at <TimeJumpedAt>. Recovery point labels and RPO measurements are based on the system time of the protected machine, and an inaccurate system clock will result in inaccurate RPO values and recovery point labels.','-','-','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EP0955',0,'PushInstall_AccessDenied','Insufficient privileges failure - Mobility agent installation failed on the source machine %SourceIP;','Access was denied while copying the agent software to the source machine using the provided account %AccountName;.','1. Ensure that the credentials provided under user account %AccountName; have administrator privileges over the source machine. For detailed instructions, check https://aka.ms/installation_credential_failures. 2. To prevent additional issues, complete following checks before proceeding.Enable file and Printer sharing services (https://aka.ms/installation_File_printer_failures).Enable Windows Management Instrumentation(WMI) (https://aka.ms/installation_WMI_errors) for private, public and domain profiles.OR You can install mobility agent on the source machine through simple manual installation process (https://aka.ms/manualinstall) or by using Software deployment tools like SCCM (https://aka.ms/install-mobility-service-using-sccm).','',0,'Yes',0,'API Errors','',86400),
('EP0956',0,'PushInstall_DomainTrustRelationshipFailed','Insufficient privileges failure - Mobility agent installation failed on the source machine %SourceIP;.','Domain trust relationship establishment between the primary domain and workstation failed while trying to login to the source machine %SourceIP;.','1. Ensure that the credentials provided under user account <%AccountName;> have administrator privileges to login through primary domain of the source machine %SourceIP;. For detailed instructions, check https://aka.ms/installation_credential_failures.2. To prevent additional issues, complete following checks before proceeding. Enable file and Printer sharing services (https://aka.ms/installation_File_printer_failures).Enable Windows Management Instrumentation(WMI) https://aka.ms/installation_WMI_errors for private, public and domain profiles. OR You can install mobility agent on the source machine through simple manual installation process https://aka.ms/manualinstall or by using Software deployment tools like SCCM https://aka.ms/install-mobility-service-using-sccm.','',0,'Yes',0,'API Errors','',86400),
('EP0957',0,'PushInstall_AccountLoginDisabledOnSourceMachine','Login failure - Mobility agent installation failed on the source machine %SourceIP;.','Unable to login to the source machine %SourceIP; because the credentials provided under user account %AccountName; have been disabled.','1. Enable Login on the source machine for the account provided. For more information, refer https://docs.microsoft.com/en-us/powershell/module/microsoft.powershell.localaccounts/enable-localuser?view=powershell-5.1 or run this command in powershell:"net user <username> /active:yes".2. To prevent additional issues, complete following checks before proceeding. Credential check https://aka.ms/installation_credential_failures: Ensure you are using valid credentials with administrative privileges.Enable file and Printer sharing services https://aka.ms/installation_File_printer_failures. Enable Windows Management Instrumentation(WMI) https://aka.ms/installation_WMI_errors for private, public and domain profiles. OR You can install mobility agent on the source machine through simple manual installation process https://aka.ms/manualinstall or by using Software deployment tools like SCCM https://aka.ms/install-mobility-service-using-sccm.','',0,'Yes',0,'API Errors','',86400),
('EP0958',0,'PushInstall_LoginAccountLockedOut','Login failure - Mobility agent installation failed on the source machine %SourceIP;.','Unable to login to the source machine %SourceIP; because the credentials provided under user account %AccountName; have been locked out due to multiple failed retries to login.','1. Ensure that the provided credentials are accurate and have administrator privileges over the source machine. For detailed instructions, check https://aka.ms/installation_credential_failures. 2. Retry the operation after sometime. 3. To prevent additional issues, complete following checks before proceeding. Credential check(https://aka.ms/installation_credential_failures): Ensure you are using valid credentials with administrative privileges.Enable file and Printer sharing services(https://aka.ms/installation_File_printer_failures). Enable Windows Management Instrumentation(WMI) (https://aka.ms/installation_WMI_errors) for private, public and domain profiles. OR You can install mobility agent on the source machine through simple manual installation process(https://aka.ms/manualinstall) or by using Software deployment tools like SCCM(https://aka.ms/install-mobility-service-using-sccm).','',0,'Yes',0,'API Errors','',86400),
('EP0959',0,'PushInstall_LogonServersUnavailable','Login failure - Mobility agent installation failed on the source machine %SourceIP;.','Logon servers are not available on the source machine %SourceIP; to complete logon request.','1. Ensure that Logon servers are available on the source machine and start the Logon service for successful Login. For detailed instructions, check https://support.microsoft.com/en-in/help/139410/err-msg-there-are-currently-no-logon-servers-available. 2. To prevent additional issues, complete following checks before proceeding.Credential check (https://aka.ms/installation_credential_failures): Ensure you are using valid credentials with administrative privileges. Enable file and Printer sharing services (https://aka.ms/installation_File_printer_failures). Enable Windows Management Instrumentation(WMI)(https://aka.ms/installation_WMI_errors) for private, public and domain profiles. OR You can install mobility agent on the source machine through simple manual installation process (https://aka.ms/manualinstall) or by using Software deployment tools like SCCM(https://aka.ms/install-mobility-service-using-sccm).','',0,'Yes',0,'API Errors','',86400),
('EP0960',0,'PushInstall_UnableToStartLogonService','Login failure - Mobility agent installation failed on the source machine %SourceIP;.','Logon service is not running on the source machine %SourceIP; An attempt was made to start the logon service but could not succeed.','1. Ensure that Logon service is running on the source machine for successful login. To start the logon service, run this command: "net start Netlogon" or start "NetLogon" from task manager. 2. To prevent additional issues, complete following checks before proceeding. Credential check (https://aka.ms/installation_credential_failures): Ensure you are using valid credentials with administrative privileges. Enable file and Printer sharing services(https://aka.ms/installation_File_printer_failures). Enable Windows Management Instrumentation(WMI) (https://aka.ms/installation_WMI_errors) for private, public and domain profiles. OR You can install mobility agent on the source machine through simple manual installation process (https://aka.ms/manualinstall) or by using Software deployment tools like SCCM (https://aka.ms/install-mobility-service-using-sccm).','',0,'Yes',0,'API Errors','',86400),
('EP0961',0,'PushInstall_SourceNetworkUnavailable','Connectivity failure - Mobility agent installation failed on the source machine %SourceIP;.','Network in which the source machine %SourceIP; resides was not found or might have been deleted and is no longer available.','1. Ensure that source machine network exists. 2. Ensure you are able to ping your Source machine (Source IP) from the Configuration server or scale-out process server using following command "telnet CS/scale-out-PS-IP-address 143". Learn more (https://aka.ms/installation_connectivity_failures).   2. To prevent additional issues, complete following checks before proceeding. Credential check (https://aka.ms/installation_credential_failures): Ensure you are using valid credentials with administrative privileges. Enable file and Printer sharing services (https://aka.ms/installation_File_printer_failures). Enable Windows Management Instrumentation(WMI) (https://aka.ms/installation_WMI_errors) for private, public and domain profiles. OR You can install mobility agent on the source machine through simple manual installation process (https://aka.ms/manualinstall) or by using Software deployment tools like SCCM (https://aka.ms/install-mobility-service-using-sccm).','',0,'Yes',0,'API Errors','',86400),
('EP0962',0,'PushInstall_SpaceNotAvailableOnSource','Mobility agent installation failed on the source machine %SourceIP;','Source machine %SourceIP; does not contain enough space to store the mobility agent software.','Verify that the C: drive on the source machine has a minimum of 100 MB free space available and retry the operation.','',0,'Yes',0,'API Errors','',86400),
('EP0963',0,'PushInstall_NonRootAccountFound','Mobility agent installation failed on the source machine %SourceIP;.','Root account was not provided for enabling protection on the source machine %SourceIP;.','Verify that the account provided is the in-built root user account and retry the operation.','',0,'Yes',0,'API Errors','',86400),
('EA0442',0,'Agent health','App-consistent recovery tag generation for <FailedNodes> has failed.','VSS provider service is missing on <FailedNodes>. So, generation of application consistent recovery points has failed','1. Run the command "vssadmin list providers".Check if ASR VSS provider is missing. 2. If the VSS Provider is not present, then follow below given steps to install the ASR VSS service 1. Open admin cmd window. 2. Navigate to the mobility service installation location.(Eg - C:\Program Files (x86)\Microsoft Azure Site Recovery\agent) 3. Run the script InMageVSSProvider_Uninstall.cmd . This will uninstall the service if it already exists. 4. Run the script InMageVSSProvider_Install.cmd to install the VSS provider. Learn more at https://aka.ms/asr_vss_installation_failures','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('EA0443',0,'Agent health','App-consistent recovery tag generation for <FailedNodes> has failed.','VSS provider service is disabled on <FailedNodes>. So, generation of application consistent recovery points has failed','Go to service explorer and check if "Azure Site Recovery VSS Provider" service is installed and is running. If the service is disabled, please enable and start the service. Learn more at https://aka.ms/asr_vss_installation_failures','Agent',3,'Yes',0,'Alerts/Events','', 86400),
('ConfigurationServerDiskResize',0,'VM health','','','ConfigurationServerDiskResize','CS',3,'Yes',0,'Alerts/Events','', 86400),
('ConfigurationServerPsFailOver',0,'VM health','','','ConfigurationServerPsFailOver','CS',3,'Yes',0,'Alerts/Events','', 86400);

SET FOREIGN_KEY_CHECKS = 1;