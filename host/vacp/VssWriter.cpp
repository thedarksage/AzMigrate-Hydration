#include "stdafx.h"
using namespace std;

#include "util.h"
//#include "common.h"
#include "VssWriter.h"
#include "devicefilter.h"
#include "StreamEngine/StreamRecords.h"
#include "StreamEngine/StreamEngine.h"
#include "InMageVssProvider_i.c"
#include "InMageVssProvider_i.h"
#include "vacp.h"
#include "VacpErrorCodes.h"


#include <atlconv.h> // Use string conversion macros used in ATL

#define VOLGUID_SIZE_IN_CHARS	36

#if (_WIN32_WINNT > 0x500)

//We will not use Static InMageVssWriter now. Instead we will dynamically create GUID
//and use that GUID for InMageVSSWriter ID. This is required to support running
//multiple VACP simultaneously and to resolve bug#2867

//static VSS_ID InMageVssWriterGUID = {
    //0x12345678, 0xd741, 0x452a,
    //0x8c, 0xba, 0x99, 0xd7, 0x44, 0x00, 0x8c, 0x04
    //};
VSS_ID InMageVssWriterGUID;
unsigned char *gpInMageVssWriterGUID;
extern bool	gbSyncTag;
extern bool gbRemoteSend;
extern bool gbSkipUnProtected;
static LPCWSTR InMageVssWriterName = L"InMageVSSWriter";

//Set 30 min. timeout for writer...CHECK, this timeout is increased to 30 minutes to support Hyper-V environments
#define INMAGE_WRITER_TIMEOUT	(30 * 60 * 1000)
unsigned long writerCount = 0;
#if (_WIN32_WINNT > 0x500)
	extern CComPtr<IVacpProvider> pInMageVssProvider;
#endif
extern bool	gbTagSend;
extern HANDLE		ghControlDevice;

extern void inm_printf(const char * format, ...);

bool InMageVssWriter::Initialize()
{
    XDebugPrintf("ENTERED: InMageWriter::Initialize\n");

    HRESULT hr;

    do
    {
        if (m_bInitialized)
        break;

        m_bInitialized = false;

    	if (bCoInitializeSucceeded == false && bIsCoInitializedByCaller == false)
		{
			hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
			if (hr != S_OK)
			{
				if (hr == S_FALSE)
				{
					m_bInitialized = true;
					bCoInitializeSucceeded = true;
					hr = S_OK;
					break;
				}
                std::stringstream ss;
                ss << "FILE: " << __FILE__ << ", LINE " << __LINE__ << "FAILED: CoInitializeEx() failed. hr = " << std::hex << hr;
                DebugPrintf("%s \n.", ss.str().c_str());
                AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
                AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
				break;
			}

			hr = CoInitializeSecurity
				(
				NULL,
				-1,
				NULL,
				NULL,
				RPC_C_AUTHN_LEVEL_CONNECT,
				RPC_C_IMP_LEVEL_IMPERSONATE,
				NULL,
	            EOAC_NONE,
		        NULL
			    );
        
			// Check if the securiy has already been initialized by someone in the same process.
			if (hr == RPC_E_TOO_LATE)
			{
				hr = S_OK;
			}

			if (hr != S_OK)
			{
	            std::stringstream ss;
                ss << "FILE: " << __FILE__ << ", LINE " << __LINE__ << "FAILED: CoInitializeSecurity() failed. hr = " << std::hex << hr;
                DebugPrintf("%s \n.", ss.str().c_str());
                AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
                AppConsistency::Get().m_vacpLastError.m_errorCode = hr;

				CoUninitialize();
			    break;
			}
			
			bCoInitializeSucceeded = true;
		}
		
		//Dynamically create GUID dor InMageVssWriter
		hr = CoCreateGuid(&InMageVssWriterGUID);
		UuidToString(&InMageVssWriterGUID,(unsigned char**)&gpInMageVssWriterGUID);
		XDebugPrintf("InMageVssWriterGUID= %s\n",(char*)gpInMageVssWriterGUID);
		
        hr = CVssWriter::Initialize(InMageVssWriterGUID, InMageVssWriterName, VSS_UT_USERDATA, VSS_ST_OTHER, VSS_APP_SYSTEM, INMAGE_WRITER_TIMEOUT);

        if (hr != S_OK)
        {
            DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
            
			if (hr == 0x80070005)
			{
				std::stringstream ss;
                ss << "FILE: " << __FILE__ << ", LINE " << __LINE__ << "FAILED: CVssWriter::Initialize failed with ACCESS_DENIED error. hr = " << std::hex << hr
                    << "\nINFO: Please modify the user account to be a member of the local Administrators or Backup Operators group on the local computer";
                DebugPrintf("\n%s \n.", ss.str().c_str());
                AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
                AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
			}
			else
			{
				DebugPrintf( "FAILED: CVssWriter::Initialize failed. hr = 0x%08x \n", hr);
			}
            break;
        }

        hr = Subscribe((VSS_SM_BACKUP_EVENTS_FLAG | VSS_SM_RESTORE_EVENTS_FLAG));

        if (hr != S_OK)
        {
            std::stringstream ss;
            ss << "FILE: " << __FILE__ << ", LINE " << __LINE__ << "FAILED: subscribe failed. hr = " << std::hex << hr;
            DebugPrintf("\n%s \n.", ss.str().c_str());
            AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            break;
        }

        m_bInitialized = true;

    } while (false);

    XDebugPrintf("EXITED: InMageWriter::Initialize\n");
    
    return m_bInitialized;            
}

