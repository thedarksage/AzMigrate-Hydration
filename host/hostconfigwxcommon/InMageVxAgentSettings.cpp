#include "stdafx.h"
#include "hostconfigwxcommon.h"
#include "InMageVxAgentSettings.h"

IMPLEMENT_DYNAMIC(CInMageVxAgentSettings, CPropertyPage)

CInMageVxAgentSettings::CInMageVxAgentSettings()
	: CPropertyPage(CInMageVxAgentSettings::IDD)
	, m_cacheDirPath(_T(""))
	, m_stCacheDirChangeNote(_T("Note: Changing cache directory requires the following steps. Not following these steps can result into data loss.\n\n\
1. Stop svagents.\n\
2. Wait for svagent and child processes to stop completely.\n\
3. Create the new cache directory.\n\
4. Move contents from old cache directory to new cache directory.\n\
5. Update new cache directory path in above text field and click Apply.\n\
6. Start svagents."))
{
	m_bChangeAttempted = false;
}

CInMageVxAgentSettings::~CInMageVxAgentSettings()
{
}

void CInMageVxAgentSettings::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_CACHE_DIR_PATH, m_cacheDirPath);
	DDX_Text(pDX, IDC_STATIC_CHACHE_DIR_CHANGE_NOTE, m_stCacheDirChangeNote);
}


BEGIN_MESSAGE_MAP(CInMageVxAgentSettings, CPropertyPage)
	ON_EN_CHANGE(IDC_EDIT_CACHE_DIR_PATH, &CInMageVxAgentSettings::OnEnChangeEditCacheDirPath)
	ON_BN_CLICKED(IDC_BTN_BROWSE_CACHE_DIR, &CInMageVxAgentSettings::OnBnClickedBtnBrowseCacheDir)
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

/************************************************************
* Event handler for VX application cache directory change	*
* Enable Apply button if change attempted					*
************************************************************/
void CInMageVxAgentSettings::OnEnChangeEditCacheDirPath()
{
	CString newVxCacheDir;
	GetDlgItem(IDC_EDIT_CACHE_DIR_PATH)->GetWindowText(newVxCacheDir);
	if(newVxCacheDir.CompareNoCase(m_cacheDirPath))
	{
		m_bChangeAttempted = true;
		SetModified();
	}
}

/****************************************************
* Handles event when user browse cache directory	*
* Enable Apply button if change attempted			*
****************************************************/
void CInMageVxAgentSettings::OnBnClickedBtnBrowseCacheDir()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	BROWSEINFO vxCacheBrowseInfo;
	ZeroMemory(&vxCacheBrowseInfo,sizeof(vxCacheBrowseInfo));
	vxCacheBrowseInfo.hwndOwner = m_hWnd;
	vxCacheBrowseInfo.lpszTitle = _T("Select Application Cache Directory");
	vxCacheBrowseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

	HRESULT hrOleInit = OleInitialize(NULL);
	if(SUCCEEDED(hrOleInit))
	{
		LPITEMIDLIST vxCacheDirectory = SHBrowseForFolder(&vxCacheBrowseInfo);
		if(NULL != vxCacheDirectory)
		{
			TCHAR newVxCacheDir[MAX_PATH] = {0};
			if(SHGetPathFromIDList(vxCacheDirectory,newVxCacheDir))
			{
				m_cacheDirPath = newVxCacheDir;
				m_bChangeAttempted = true;
				SetModified();
				UpdateData(FALSE);
			}
			CoTaskMemFree(vxCacheDirectory);
		}
		OleUninitialize();
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Unable to browse folders. OLE initialization failed.\n");
		AfxMessageBox("Unable to browse folders. OLE initialization failed.\nPlease enter the cache path in provided text box and click Apply", MB_OK|MB_ICONERROR);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
}

/************************************************************
* Checks whether the config data is really changed or not	*
************************************************************/
bool CInMageVxAgentSettings::ValidateAgentConfigChange()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	if(!m_bChangeAttempted)
	{
		//Validate that the data is really modified.
		if(!CInmConfigData::GetInmConfigInstance().GetVxAgentSettings().vxAgentCacheDir.CompareNoCase(m_cacheDirPath))
		{
			DebugPrintf(SV_LOG_DEBUG, "No change attempted in Agent tab.\n");
			m_bChangeAttempted = false;
		}
		else
		{
			m_bChangeAttempted = true;
			DebugPrintf(SV_LOG_DEBUG, "Change attempted in Agent tab.\n");
			DebugPrintf(SV_LOG_DEBUG, "Old Cache directory path = %s\n", CInmConfigData::GetInmConfigInstance().GetVxAgentSettings().vxAgentCacheDir.GetString());
			DebugPrintf(SV_LOG_DEBUG, "New Cache directory path = %s\n", m_cacheDirPath.GetString());
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return m_bChangeAttempted;
}

/************************************************************************************
* Do config data exchange from control variable to structure to persist the data	*
************************************************************************************/
BOOL CInMageVxAgentSettings::SyncAgentConfigData(AGENT_SETTINGS &vxAgentSettings)
{
	if(m_cacheDirPath.IsEmpty())
	{
		DebugPrintf(SV_LOG_ERROR, "Application cache directory cannot be empty.\n");
		AfxMessageBox("Application cache directory cannot be empty.",MB_OK|MB_ICONERROR);
		return FALSE;
	}
	vxAgentSettings.vxAgentCacheDir = m_cacheDirPath;
	return TRUE;
}

/********************************************************************
* Do config data exchange from persisted data to control variables	*
********************************************************************/
void CInMageVxAgentSettings::SyncAgentConfigData()
{
	m_cacheDirPath = CInmConfigData::GetInmConfigInstance().GetVxAgentSettings().vxAgentCacheDir;
}

/********************************************************************
* Update and validate the config data when tab/page lose focus		*
********************************************************************/
BOOL CInMageVxAgentSettings::OnKillActive()
{
	UpdateData();
	ValidateAgentConfigChange();
	if(!m_bChangeAttempted)
		SetModified(FALSE);
	return CPropertyPage::OnKillActive();
}

bool CInMageVxAgentSettings::IsAgentConfigChanged()
{
	//ValidateAgentConfigChange();
	return m_bChangeAttempted;
}

void CInMageVxAgentSettings::ResetConfigChangeStatus(bool bStatus)
{
	m_bChangeAttempted = bStatus;
}

HBRUSH CInMageVxAgentSettings::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{   
   HBRUSH hBrushHandle = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);  
   if((pWnd->GetDlgCtrlID() == IDC_ASTERIC) || (pWnd->GetDlgCtrlID() == IDC_ASTERIC2))   
   {        
      pDC->SetTextColor(RGB(127, 0, 0)); 
   } 
   return hBrushHandle;
}

