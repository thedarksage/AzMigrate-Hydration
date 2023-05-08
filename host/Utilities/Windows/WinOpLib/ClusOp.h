#ifndef _CLUS_OP_H
#define _CLUS_OP_H

#include "Windows.h"
#include "tchar.h"
#include "strsafe.h"
#include "AtlConv.h"
#include "ClusApi.h"
#include "ResApi.h"

using namespace std;

#define OPTION_CHECK_ACTIVE_NODE	"CheckActiveNode"
#define CLUSTER_NETWORK_NAME_RESOURCE_TYPE		 "Network Name"
#define CLUSTER_RESOURCE_NETOWRK_NAME_PROPERTY	 "Name"

enum operationToPerformOnCluster
{ 
	FIND_VIRTUAL_SERVER_GROUP = 0, 
	FIND_FILESHARES_VOLUMES_OF_VIRTUALSERVER, 
	OFFLINE_VIRTUALSERVER_RESOURCE
};

//The ClusOpMain function
int ClusOpMain(int argc, char* argv[]);

class ClusOp
{
private:
	//Data Members
	DWORD ClusDocEx_DEFAULT_CB;
	DWORD	dwIndex;
	DWORD   cbNameSize;
	DWORD   cbResultSize;
	LPDWORD lpcbResultSize;

	DWORD	cbOutBufferSize;
	BYTE	bOutBuffer[8192];//MCHECK a huge number to capture lists
	LPVOID lpOutBuffer;
	DWORD	dwEnum;
	DWORD	cbEntrySize;
	DWORD	cchNameSize;
	DWORD	cbNameAlloc;
	DWORD	dwType;
	CLUSPROP_BUFFER_HELPER cbh;
	HCLUSENUM hClusEnum;
	HRESOURCE hResource;
	HCLUSTER hCluster;
	HNODE hNode;
	string strVirtualServerName;
	wstring ClusterResourceGroup;
	WCHAR szName[512];//Used to store the Resource Group in which the Virtual Server resides

	//Member Functions
	DWORD ListEntrySize( DWORD cbDataLength);
	BOOL GetResNameAndCompareWithVServer(LPWSTR lpszResourceName,LPWSTR lpszVirtualServerName);
	bool DetermineActiveness(LPWSTR lpszGroupName);
	bool FindResGroupForVServer(const string &strHost,
						const string resourceType,
						const string strPropertyToBeFetched);
	int IsActiveClusterNode(LPWSTR lpszVirtualServerName = NULL, LPWSTR lpszNodeName = NULL);
	bool m_bIsCluster;
	
public:	
	ClusOp();
	~ClusOp();
	//if the NodeName is NULL, then the current node's hostname is passed
	int ClusOp::CheckClustActiveness(string strVirSer);
	void ClusOp_Usage();

};

#endif