bool STDMETHODCALLTYPE InMageVssWriter::OnIdentify(IN IVssCreateWriterMetadata *pMetadata)
{
	static int counter = 0;
	using namespace std;
	USES_CONVERSION;
    HRESULT hr = S_OK;
    XDebugPrintf("ENTERED: InMageWriter::OnIdentify\n");
    do
    {
        m_uTotalComponentsAdded = 0;
		#if( _WIN32_WINNT >=0x502)
		hr = pMetadata->SetBackupSchema(VSS_BS_COPY);
		if( hr != S_OK)
		{
		  inm_printf("\n FILE: %s, LINE %d\n",__FILE__,__LINE__);
		  inm_printf("\n FAILED: IVssCreateWriterMetadata::SetBackupSchema() failed. hr = 0x%08x \n",hr);
		  break;
		}
		#endif

        hr = pMetadata->SetRestoreMethod(VSS_RME_CUSTOM, NULL, NULL, VSS_WRE_NEVER, false);

        if (hr != S_OK)
        {
            DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf( "FAILED: IVssCreateWriterMetadata::SetRestoreMethod() failed. hr = 0x%08x \n.", hr);
            break;
        }

		//Specify the VolumeName
		char szVolumeName[3];
		
		szVolumeName[0] = 'X';
		szVolumeName[1] = ':';
		szVolumeName[2] = '\0';

		//Specify the Root Directory
		char szRootDirectoryName[4];

		szRootDirectoryName[0] = 'X';
		szRootDirectoryName[1] = ':';
		szRootDirectoryName[2] = '\\';
		szRootDirectoryName[3] = '\0';

		//File specification. Use * to represent all files in the volume.
		char *szFilespec = "*"; 

		DWORD driveMask = m_dwProtectedDriveMask;

        for( int i = 0; i < MAX_DRIVES && driveMask ; i++ )
        {
            DWORD dwMask = 1<<i;

            if(0 == ( driveMask &  dwMask ) ) 
            {
                continue;
            }

			//Clear i-th bit
			driveMask &= ~(dwMask);

            szVolumeName[0] = 'A' + i;
            szRootDirectoryName[0] = 'A' + i;
            
            //Add recursively
            bool bRecursiveAdd = true;

			if(gbSkipUnProtected)
			{
				//Check if it is a valid volume (existing volume )or not
				if(!IsValidVolumeEx(szVolumeName))
				{
					inm_printf("\n %s is an invalid or non-existent volume and hence it will not be added in the snapshot set.\n",szVolumeName);
					continue;
				}
				char szVolumePathName[MAX_PATH] = {0};
				if(!GetVolumePathName((LPCSTR)szVolumeName,szVolumePathName,MAX_PATH))
				{
					//hr = S_FALSE;
					inm_printf("\nCould not get the Volume Path Name for %s and hence not selecting this component for backup!\n",szVolumeName);
					//break;
					continue;
				}
				//convert the VolumePathName to Volume GUID
				unsigned long GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
				WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
				if(!VolumeGUID)
				{
					continue;
				}
				else
				{
					memset((void*)VolumeGUID,0x00,GuidSize);
				}
				if(!MountPointToGUID((PTCHAR)szVolumePathName,VolumeGUID,GuidSize))
				{
					delete VolumeGUID;
					VolumeGUID = NULL;
					continue;
				}

				if(IsDriverLoaded(INMAGE_FILTER_DOS_DEVICE_NAME))
				{
					//Check if this volume is not-protected or a vsnap volume.In either case we should not add the component.
					etWriteOrderState DriverWriteOrderState;
					DWORD dwLastError = 0;
					if(!CheckDriverStat(ghControlDevice,VolumeGUID,&DriverWriteOrderState,&dwLastError))				
					{
						//inm_printf("\n Skipping %s as the volume is either a vsnap or it is not-protected!@Time: %s \n",szVolumeName,GetLocalSystemTime().c_str());
						continue;
					}
				}
				if(VolumeGUID)
				{
					delete VolumeGUID;
					VolumeGUID = NULL;
				}
			}
			
            XDebugPrintf("\nAdding Component %s \n", szVolumeName);

			bool bSelectable = true;
            hr = pMetadata->AddComponent(VSS_CT_FILEGROUP, NULL, A2CW(szVolumeName), NULL, NULL, 0, false, true, bSelectable);

            if (hr != S_OK)
            {
                DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf( "FAILED: IVssCreateWriterMetadata::AddComponent() failed. hr = 0x%08x \n.", hr);
                DebugPrintf( "FAILED: Skipping VolumeName %s  ...\n",szVolumeName);
                continue;
            }

            XDebugPrintf("Adding Files %s%s (recursively) \n", szRootDirectoryName, szFilespec);
			if(counter == 1)
				DebugPrintf("Preparing Files %s%s (recursively) \n", szRootDirectoryName, szFilespec);
            hr = pMetadata->AddFilesToFileGroup(NULL, A2CW(szVolumeName), A2CW(szRootDirectoryName), A2CW(szFilespec), bRecursiveAdd, NULL);

            if (hr != S_OK)
            {
                DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf( "FAILED: IVssCreateWriterMetadata::AddFilesToFileGroup() failed. hr = 0x%08x \n.", hr);
                DebugPrintf( "FAILED: Skipping VolumeName %s  ...\n", szVolumeName);
                continue;
            }

            m_uTotalComponentsAdded++;
        }

		string strVolMP;
		string strRootDir;
		for(unsigned  int j = 0; j < volMountPoint_info.dwCountVolMountPoint; j++ )
		{
			strVolMP =volMountPoint_info.vVolumeMountPoints[j].strVolumeMountPoint;
			strRootDir = volMountPoint_info.vVolumeMountPoints[j].strVolumeName;
			size_t stringSize = strRootDir.length();
			if(strRootDir.at(stringSize-1) != '\\')
			{
				strRootDir = strRootDir + "\\";
			}

			//Add recursively
			bool bRecursiveAdd = true;
			bool bSelectable = true;

			if(gbSkipUnProtected)//Skip UnProtected Volumes or vsnaps or drives which are non-existent!
			{
				XDebugPrintf("\nAdding Component %s \n", volMountPoint_info.vVolumeMountPoints[j].strVolumeName.c_str());

				//bool bSelectable = true;

				if(IsDriverLoaded(INMAGE_FILTER_DOS_DEVICE_NAME))
				{
					//Check if this volume is not-protected or a vsnap volume.In either case we should not add the component.
					etWriteOrderState DriverWriteOrderState;
					DWORD dwLastError = 0;
					std::string strGuid = strVolMP.substr(11,GUID_SIZE_IN_CHARS);
					
					if (!CheckDriverStat(ghControlDevice, A2W(strGuid.c_str()), &DriverWriteOrderState, &dwLastError))
					{
						//inm_printf("\n Skipping %s as the volume is either a vsnap or it is not-protected!@Time: %s \n",strRootDir.c_str(),GetLocalSystemTime().c_str());
						continue;
					}
				}
			}
			hr = pMetadata->AddComponent(VSS_CT_FILEGROUP, NULL, A2CW(strRootDir.c_str()), NULL, NULL, 0, false, true, bSelectable);

			if (hr != S_OK)
			{
				DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
				DebugPrintf( "FAILED: IVssCreateWriterMetadata::AddComponent() failed. hr = 0x%08x \n.", hr);
				DebugPrintf( "FAILED: Skipping VolumeName %s  ...\n",strRootDir.c_str());
				continue;
			}

			XDebugPrintf("Adding Files %s%s (recursively) \n", strRootDir.c_str(), szFilespec);
			if(counter == 1)
				DebugPrintf("Preparing Files %s%s (recursively) \n",strRootDir.c_str() , szFilespec);
			hr = pMetadata->AddFilesToFileGroup(NULL, A2CW(strRootDir.c_str()), A2CW(strRootDir.c_str()), A2CW(szFilespec), bRecursiveAdd, NULL);

			if (hr != S_OK)
			{
				DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
				DebugPrintf( "FAILED: IVssCreateWriterMetadata::AddFilesToFileGroup() failed. hr = 0x%08x \n.", hr);
				DebugPrintf( "FAILED: Skipping VolumeName %s  ...\n", strRootDir.c_str());
				continue;
			}
			m_uTotalComponentsAdded++;
		}
		//Add any components of applications whose affected volumes are zero 
		//(like SharePoint..because for SharePoint Farm Server, the DB components 
		//are on remote server and hence affected volumes locally are zero.)
		if(m_AppsSummary.size() > 0)
		{
		  vector<AppSummary_t>::iterator iter = m_AppsSummary.begin();
		  for(;iter != m_AppsSummary.end();iter++)
		  {
			if((0 == (*iter).totalVolumes) && ((*iter).m_vComponents.size()> 0))
			{
			  //Add each component
                vector<ComponentSummary_t>::iterator iterCmp = (*iter).m_vComponents.begin();
                for (; iterCmp != (*iter).m_vComponents.end(); iterCmp++)
			  {
				hr = pMetadata->AddComponent(VSS_CT_FILEGROUP/*VSS_CT_DATABASE*/,
											A2CW((*iterCmp).ComponentLogicalPath.c_str()),
											A2CW((*iterCmp).ComponentName.c_str()),
											A2CW((*iterCmp).ComponentCaption.c_str()),
											NULL,
											0,
											false,
											true,
											(*iterCmp).bIsSelectable);
				if (hr != S_OK)
				{
					DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
					DebugPrintf( "FAILED: IVssCreateWriterMetadata::AddComponent() failed. hr = 0x%08x \n.", hr);
					DebugPrintf( "FAILED: Skipping Component %s  ...\n",(*iterCmp).ComponentName);
					continue;
				}
				//Add to FileGroup
			  }
			}
		  }
		}
	} while (false);
    
    XDebugPrintf( "EXITED: InMageWriter::OnIdentify\n");
	counter++;
	XDebugPrintf( "InMageWriter::OnIdentify is called %d times\n",counter);
    return true;
}

