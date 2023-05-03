#ifndef __CLUSTER_UTIL__H
#define __CLUSTER_UTIL__H
#include "appglobals.h"
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <clusapi.h>
#include <atlconv.h>
#include <ResApi.h>
#include <map>

//Quorum Types for windows 2008 and above versions of windows cluster
#define CLUSTER_QUORUM_TYPE_NODE_MAJORITY			"Node Majority"
#define CLUSTER_QUORUM_TYPE_NODE_DISK_MAJORITY		"Node and Disk Majority"
#define CLUSTER_QUORUM_TYPE_NODE_FS_MAJORITY		"Node and File Share Majority"
#define CLUSTER_QUORUM_TYPE_NO_MAJORITY				"No Majority"


enum NODECLUSTERSTATUS
{
	CLUSTER_STATE_NOT_INSTALLED,
    CLUSTER_STATE_NOT_CONFIGURED,
	CLUSTER_STATE_NOT_RUNNING,
	CLUSTER_STATE_RUNNING
} ;
struct ResourceInfo
{
	CLUSTER_RESOURCE_STATE  m_resourceStatus;
	std::string m_ownerNodeName;
	std::string m_ownerGroup;
	void clean()
	{
		m_resourceStatus = ClusterResourceStateUnknown;
		m_ownerNodeName = "";
		m_ownerGroup = "";
	}
} ;
struct NetworkResourceInfo
{
	std::string m_nwName ;
	ResourceInfo m_resInfo ;
	std::list<std::string> m_dependendentIpList;
    std::string m_ErrorString;
	void clean()
	{
		m_nwName = "";
		m_resInfo.clean();
		m_dependendentIpList.clear();
		m_ErrorString = "";
	}
	~NetworkResourceInfo()
	{
		clean();
	}
 };

struct ClusterResourceInformation
{
	std::string id;
	std::string name;
	std::string type;
	std::string errorMsg;
	DWORD state;
	std::vector<std::string> dependencies;
	std::map<std::string,std::string> otherProp;
	ClusterResourceInformation()
		:state(0) {}
};

struct ClusterGroupInformation
{
	std::string id;
	std::string name;
	std::string errorMsg;
	std::string ownerNode;
	WORD state;
	std::vector<std::string> prefOwners;
	std::vector<ClusterResourceInformation> resources;
	ClusterGroupInformation()
		:state(-1) {}
};

struct ClusterInformation
{
	std::string m_nodeName;
	std::string m_clusterName; 	
	std::string m_clusterIP;
	std::string m_clusterID;
	std::map<std::string, NetworkResourceInfo> m_networkInfoMap ;
	std::list<std::string> m_nodes;
	std::vector<ClusterGroupInformation> m_groupsInfo;
	struct {
		std::string type;
		std::string path;
		std::string resource;
	}m_quorumInfo;
    void clean()
    {
		m_nodeName = "";
		m_clusterName = "";
		m_clusterIP = "";
		m_clusterID = "";
        m_networkInfoMap.clear();
		m_nodes.clear();
		m_groupsInfo.clear();
		m_ErrorString = "";
		m_ErrorCode = 0;
    }
	~ClusterInformation()
	{
		clean();
	}
    std::string m_ErrorString;
    long m_ErrorCode;
} ;

bool isClusterNode() ;
SVERROR getClusterStatus( NODECLUSTERSTATUS& status, std::string nodeName = "" );
bool isClusterStateOk() ;
static SVERROR ClusterResource_Control( HRESOURCE hResource, DWORD dwControlCode, LPVOID& lpszType, DWORD& cbBuffSize, DWORD& bytesReturned );
BOOL ResourceTypeEqual(const std::wstring & resourceName, const std::string & resourceType, HCLUSTER hClus = NULL);
SVERROR getResourcesByType(const std::string& clusterName, const std::string& resourceTypeName, std::list<std::string>& resourceNameList) ;
SVERROR getResourcesByTypeFromGroup(const std::string & resourceName, const std::string& resoruceTypeName, std::list<std::string> & ResourceNameList,const std::string& clusterName = "");
SVERROR getResourcePropertiesMap( std::string, std::string, std::list<std::string>&, std::map<std::string, std::string>& );
SVERROR getDependedResorceNameListOfType( std::string dependentResourceName, std::string dependedResourceType, std::list<std::string>& dependedResourceNameList );
bool getClusterName( std::string& );
SVERROR getClusterIpAddress(const std::string& clustername, std::string& );
SVERROR getResourceState( std::string clusterName, std::string, CLUSTER_RESOURCE_STATE&, std::string&, std::string& );
SVERROR getDependencyList( std::string query, std::list<std::string>& ); 

SVERROR getClusterInfo( ClusterInformation& );
SVERROR getClusterQuorumInfo(ClusterInformation& );
SVERROR getNodesPresentInCluster(const std::string& clusterName, std::list<std::string>& resourceNameList);
static SVERROR ClusterNode_Control( HNODE hNode, DWORD dwControlCode, LPVOID& lpszType, DWORD& cbBuffSize, DWORD& bytesReturned );
bool GetNameId(std::string&) ;


