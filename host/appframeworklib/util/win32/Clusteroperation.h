#ifndef CLUSTER_OPERATION_H
#define CLUSTER_OPERATION_H



#include <iostream>
#include <string>
#include <map>
#include <list>
#include <sstream>
#include "appglobals.h"
#include "controller.h"
#include "localconfigurator.h"
#include "util/common/util.h"
#include <clusapi.h>
#include <atlconv.h>
#include <ResApi.h>


#define INM_BUFFER_LEN 1024
#define INM_TYPE_BUFFER_LEN 1024
#define WIN2K8_SERVER_VER 0x6
#define RESOURCE_OFFLINE_ATTEMPTS  10	
#define RESOURCE_ONLINE_ATTEMPTS 10

class ClusterOp
{
private:
		USHORT  m_uTimeOut;
		std::string m_strCommandToExecute;
	    std::string m_stdOut;
	    LocalConfigurator m_objLocalConfigurator;
		std::string getClusUtilPath(bool &);
		void sanitizeCommmand(std::string &);
		std::string m_strStdOut;
		SVERROR offlineClusterResources(std::string& resourceName);
		
		SVERROR ClusterResource_Control( HRESOURCE hResource, DWORD dwControlCode, LPVOID& lpOutBuffer, DWORD& cbBuffSize, DWORD& bytesReturned );
		SVERROR persistClusterResourcesProp();
		std::list<RescoureInfo> m_resouceInfoList;

public:
    SVERROR onlineClusterResources(const std::string&,std::string& errorLog);
	ClusterOp(std::string strCommand, SV_UINT timeout)
	{
		m_strCommandToExecute     += std::string(" "); //For Command separator
		m_strCommandToExecute += strCommand;
		m_uTimeOut = timeout ;
	}
	ClusterOp()
	{
		m_uTimeOut = 0;
	}
	SVERROR offlineResources();
    SVERROR getCLusterResourceInfo(std::string clusterName);
	bool breakCluster(const std::string &,SV_ULONG& exitCode) ;	
	bool constructCluster(const std::string &ouputFileName,SV_ULONG &exitCode);
	std::string stdOut() { return m_stdOut;}

	SVERROR findNodeState(const std::string&,const std::string& ,bool &);
	bool allClusterNodesUp(const std::string& );

	SVERROR persistClusterResourceState(); // This will serialise resouces after bringing them offline.
    SVERROR revertResourcesToOriginalState(std::string& result);// This call will unserialise resouces and bring them online (and their dependent Resources)
    bool BringW2k8ClusterDiskOnline(const std::string &outputFileName,SV_ULONG &exitCode);
    bool OfflineorOnlineResource(const std::string& resourceType, const std::string& appName,std::string m_VirtualServerName,bool bMakeOnline);
    SVERROR getClusterResources(std::list<RescoureInfo>& resouceInfoList);
    bool checkClusterResourceAvailable();
} ;

#endif