bool STDMETHODCALLTYPE InMageVssWriter::OnPrepareBackup(IN IVssWriterComponents *pWriterComponents)
{
	using namespace std;
	USES_CONVERSION;    // declare locals used by the ATL macros

    HRESULT hr;
    unsigned cComponents;

    XDebugPrintf( "ENTERED: InMageVssWriter::OnPrepareBackup\n");

    switch(GetBackupType())
    {
        default:
            XDebugPrintf( "BackupType is = unknown\n");
            break;

        case VSS_BT_FULL:
            XDebugPrintf( "BackupType is = FULL\n");
            break;

        case VSS_BT_INCREMENTAL:
            XDebugPrintf( "BackupType is = Incremental\n");
            break;

        case VSS_BT_DIFFERENTIAL:
            XDebugPrintf( "BackupType is = Differential\n");
            break;

        case VSS_BT_LOG:
            XDebugPrintf( "BackupType is = Log\n");
            break;

		//VSS_BT_COPY is NOT defined in VSS SDK for Windows XP.
		//It is defined in VSS SDK for Windows 2003. Hence the
		//below statements are ifdef'd, so that Build for
		//Windows XP will not FAIL.
#if (_WIN32_WINNT >= 0x502)

        case VSS_BT_COPY:
            XDebugPrintf( "BackupType is = Copy\n");
            break;
#endif

        case VSS_BT_OTHER:
            XDebugPrintf( "BackupType is = Other\n");
            break;
    }

    XDebugPrintf( "AreComponentsSelected = %s\n", AreComponentsSelected() ? "yes" : "no");

    XDebugPrintf( "BootableSystemStateBackup = %s\n", IsBootableSystemStateBackedUp() ? "yes" : "no");
    
    do
    {

        //Break if no components were added in OnIdentify
        if (m_uTotalComponentsAdded == 0)
        {
            break;
        }

        hr = pWriterComponents->GetComponentCount(&cComponents);

        if (hr != S_OK)
        {
            DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf( "FAILED: IVssWriterComponents::GetComponentCount() failed. hr = 0x%08x \n.", hr);
            break;
        }

        XDebugPrintf("Number of components explicitly added= %d \n", cComponents);

        for(unsigned iComponent = 0; iComponent < cComponents; iComponent++)
        {
            CComPtr<IVssComponent> pComponent;
           

            hr = pWriterComponents->GetComponent(iComponent, &pComponent);

            if (hr != S_OK)
            {
                DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf( "FAILED: IVssComponent::GetComponent() failed. hr = 0x%08x \n.", hr);
                DebugPrintf( "FAILED: Skipping component %d\n",iComponent);
                continue;
            }
 
            CComBSTR bstrComponentName;

            hr = pComponent->GetComponentName(&bstrComponentName);

            if (hr != S_OK)
            {
                DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf( "FAILED: IVssComponent::GetComponentName() failed. hr = 0x%08x \n.", hr);
                DebugPrintf( "FAILED: Skipping component %d\n",iComponent);
                continue;
            }
            else
            {
                XDebugPrintf("Backing up component %s\n",W2CA(bstrComponentName));
            }
    
          }

    } while(false);

    XDebugPrintf( "EXITED: InMageVssWriter::OnPrepareBackup\n");
    
    return true;
}