inline
DWORD
WINAPI
ClusHlpr_ListEntrySize
(
    DWORD cbDataLength
)
{
    return
    (
        sizeof( CLUSPROP_VALUE ) +     // Syntax and length.
        ALIGN_CLUSPROP( cbDataLength ) // Data and padding.
    );
}
inline std::string ClusterGroupStateMsg(DWORD dwState)
{
	switch(dwState)
	{
	case ClusterGroupOnline:
		return "ClusterGroupOnline";
	case ClusterGroupOffline:
		return "ClusterGroupOffline";
	case ClusterGroupFailed:
		return "ClusterGroupFailed";
	case ClusterGroupPartialOnline:
		return "ClusterGroupPartialOnline";
	case ClusterGroupPending:
		return "ClusterGroupPending";
	default:
		return "ClusterGroupStateUnknown";
	}
}
inline std::string ClusterResourceStateMsg(DWORD dwState)
{
	switch(dwState)
	{
	case ClusterResourceInitializing:
		return "ClusterResourceInitializing";
	case ClusterResourceOnline:
		return "ClusterResourceOnline";
	case ClusterResourceOffline:
		return "ClusterResourceOffline";
	case ClusterResourceFailed:
		return "ClusterResourceFailed";
	case ClusterResourcePending:
		return "ClusterResourcePending";
	case ClusterResourceOnlinePending:
		return "ClusterResourceOnlinePending";
	case ClusterResourceOfflinePending:
		return "ClusterResourceOfflinePending";
	default:
		return "ClusterResourceStateUnknown";
	}
}
DWORD ClusUtil_ClusterResourceControl(
									  HRESOURCE hRes,
									  const DWORD dwControlCode,
									  LPVOID *lpOutBuff,
									  DWORD & bytesReturned,
									  LPVOID lpInBuff=NULL,
									  DWORD cbInBuff=0
									  );
DWORD GetClusterEnumObjects(HCLUSTER hCluster, 
							const DWORD dwClusEnumType,
							std::vector<std::wstring> & objects
							);
DWORD GetClusterGroupContents(HCLUSTER hCluster,
							  const std::wstring & grpName,
							  std::vector<std::wstring>& grpContents,
							  DWORD dwType=CLUSTER_GROUP_ENUM_CONTAINS
							  );
class ClusterGroupOp
{
public:
	ClusterGroupOp(std::string ClusterName = "")
		:m_hCluster(NULL),m_dwError(0)
	{
		USES_CONVERSION;
		m_hCluster = OpenCluster( ClusterName.empty() ? NULL : A2W(ClusterName.c_str()) );
		if(NULL == m_hCluster)
			m_dwError = GetLastError();
	}
	
	~ClusterGroupOp()
	{
		Close();
	}
	
	void Close()
	{
		if(NULL != m_hCluster){
			CloseCluster(m_hCluster);
			m_hCluster = NULL;
		}
	}

	bool isConnected() { return NULL == m_hCluster ? false : true ;}

	DWORD GetError() const { return m_dwError; }

	DWORD GetClusGroupsInfo(std::vector<ClusterGroupInformation> & ClusGroups);

private:
	DWORD GetClusterGroups(std::vector<std::wstring> & groups);	
	
	void FillClusterGroupNodes(const std::wstring & grpName ,ClusterGroupInformation & info);
	
	void FillClusterGroupResourcesInfo(
		const std::wstring & grpName, 
		ClusterGroupInformation & info);
	
	void FillClusterGroupDetails(const std::wstring & grpName, ClusterGroupInformation & info);

	HCLUSTER m_hCluster;
	DWORD	 m_dwError;
};

class ClusterResourceOp
{
public:
	ClusterResourceOp(HCLUSTER hCluster, std::wstring wszResource)
		:m_hRes(NULL),m_dwError(0)
	{
		if(hCluster && !wszResource.empty())
		{
			m_wszResource = wszResource;
			m_hRes = OpenClusterResource(hCluster,wszResource.c_str());
			if(NULL == m_hRes)
				m_dwError = GetLastError();
		}
	}

	bool isOpened(){ return NULL == m_hRes ? false : true ; }

	void GetResourceInformation(ClusterResourceInformation & info)
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
		USES_CONVERSION;
		info.name = W2A(m_wszResource.c_str());
		
		if(!isOpened())
		{
			char szError[16];
			info.errorMsg = "Open Resource error ";
			info.errorMsg = ultoa(m_dwError,szError,10);
			info.errorMsg = ";";
			return ;
		}
		
		info.state = GetClusterResourceState(m_hRes,NULL,0,NULL,0);
		
		FillResourceId(info);
		
		FillResourceType(info);
		
		FillResourcePrivateDetails(info);

		FillResourceDependencies(info);
	}

	DWORD GetError() const
	{
		return m_dwError;
	}

	~ClusterResourceOp()
	{
		close();
	}
private:
	void FillFileShareWitnessInfo(ClusterResourceInformation & info);
	void FillNetworkNameInfo(ClusterResourceInformation & info);
	void FillIPAddressInfo(ClusterResourceInformation & info);
	void FillDiskInfo(ClusterResourceInformation & info);
	void FillResourceType(ClusterResourceInformation & info);
	void FillResourceId(ClusterResourceInformation & info);
	void FillResourceState(ClusterResourceInformation & info);
	void FillResourcePrivateDetails(ClusterResourceInformation & info);
	void FillResourceDependencies(ClusterResourceInformation & info);
	DWORD GetSzProperty(DWORD dwControlCode, 
		const std::string & szPropName,
		std::string & szValue);
	void close() 
	{ 
		if(m_hRes){
			CloseClusterResource(m_hRes);
			m_hRes = NULL;
		}
	}
	HRESOURCE m_hRes;
	std::wstring m_wszResource;
	DWORD m_dwError;
};

#endif