bool STDMETHODCALLTYPE InMageVssWriter::OnPrepareSnapshot()
{
    XDebugPrintf( "ENTERED: InMageWriter::OnPrepareSnapshot\n");
    XDebugPrintf( "EXITED: InMageWriter::OnPrepareSnapshot\n");

    return true;
}


bool STDMETHODCALLTYPE InMageVssWriter::OnFreeze()
{
	bool bStatus = true;
    XDebugPrintf( "ENTERED: InMageWriter::OnFreeze\n");
	if (CallBackHandler != (void (*)(void *))NULL)
	{
		XDebugPrintf("Calling CallBackHandler ...\n");
		(*CallBackHandler)(CallBackContext);
		XDebugPrintf("CallBackHandler Returned\n");
	}
    XDebugPrintf( "EXITED: InMageWriter::OnFreeze\n");
    return bStatus;
}

bool STDMETHODCALLTYPE InMageVssWriter::OnThaw()
{
	bool bStatus = true;

    XDebugPrintf( "ENTERED: InMageWriter::OnThaw\n");

	//Code specific to InMageVssProvider
#if (_WIN32_WINNT > 0x500)
	HRESULT hr = S_OK;
	unsigned short ulTagSent = 0;;//non-zero value is success
	if(pInMageVssProvider)
	{
		hr = pInMageVssProvider->IsTagIssuedSuccessfullyDuringCommitTime(&ulTagSent);
		if(SUCCEEDED(hr))
		{
			if(ulTagSent > 0)
			{
				gbTagSend = true;
				inm_printf("\n Tag is issued successfully by InMageVssProvider.\n");
				bStatus = true;
			}
			else
			{
				gbTagSend = false;
				inm_printf("\n InMageVssProvider Failed to issue the tag successfully.\n");
				bStatus = false;
			}
		}
		else
		{
			hr = E_FAIL;
			bStatus = false;
			inm_printf("\n InMageVssProvider failed to determine if tags were issued successfully or not!\n");
		}			
	}//end of if(pInMageVssProvider)
#endif    
	
	XDebugPrintf( "EXITED: InMageWriter::OnThaw\n");
    return bStatus;
}

bool STDMETHODCALLTYPE InMageVssWriter::OnBackupComplete(IN IVssWriterComponents *pWriterComponents)
{
	using namespace std;
	USES_CONVERSION;    // declare locals used by the ATL macros

    HRESULT hr;
    unsigned cComponents;

    XDebugPrintf( "ENTERED: InMageWriter::OnBackupComplete\n");

    do
    {

        //Break if no components were added in OnIdentify
        if (m_uTotalComponentsAdded == 0)
        {
            break;
        }

        hr = pWriterComponents->GetComponentCount(&cComponents);

        if (hr != S_OK)
        {
            DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
            DebugPrintf( "FAILED: IVssWriterComponents::GetComponentCount() failed. hr = 0x%08x \n.", hr);
            break;
        }

        XDebugPrintf( "Number of components = %d \n", cComponents);

        for(unsigned iComponent = 0; iComponent < cComponents; iComponent++)
        {
            CComPtr<IVssComponent> pComponent;


            hr = pWriterComponents->GetComponent(iComponent, &pComponent);

            if (hr != S_OK)
            {
                DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf( "FAILED: IVssComponent::GetComponent() failed. hr = 0x%08x \n.", hr);
                DebugPrintf( "FAILED: Skipping component %d\n",iComponent);
                continue;
            }
 
            CComBSTR bstrComponentName;
            bool bBackupSucceeded;

            hr = pComponent->GetComponentName(&bstrComponentName);

            if (hr != S_OK)
            {
                DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf( "FAILED: IVssComponent::GetComponentName() failed. hr = 0x%08x \n.", hr);
                DebugPrintf( "FAILED: Skipping component %d\n",iComponent);
                continue;
            }
    
            hr = pComponent->GetBackupSucceeded(&bBackupSucceeded);

            if (hr != S_OK)
            {
                DebugPrintf( "FILE: %s, LINE %d\n",__FILE__, __LINE__);
                DebugPrintf( "FAILED: IVssComponent::GetBackupSucceeded() failed. hr = 0x%08x \n.", hr);
                DebugPrintf( "FAILED: Skipping component %s... \n", W2CA(bstrComponentName));
                continue;
            }

            if (bBackupSucceeded)
            {
                XDebugPrintf( "SUCCESS: Backup succeeded for component %s \n", W2CA(bstrComponentName));
            }
            else
            {
                XDebugPrintf( "FAILED: Backup failed for component %s \n", W2CA(bstrComponentName));
            }
            
        }
        
    } while (false);

    XDebugPrintf( "EXITED: InMageWriter::OnBackupComplete\n");
    return true;
}

bool STDMETHODCALLTYPE InMageVssWriter::OnBackupShutdown(IN VSS_ID SnapshotSetId)
{

    XDebugPrintf( "ENTERED: InMageWriter::OnBackupShutdown\n");
    XDebugPrintf( "EXITED: InMageWriter::OnBackupShutdown\n");

    return true;
}

bool STDMETHODCALLTYPE InMageVssWriter::OnPreRestore(IN IVssWriterComponents *pWriter)
{

    XDebugPrintf( "ENTERED: InMageWriter::OnPreRestore\n");
    XDebugPrintf( "EXITED: InMageWriter::OnPreRestore\n");

    return true;
}

bool STDMETHODCALLTYPE InMageVssWriter::OnPostRestore(IN IVssWriterComponents *pWriter)
{
    XDebugPrintf( "ENTERED: InMageWriter::OnPostRestore\n");
    XDebugPrintf( "EXITED: InMageWriter::OnPostRestore\n");

    return true;
}

bool STDMETHODCALLTYPE InMageVssWriter::OnAbort()
{
    XDebugPrintf( "ENTERED: InMageWriter::OnAbort\n");
    XDebugPrintf( "EXITED: InMageWriter::OnAbort\n");

    return true;
}

bool STDMETHODCALLTYPE InMageVssWriter::OnPostSnapshot(IN IVssWriterComponents *pWriter)
{

	XDebugPrintf( "ENTERED: InMageWriter::OnPostSnapshot\n");
	
	if(gbSyncTag)
	{
		HRESULT hr = NULL;
		hr = GetSyncTagVolumeStatus();
		if(hr != S_OK)
		{
			SetApplicationConsistencyState(false);
		}
		else
		{
			SetApplicationConsistencyState(true);
		}
	}
	else
	{
	  DebugPrintf("\nGetting TagVolumeStatus...\n");
	  HRESULT hr = NULL;
	  hr = GetTagVolumeStatus();
	  if(hr != S_OK)
	  {
		  SetApplicationConsistencyState(false);
	  }
	  else
	  {
		  SetApplicationConsistencyState(true);
	  }
	}

	XDebugPrintf( "EXITED: InMageWriter::OnPostSnapshot\n");

    return true;
}

/*void STDMETHODCALLTYPE InMageVssWriter::PrintLocalTime()
{
	LARGE_INTEGER performanceCounter;
	performanceCounter.QuadPart = 0;
	QueryPerformanceCounter(&performanceCounter);
	inm_printf("Performance Counter = %I64u\n",performanceCounter.QuadPart);
}*/
#endif
