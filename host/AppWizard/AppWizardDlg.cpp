// AppWizardDlg.cpp : implementation file
//

#include "stdafx.h"
#include "AppWizard.h"
#include "AppWizardDlg.h"
#include ".\appwizarddlg.h"
#include "applicationsettings.h"
#include "AppData.h"
#include "exchange/exchangeapplication.h"
#include "ruleengine.h"
#include "ruleengine/validator.h"
#include "fileserver/fileserverdiscovery.h"
#include <sstream>
#include "config/appconfigurator.h"
#include "configvalueobj.h"
#include "configuratorrpc.h"
#include "proxy.h"
#include "config/applocalconfigurator.h"
#include "host.h"
#include <boost/lexical_cast.hpp>
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
extern std::list<std::string> installedApps ;

#define POPULATE_SYS_DETAILS			1
#define POPULATE_APPLICATION_DETAILS	2
#define POPULATE_VERSION_DETAILS		3
#define POPULATE_INSTANCE_DETAILS		4
#define POPULATE_DB_DETAILS				5
#define POPULATE_STORAGEGROUP_DETAILS	6
#define POPULATE_MAILBOX_DETAILS		7
#define POPULATE_FILESERVER_DETAILS		8
#define POPULATE_SERVNTNAME_DETAILS		9
#define POPULATE_BES_DETAILS		10

extern CInitializerDlg intlDlg;
extern CDialogeProcessing m_dlgProcessing;
extern CEvent g_dlgEvent;
extern UINT InitDiscAppsThreadFunc (LPVOID pParam);

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

UINT discAppInfoThreadFunc (LPVOID pParam)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	g_dlgEvent.Lock();
	m_dlgProcessing.m_strTitle = _T("InMage Scout - Discovering Applications Metadata...");
    performDiscovery() ;
	m_dlgProcessing.PostMessage(DISC_MSG_STEP_FINISH,(WPARAM)0,(LPARAM)0);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
    return 1;
}

UINT ValidateAppThreadFunc (LPVOID pParam)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	g_dlgEvent.Lock();
	m_dlgProcessing.m_strTitle = _T("InMage Scout - Validating Application's Pre-Requisites...");
	m_dlgProcessing.PostMessage(SET_TITLE,(WPARAM)0,(LPARAM)0);
	AppData::GetInstance()->ValidateRules();
	Sleep(500);
	m_dlgProcessing.PostMessage(VALIDATE_APP_FINISH,(WPARAM)0,(LPARAM)0);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
    return 1;
}
UINT GenerateSummaryThreadFunc(LPVOID  pParam)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	g_dlgEvent.Lock();
    m_dlgProcessing.m_strTitle = _T("InMage Scout - Generating Application Readiness Wizard's Summary...");
	m_dlgProcessing.PostMessage(SET_TITLE,(WPARAM)0,(LPARAM)0);

	AppData::GetInstance()->GenerateSummary();
	Sleep(500);
	m_dlgProcessing.PostMessage(GENERATE_SUMMARY_FINISH,(WPARAM)0,(LPARAM)0);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return 1;
}

void UpdateToCX()
{
	AppData& appdata = *AppData::GetInstance();
	appdata.m_error = RWIZARD_CX_UPATE_FAILED;
	AppLocalConfigurator config;
    std::string biosId("");
    GetBiosId(biosId);

    const ConfiguratorDeferredProcedureCall dpc = marshalCxCall("::getVxAgent", config.getHostId(), biosId);

	const DiscoveryInfo &discInfo = *(appdata.m_discoveyInfo);
	string DiscMarshalString = marshalCxCall(dpc , "UpdateDiscoveryInfo" , discInfo);
	//Persist update string into file before sending to CX.
	CStdioFile cxUpdateStrFile;
	if(cxUpdateStrFile.Open("ReadinessWizardCXUpdate.txt",CFile::modeWrite | CFile::modeCreate | CFile::modeNoTruncate))
	{
		cxUpdateStrFile.SeekToEnd();
		cxUpdateStrFile.WriteString(DiscMarshalString.c_str());
		cxUpdateStrFile.WriteString("\n\n");
		cxUpdateStrFile.Close();
	}
	string strRet;
	try
	{
		TalWrapper tal;
		strRet = tal(DiscMarshalString);
	}
	catch(std::exception & e)
	{
		appdata.m_error = RWIZARD_CX_UPATE_TRASPORT_EXCEPTION;
		appdata.m_ErrorMsg = e.what();
		DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		appdata.m_error = RWIZARD_CX_UPATE_UNKNOWN_EXCEPTION;
		appdata.m_ErrorMsg = "Encountered Unknown exception in CX trasport.";
		DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
	}

	InmPolicyUpdateStatus ret = POLICY_UPDATE_FAILED;
	
	try
	{
		if(!strRet.empty())
		{
			ret = unmarshal<InmPolicyUpdateStatus> (strRet);
			DebugPrintf(SV_LOG_DEBUG,"CX Update return code: %d",(int)ret);
			if(POLICY_UPDATE_COMPLETED == ret)
			{
				appdata.m_error = RWIZARD_CX_UPATE_SUCCESS;
				appdata.m_ErrorMsg = "Update successful";
			}
			else if (POLICY_UPDATE_DUPLICATE == ret)
			{
				appdata.m_error = RWIZARD_CX_UPATE_DUPLICATE;
				appdata.m_ErrorMsg = "Duplicate update";
			}
			else if (POLICY_UPDATE_DELETED == ret)
			{
				appdata.m_error = RWIZARD_CX_UPATE_DELETED;
				appdata.m_ErrorMsg = "Upade deleted";
			}
			else //POLICY_UPDATE_FAILED
			{
				appdata.m_ErrorMsg = "Update failed";
			}
		}
		else
		{
			appdata.m_error = RWIZARD_CX_UPATE_TRASPORT_NO_RESULT;
		}
	}
	catch( ContextualException& ce )
    {
		appdata.m_error = RWIZARD_CX_UPATE_UNMARSHAL_EXCEPTION;
		appdata.m_ErrorMsg = ce.what();
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch( const std::exception & e )
    {
		appdata.m_error = RWIZARD_CX_UPATE_UNMARSHAL_EXCEPTION;
		appdata.m_ErrorMsg = e.what();
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
		appdata.m_error = RWIZARD_CX_UPATE_UNKNOWN_EXCEPTION;
		appdata.m_ErrorMsg = "Encountered Unknown exception in unmarshaling CX trasport result.";
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }
}

UINT UpdateToCXThreadFunc(LPVOID pParam)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	g_dlgEvent.Lock();
	m_dlgProcessing.m_strTitle = _T("InMage Scout - Updating discovery information to CX ...");
	m_dlgProcessing.PostMessage(SET_TITLE,(WPARAM)0,(LPARAM)0);
	UpdateToCX();
	Sleep(500);
	m_dlgProcessing.PostMessage(REPORT_TO_CX_FINISHED,(WPARAM)0,(LPARAM)0);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	return 1;
}

CAppWizardDlg::CAppWizardDlg(CWnd* pParent)
	: CDialog(CAppWizardDlg::IDD, pParent)
	, m_commentsEdit(_T(""))
	, m_flagValidate(false)
	, m_flagStep1(false)
	, m_validatonSummary(_T(""))
{
	m_hInmIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CAppWizardDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE_APP, m_Inmapptree);
	DDX_Control(pDX, IDC_LIST_APP, m_InmappDetails);
	DDX_Text(pDX, IDC_RICHEDIT2_COMMENTS, m_commentsEdit);
	DDX_Control(pDX, IDC_BUTTON_VALIDATE, m_InmbtnValidate);
	DDX_Control(pDX, IDC_BUTTON_BACK, m_btnBack);
	DDX_Control(pDX, IDC_BUTTON_REDISCOVER, m_btnRediscover);
	DDX_Control(pDX, IDC_STATIC_WZRD_STATE, m_groupWzrdState);
	DDX_Text(pDX, IDC_RICHEDIT2_SUMMARY, m_validatonSummary);
}

BEGIN_MESSAGE_MAP(CAppWizardDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_APP, OnTvnSelchangedTreeApp)
	ON_BN_CLICKED(IDC_BUTTON_VALIDATE, OnBnClickedButtonValidate)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_APP, OnLvnItemchangedListApp)
	ON_BN_CLICKED(IDC_BUTTON_BACK, OnBnClickedButtonBack)
	ON_BN_CLICKED(IDC_BUTTON_REDISCOVER, OnBnClickedButtonRediscover)
	ON_BN_CLICKED(ID_CLOSE, OnBnClickedClose)
	ON_NOTIFY(NM_CLICK, IDC_TREE_APP, &CAppWizardDlg::OnNMClickTreeApp)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CAppWizardDlg message handlers

BOOL CAppWizardDlg::OnInitDialog()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	CDialog::OnInitDialog();
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}
	
	SetIcon(m_hInmIcon, TRUE);	
	SetIcon(m_hInmIcon, FALSE);
	SetWindowText(_T("InMage Scout Application Readiness Wizard"));
	ModifyStyle(WS_MAXIMIZEBOX,WS_MINIMIZEBOX);

	m_InmappTreeImageList.Create(IDB_BITMAP_TREE,16,1,RGB(255,255,255));
	m_Inmapptree.SetImageList(&m_InmappTreeImageList,TVSIL_NORMAL);

	m_InmappDetails.SetImageList(&m_appListCtrlImageList,LVSIL_SMALL);

	DWORD dwExStyles = m_InmappDetails.GetExtendedStyle();
	
	dwExStyles |= LVS_EX_FULLROWSELECT;
	m_InmappDetails.SetExtendedStyle(dwExStyles);

	CFont newFont;
	newFont.CreatePointFont(80,_T("Times New Roman"));

	m_InmappDetails.InsertColumn(0,_T(" "));
	m_InmappDetails.InsertColumn(1,_T(" "));

	displayDiscApps();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);	
	return TRUE;
}

void CAppWizardDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
	if(nID == SC_CLOSE)
		EndDialog(0);
}

void CAppWizardDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);

		CRect rect;
		GetClientRect(&rect);

		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hInmIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

HCURSOR CAppWizardDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hInmIcon);
}
void CAppWizardDlg::displayDiscApps()
{
	std::string hostName = Host::GetInstance().GetHostName();
	m_flagValidate = false;
	m_flagStep1 = true;
	m_InmbtnValidate.SetWindowText(_T("Next "));
	m_InmbtnValidate.EnableWindow(true);
	m_btnBack.EnableWindow(false);
	m_btnRediscover.ShowWindow(SW_HIDE);
	m_InmappDetails.DeleteAllItems();
	m_Inmapptree.DeleteAllItems();
	LVCOLUMN lvclmn;
	lvclmn.mask = LVCF_TEXT|LVCF_WIDTH;
	lvclmn.pszText = _T("Property");
	lvclmn.cx = 150;
	m_InmappDetails.SetColumn(0,&lvclmn);
	lvclmn.pszText = _T("Value");
	lvclmn.cx = 370;
	m_InmappDetails.SetColumn(1,&lvclmn);
	m_groupWzrdState.SetWindowText(_T("List Of Applications Installed"));
	m_commentsEdit.Format(_T("Welcome to InMage Scout Application Readiness Wizard"));
	UpdateData(false);
	m_Inmapptree.DeleteAllItems();// Usefull in case of re-discovery
	m_Inmapptree.ModifyStyle(0,TVS_CHECKBOXES);
	HTREEITEM root = m_Inmapptree.InsertItem(_T(hostName.c_str()),0,0);
	if( root != NULL )
	{
		m_Inmapptree.SetCheck(root);
    getExistedAppList() ;
    std::list<string>::iterator installedAppIter = installedApps.begin() ;
    while(installedAppIter != installedApps.end())
	{
			HTREEITEM aItem = m_Inmapptree.InsertItem(_T(installedAppIter->c_str()),2,3,root);
			if( aItem != NULL )
			{
				m_Inmapptree.SetCheck(aItem);
			}
	        
        installedAppIter++ ;
	}
	m_Inmapptree.Expand(root,TVE_EXPAND);
}
}
void CAppWizardDlg::displayDiscInfo()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	m_flagValidate = false;
	m_flagStep1 = false;
	m_Inmapptree.ModifyStyle(TVS_CHECKBOXES,0);
	m_InmbtnValidate.SetWindowText(_T("Next"));
	m_btnBack.EnableWindow(true);
	m_btnRediscover.ShowWindow(SW_SHOW);
	m_InmappDetails.DeleteAllItems();
	LVCOLUMN lvclmn;
	lvclmn.mask = LVCF_TEXT|LVCF_WIDTH;
	lvclmn.pszText = _T("Property");
	lvclmn.cx = 150;
	m_InmappDetails.SetColumn(0,&lvclmn);
	lvclmn.pszText = _T("Value");
	lvclmn.cx = 370;
	m_InmappDetails.SetColumn(1,&lvclmn);
	m_groupWzrdState.SetWindowText(_T("Application Metadata"));
	m_commentsEdit.Format(_T("Welcome to InMage Scout Application Readiness Wizard"));
	UpdateData(false);
	updateTree();
	m_Inmapptree.SelectItem(m_Inmapptree.GetRootItem());
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void CAppWizardDlg::updateTree()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	AppData& appdata = *AppData::GetInstance();
	m_Inmapptree.DeleteAllItems();
	vector<AppTreeFormat>& temp_vector = appdata.getAppTreeVector();
	std::string hostName = Host::GetInstance().GetHostName();
	m_InmsysInfo = m_Inmapptree.InsertItem(_T(hostName.c_str()),0,0);
	
	std::map<std::string,ExchAppVersionDiscInfo>::const_iterator exchIter = appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
	vector<AppTreeFormat>::const_iterator& appIter = temp_vector.begin();
    std::map<std::string, FileServerInstanceDiscoveryData>::iterator discoveryIter = appdata.m_fileservApp.m_fileShareInstanceDiscoveryMap.begin() ;
    std::map<std::string, FileServerInstanceMetaData>::iterator instanceIter = appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.begin() ;
	if(m_flagValidate)
	{
		if(ConfigValueObj::getInstance().isApplicationSelected(INM_APP_MSSQL))
		{
			m_InmMSSQL = m_Inmapptree.InsertItem(_T("MS SQL Server"),2,3,m_InmsysInfo);
			while(appIter != temp_vector.end())
			{
				const AppTreeFormat& tempATF = *appIter;
				HTREEITEM appHItem = m_Inmapptree.InsertItem(tempATF.m_appName,2,3,m_InmMSSQL);
				vector<CString>::const_iterator& instanceIter = tempATF.m_instancevector.begin();
				while(instanceIter != tempATF.m_instancevector.end())
				{
					const CString& tempInstanceName = *instanceIter;
					HTREEITEM instanceHItem = m_Inmapptree.InsertItem(tempInstanceName,2,2,appHItem);
					instanceIter++;
				}
				appIter++;
			}
		}
        if(ConfigValueObj::getInstance().isApplicationSelected(INM_APP_EXCHNAGE))
		{
			bool bFirst = true;
//			m_InmExchange = m_Inmapptree.InsertItem(appdata.appType(INM_APP_EXCHNAGE),2,3,m_InmsysInfo);
			while(exchIter != appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.end())
			{
				const ExchAppVersionDiscInfo &appVerInfo = exchIter->second;
				if( bFirst )
				{
					m_InmExchange = m_Inmapptree.InsertItem(appdata.appType(appVerInfo.m_appType),2,3,m_InmsysInfo);
					bFirst = false ;
				}
				if(!exchIter->first.empty())
					HTREEITEM appHIter = m_Inmapptree.InsertItem(exchIter->first.c_str(),2,3,m_InmExchange);
				if( appVerInfo.m_appType == INM_APP_EXCH2003 || appVerInfo.m_appType == INM_APP_EXCH2007 || appVerInfo.m_appType == INM_APP_EXCHNAGE )
			    {
						// nothing will be there in validation. except server Name.
			    }
			    else if( appVerInfo.m_appType == INM_APP_EXCH2010 )
			    {
					
			    }
			    else
				{
			   
				}
				exchIter++;
			}
		}
        if( appdata.m_fileservApp.m_fileShareInstanceDiscoveryMap.size() > 0 )
        {
		    m_InmFileServer = m_Inmapptree.InsertItem(appdata.appType(INM_APP_FILESERVER),2,3,m_InmsysInfo);
            if( appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.empty() == false )
            {
                while(discoveryIter != appdata.m_fileservApp.m_fileShareInstanceDiscoveryMap.end())
	            {
					if( appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.find(discoveryIter->first) != appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.end() )
					{
						if( appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.find(discoveryIter->first)->second.m_ErrorCode == 0 )
						{
							m_Inmapptree.InsertItem(discoveryIter->first.c_str(),m_InmFileServer);
						}
					}
		            discoveryIter++;
	            }
            }
        }
        if( ConfigValueObj::getInstance().isApplicationSelected(INM_APP_BES) )
        {
            m_InmBES = m_Inmapptree.InsertItem(appdata.appType(INM_APP_BES),2,3,m_InmsysInfo);    
        }
	}
	else 
	{
        if( ConfigValueObj::getInstance().isApplicationSelected(INM_APP_MSSQL) )
        {
            m_InmMSSQL = m_Inmapptree.InsertItem(_T("MS SQL Server"),2,3,m_InmsysInfo);
            while(appIter != temp_vector.end())
            {
                const AppTreeFormat& tempATF = *appIter;
                HTREEITEM appHItem = m_Inmapptree.InsertItem(tempATF.m_appName,2,3,m_InmMSSQL);
                vector<CString>::const_iterator& instanceIter = tempATF.m_instancevector.begin();
                while(instanceIter != tempATF.m_instancevector.end())
                {
	                const CString& tempInstanceName = *instanceIter;
	                HTREEITEM instanceHItem = m_Inmapptree.InsertItem(tempInstanceName,2,3,appHItem);
	                vector<CString> tempDBVector = tempATF.m_DBmap.find(tempInstanceName)->second;
	                vector<CString>::const_iterator tempDBIter = tempDBVector.begin();
	                while(tempDBIter != tempDBVector.end())
	                {
		                const CString& tempDBName = *tempDBIter;
		                m_Inmapptree.InsertItem(tempDBName,1,1,instanceHItem);
		                tempDBIter++;
	                }
	                instanceIter++;
                 }
                appIter++;
              }
         }
         if(ConfigValueObj::getInstance().isApplicationSelected(INM_APP_EXCHNAGE))
         {
			bool bFirst = true;
            if( appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.empty() )
            {
                m_InmExchange = m_Inmapptree.InsertItem(appdata.appType(INM_APP_EXCHNAGE),2,3,m_InmsysInfo);
            }
           while(exchIter != appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.end())
           {
               const ExchAppVersionDiscInfo &appVerInfo = exchIter->second;
			   if(bFirst)
			   {
					m_InmExchange = m_Inmapptree.InsertItem(appdata.appType(appVerInfo.m_appType),2,3,m_InmsysInfo);
					bFirst = false;
			   }
               HTREEITEM appHIter = m_Inmapptree.InsertItem(exchIter->first.c_str(),2,3,m_InmExchange);
			   if( appVerInfo.m_appType	== INM_APP_EXCH2003 || appVerInfo.m_appType	== INM_APP_EXCH2007 || appVerInfo.m_appType == INM_APP_EXCHNAGE )
			   {
                   ExchangeMetaData& tempMetadata = appdata.m_pExchangeApp.m_exchMetaDataMap[exchIter->first];
                   std::list<ExchangeStorageGroupMetaData>::const_iterator SGMetaDaraIter = tempMetadata.m_storageGrps.begin();
                   while(SGMetaDaraIter != tempMetadata.m_storageGrps.end())
                   {
                       const ExchangeStorageGroupMetaData &SGMetatData = *SGMetaDaraIter;
                       HTREEITEM SGHItem = m_Inmapptree.InsertItem(SGMetatData.m_storageGrpName.c_str(),2,3,appHIter);
                       std::list<ExchangeMDBMetaData>::const_iterator DBMetaDataIter = SGMetatData.m_mdbMetaDataList.begin();
                       while(DBMetaDataIter != SGMetatData.m_mdbMetaDataList.end())
                       {
                           const ExchangeMDBMetaData &DBMetaData = *DBMetaDataIter;
						   if(!DBMetaData.m_mdbName.empty())
							   m_Inmapptree.InsertItem(DBMetaData.m_mdbName.c_str(),1,1,SGHItem);
                           DBMetaDataIter++;
                       }
                       SGMetaDaraIter++;
                   }
			   }
			   else if( appVerInfo.m_appType == INM_APP_EXCH2010 )
			   {
				   Exchange2k10MetaData& tempMetadata = appdata.m_pExchangeApp.m_exch2k10MetaDataMap[exchIter->first];
				   std::list<ExchangeMDBMetaData>::const_iterator DBMetaDataIter = tempMetadata.m_mdbMetaDataList.begin();
				   while( DBMetaDataIter != tempMetadata.m_mdbMetaDataList.end() )
				   {
					   const ExchangeMDBMetaData &DBMetaData = *DBMetaDataIter;
					   if(!DBMetaData.m_mdbName.empty())
						   m_Inmapptree.InsertItem(DBMetaData.m_mdbName.c_str(),1,1,appHIter);
					   DBMetaDataIter++;					
				   }
			   }
			   else
			   {
					
			   }
               exchIter++;
            }
         }
 	     if(discoveryIter != appdata.m_fileservApp.m_fileShareInstanceDiscoveryMap.end())
		 {
            m_InmFileServer = m_Inmapptree.InsertItem(appdata.appType(INM_APP_FILESERVER),2,3,m_InmsysInfo);
            if( appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.empty() == false )
            {
                while(discoveryIter != appdata.m_fileservApp.m_fileShareInstanceDiscoveryMap.end())
	            {
					if( appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.find(discoveryIter->first) != appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.end() )
					{
						if( appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.find(discoveryIter->first)->second.m_ErrorCode == 0 )
						{
		            		m_Inmapptree.InsertItem(discoveryIter->first.c_str(),m_InmFileServer);
						}
					}
		            discoveryIter++;
	            }
            }
         }
         if( ConfigValueObj::getInstance().isApplicationSelected(INM_APP_BES) )
         {
             m_InmBES = m_Inmapptree.InsertItem(appdata.appType(INM_APP_BES),2,3,m_InmsysInfo);           
         }
 	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void CAppWizardDlg::OnTvnSelchangedTreeApp(NMHDR *pNMHDR, LRESULT *pResult)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	CString strSelItem = m_Inmapptree.GetItemText(pNMTreeView->itemNew.hItem);
	CString strSelItemParent = m_Inmapptree.GetItemText(m_Inmapptree.GetParentItem(pNMTreeView->itemNew.hItem));
	CString strSelGrandParent = m_Inmapptree.GetItemText(m_Inmapptree.GetParentItem(m_Inmapptree.GetParentItem(pNMTreeView->itemNew.hItem)));
	AppData& appdata = *AppData::GetInstance();
	BeginWaitCursor();
	m_InmappDetails.DeleteAllItems();
	if(strSelItemParent.IsEmpty())
	{
		if(m_flagValidate)
		{
			populateValidateInfo(POPULATE_SYS_DETAILS);
		}
		else
		{
            populateAppDetailsList(POPULATE_SYS_DETAILS);
		}
		m_commentsEdit.Format(_T("Root Item: ")+strSelItem+_T(" is selected"));
		UpdateData(false);
	}
	else if(strSelItem.Compare(appdata.appType(INM_APP_FILESERVER)) == 0)
	{
		m_commentsEdit.Format(strSelItem+_T(" is selected"));
		UpdateData(false);
		if(m_flagValidate)
		{
			populateValidateInfo(POPULATE_APPLICATION_DETAILS,strSelItem);
		}
		else
		{
			populateAppDetailsList(POPULATE_FILESERVER_DETAILS);
		}
		
	}
	else if(appdata.isServNTName(strSelItem))
	{
		if(m_flagValidate)
		{
			populateValidateInfo(POPULATE_SERVNTNAME_DETAILS, strSelItem);
		}
		else
		{
			populateAppDetailsList(POPULATE_SERVNTNAME_DETAILS,strSelItem);
		}
		m_commentsEdit.Format(strSelItem+_T(" is selected"));
		UpdateData(false);
	}
	else if(appdata.isVersion(strSelItem))
	{
		//version item has selected
		if(m_flagValidate)
		{
			populateValidateInfo(POPULATE_VERSION_DETAILS,strSelItem);
		}
		else
		{
            populateAppDetailsList(POPULATE_VERSION_DETAILS,strSelItem);
		}
		m_commentsEdit.Format(strSelItem+_T(" is selected"));
		UpdateData(false);
	}
	else if(appdata.isInstance(strSelItem))
	{
        if(m_flagValidate)
		{
			populateValidateInfo(POPULATE_INSTANCE_DETAILS,strSelItem);
		}
		else
		{
            populateAppDetailsList(POPULATE_INSTANCE_DETAILS,strSelItem);
		}
		m_commentsEdit.Format(strSelItem+_T(" is selected"));
		UpdateData(false);
	}
	else if(appdata.isDatabase(strSelItem,strSelItemParent))
	{
		//DataBase Item has selected
		populateAppDetailsList(POPULATE_DB_DETAILS,strSelItem,strSelItemParent);
		m_commentsEdit.Format(_T("DataBase: ")+strSelItem+_T(" of the Instance ")+strSelItemParent+_T(" is selected"));
		UpdateData(false);
	}
	else if(strSelItem.Compare(appdata.appType(INM_APP_MSSQL)) == 0)//MS SQL Application selected
	{
		if(m_flagValidate)
		{
			populateValidateInfo(POPULATE_APPLICATION_DETAILS, strSelItem);
		}
		else
		{
            populateAppDetailsList(POPULATE_APPLICATION_DETAILS,strSelItem);
		}
		m_commentsEdit.Format(strSelItem+_T(" is selected"));
		UpdateData(false);
	}
	else if(strSelItem.Compare(appdata.appType(INM_APP_EXCHNAGE)) == 0)//Exchange application selected
	{
		if(m_flagValidate)
			populateValidateInfo(POPULATE_APPLICATION_DETAILS,strSelItem);
		else
			populateAppDetailsList(POPULATE_APPLICATION_DETAILS,strSelItem);

		m_commentsEdit.Format(strSelItem+_T(" is selected"));
		UpdateData(false);
	}
	else if(appdata.isStorageGroup(strSelItem,strSelItemParent))
	{
		if(m_flagValidate)
			populateValidateInfo(POPULATE_STORAGEGROUP_DETAILS,strSelItem,strSelItemParent);
		else
			populateAppDetailsList(POPULATE_STORAGEGROUP_DETAILS,strSelItem,strSelItemParent);

		m_commentsEdit.Format(strSelItem+_T(" is selected"));
		UpdateData(false);
	}
	else if(appdata.isMailBox(strSelItem,strSelItemParent,strSelGrandParent))
	{
		if(m_flagValidate)
			populateValidateInfo(POPULATE_MAILBOX_DETAILS,strSelItem,strSelItemParent,strSelGrandParent);
		else
			populateAppDetailsList(POPULATE_MAILBOX_DETAILS,strSelItem,strSelItemParent,strSelGrandParent);

		m_commentsEdit.Format(strSelItem+_T(" is selected"));
		UpdateData(false);
	}
    else if(strSelItem.Compare(appdata.appType(INM_APP_BES)) == 0)
	{
		m_commentsEdit.Format(strSelItem+_T(" is selected"));
		UpdateData(false);
		if(m_flagValidate)
		{
			populateValidateInfo(POPULATE_APPLICATION_DETAILS,strSelItem);
		}
		else
		{
			populateAppDetailsList(POPULATE_BES_DETAILS);
		}
		
	}
	*pResult = 0;
	EndWaitCursor();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void CAppWizardDlg::populateAppDetailsList(UINT choice,CString strSelItem,CString strSelItemParent,CString strSelItemGrandParent)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	AppData& appdata = *AppData::GetInstance();
	m_InmappDetails.DeleteAllItems();
	int nRowCount = 0;
	bool flagExchange = false;
	std::list<MSSQLVersionDiscoveryInfo>::iterator sqlVersionDiscoveryInfoIter;
	std::map<std::string,ExchAppVersionDiscInfo>::iterator exchangeVersionDiscoveryInfoIter;
	std::list<MSSQLInstanceDiscoveryInfo>::iterator instanceDiscoveryInfoIter;
	MSSQLInstanceMetaData instanceMetaData;
	CString tempStr;
	std::list<std::pair<std::string,std::string> > pairList; 
	std::list<std::pair<std::string,std::string> >::iterator pairIter;
	std::list<VssProviderProperties>::iterator vssIter;
    std::list<VSSWriterProperties>::iterator vssWriterIter ;
	std::list<ExchangeStorageGroupMetaData>::iterator storageGrpsIter;
    std::map<std::string, FileServerInstanceDiscoveryData>::iterator fileServerDiscIter ;
    std::map<std::string, FileServerInstanceMetaData>::iterator fileServerMetaDataIter ;
	USES_CONVERSION;
    if(ConfigValueObj::getInstance().getApplications().size() == 0 )
    {
        return ;
    }
	switch(choice)
	{
	case POPULATE_FILESERVER_DETAILS:
		{
			list<InmProcess> tempProcList;
			nRowCount = populateServiceNprocessInfo(nRowCount,appdata.m_fileservApp.m_services,tempProcList);
            if( appdata.m_fileservApp.m_services.size() > 0 )
            {
			    std::list<InmService>::iterator inmServiceIter = appdata.m_fileservApp.m_services.begin();
			    if( inmServiceIter->m_svcStatus != INM_SVCSTAT_RUNNING )
			    {
				    m_commentsEdit.Format(_T("WARNING: Server(lanmanserver) service is not running.\n\t  Failed to Find Out the Shared Resources Info."));
				    UpdateData(false);
				    break;
			    }
            }
		}
		break;
	case POPULATE_SERVNTNAME_DETAILS:
		{
			fileServerDiscIter = appdata.m_fileservApp.m_fileShareInstanceDiscoveryMap.find( ((LPCTSTR)strSelItem) ); 
            fileServerMetaDataIter = appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.end() ;
			nRowCount = 0;

			if(fileServerDiscIter != appdata.m_fileservApp.m_fileShareInstanceDiscoveryMap.end() && 
               appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.empty() == false )
			{
					m_InmappDetails.InsertItem( nRowCount, _T( "NETWORK NAME:" ) ) ; 
					m_InmappDetails.SetItemText(nRowCount,1,fileServerDiscIter->first.c_str());
					nRowCount++;

					std::list<std::string>::iterator ipsetIter = fileServerDiscIter->second.m_ips.begin();
					std::string ip;
					while( ipsetIter != fileServerDiscIter->second.m_ips.end() )
					{
						m_InmappDetails.InsertItem( nRowCount, _T( "IP ADDRESS:" ) ) ; 
						ip = *ipsetIter;
						m_InmappDetails.SetItemText(nRowCount,1, _T (ip.c_str()) );
						nRowCount++;
						ipsetIter++;
					}	
					fileServerMetaDataIter = appdata.m_fileservApp.m_fileShareInstanceMetaDataMap.find(fileServerDiscIter->first) ;
                    //std::list<std::string>::iterator volumeMapIter = appdata.m_fileservApp.m_netwoknameVolumeMap.find( iterFileServm_sqlApp.first.c_str() );
                    std::list<std::string>::iterator setIter = fileServerMetaDataIter->second.m_volumes.begin();
				    std::stringstream volumeStream;
				    std::string volumePath;
				    while( setIter != fileServerMetaDataIter->second.m_volumes.end() )
				    {
					    volumePath.clear();
					    volumePath = *setIter;
					    volumeStream << volumePath << "   ";
					    setIter++;
				    }
				    m_InmappDetails.InsertItem( nRowCount, _T("DISCOVERED VOLUMES: ") );
				    m_InmappDetails.SetItemText(nRowCount,1, _T (volumeStream.str().c_str()) );
				    nRowCount++;
                    
                    m_InmappDetails.InsertItem(nRowCount,_T(" "));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T(" SHARED RESOURCE INFO"));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T("--------------------------------"));
					nRowCount++;
                    std::map<std::string, FileShareInfo>::iterator shareInfoIter = fileServerMetaDataIter->second.m_shareInfo.begin() ;
					while(shareInfoIter != fileServerMetaDataIter->second.m_shareInfo.end())
					{
                        std::list<std::pair<std::string, std::string> > properties = shareInfoIter->second.getProperties();
                        std::list<std::pair<std::string,std::string> >::iterator iterMapProperty = properties.begin();
						while(iterMapProperty != properties.end())
						{
							m_InmappDetails.InsertItem(nRowCount,iterMapProperty->first.c_str());
							m_InmappDetails.SetItemText(nRowCount,1,iterMapProperty->second.c_str());
							nRowCount++;
							iterMapProperty++;
						}
						m_InmappDetails.InsertItem(nRowCount,_T(" "));
						nRowCount++;
						shareInfoIter++;
					}
			}
		}
		break;
	case POPULATE_STORAGEGROUP_DETAILS:
		{
			nRowCount = 0;
			exchangeVersionDiscoveryInfoIter = appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
			while(exchangeVersionDiscoveryInfoIter != appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.end())
			{
				if(strSelItemParent.Compare(exchangeVersionDiscoveryInfoIter->first.c_str()) == 0)
				{
					ExchangeMetaData &tempMetadata = appdata.m_pExchangeApp.m_exchMetaDataMap[exchangeVersionDiscoveryInfoIter->first];
					storageGrpsIter = tempMetadata.m_storageGrps.begin();
					while(storageGrpsIter != tempMetadata.m_storageGrps.end())
					{
						ExchangeStorageGroupMetaData &sgMetadata = *storageGrpsIter;
						if(strSelItem.Compare(sgMetadata.m_storageGrpName.c_str()) == 0)
						{
							std::list<std::pair<std::string, std::string> > propertyList = sgMetadata.getProperties();
							std::list<std::pair<std::string, std::string> >::iterator propListIter = propertyList.begin();
							while(propListIter != propertyList.end())
							{
								m_InmappDetails.InsertItem(nRowCount,propListIter->first.c_str());
								m_InmappDetails.SetItemText(nRowCount,1,propListIter->second.c_str());
								nRowCount++;
								propListIter++;                   
							}
                            std::list<std::string> volumesList = sgMetadata.m_volumeList ;
					        std::list<std::string>::const_iterator VolumesIter = volumesList.begin();
					        m_InmappDetails.InsertItem(nRowCount,_T("Volumes"));
					        while(VolumesIter != volumesList.end())
					        {

						        m_InmappDetails.SetItemText(nRowCount,1,CString(VolumesIter->c_str()));
						        nRowCount++;
						        m_InmappDetails.InsertItem(nRowCount,_T(" "));
						        VolumesIter++;
					        }
							break;// terminates loop
						}
						storageGrpsIter++;
					} 
				}
				exchangeVersionDiscoveryInfoIter++;
			}
			
		}
		break;
	case POPULATE_MAILBOX_DETAILS:
		{
			nRowCount = 0;
			exchangeVersionDiscoveryInfoIter = appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
			while(exchangeVersionDiscoveryInfoIter != appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.end())
			{
				const ExchAppVersionDiscInfo &appVerInfo = exchangeVersionDiscoveryInfoIter->second;
				DebugPrintf(SV_LOG_DEBUG, "The Value: %s\n", exchangeVersionDiscoveryInfoIter->first.c_str()) ;
				DebugPrintf(SV_LOG_DEBUG, "The Value: %s\n", strSelItemGrandParent) ;
				if(appVerInfo.m_appType == INM_APP_EXCH2003 || appVerInfo.m_appType == INM_APP_EXCH2007 || appVerInfo.m_appType == INM_APP_EXCHNAGE)
				{
				if(strSelItemGrandParent.Compare(exchangeVersionDiscoveryInfoIter->first.c_str()) == 0)
				{
					ExchangeMetaData &tempMetadata = appdata.m_pExchangeApp.m_exchMetaDataMap[exchangeVersionDiscoveryInfoIter->first];
					storageGrpsIter = tempMetadata.m_storageGrps.begin();
					while(storageGrpsIter != tempMetadata.m_storageGrps.end())
					{
						ExchangeStorageGroupMetaData &sgMetadata = *storageGrpsIter;
						if(strSelItemParent.Compare(sgMetadata.m_storageGrpName.c_str()) == 0)
						{
							std::list<ExchangeMDBMetaData>::iterator exchangeMDBIter = sgMetadata.m_mdbMetaDataList.begin();
							while(exchangeMDBIter != sgMetadata.m_mdbMetaDataList.end())
							{
								ExchangeMDBMetaData &exchangeMDBMetadata = *exchangeMDBIter;
								if(strSelItem.Compare(exchangeMDBMetadata.m_mdbName.c_str()) == 0)
								{
									std::list<std::pair<std::string, std::string> > propertyList = exchangeMDBMetadata.getProperties();
									std::list<std::pair<std::string, std::string> >::iterator propListIter = propertyList.begin();
									while(propListIter != propertyList.end())
									{
										m_InmappDetails.InsertItem(nRowCount,propListIter->first.c_str());
										m_InmappDetails.SetItemText(nRowCount,1,propListIter->second.c_str());
										nRowCount++;
										propListIter++;
									}
									break;
								}
								exchangeMDBIter++;
							}
						}
						storageGrpsIter++;
					} 
				    }
				}
				else if( appVerInfo.m_appType == INM_APP_EXCH2010 )
				{
					if(strSelItemGrandParent.Compare(appdata.appType(INM_APP_EXCH2010)) == 0)
					{
						Exchange2k10MetaData &tempMetadata = appdata.m_pExchangeApp.m_exch2k10MetaDataMap[exchangeVersionDiscoveryInfoIter->first];
						std::list<ExchangeMDBMetaData>::iterator exchangeMDBIter = tempMetadata.m_mdbMetaDataList.begin();
						while(exchangeMDBIter != tempMetadata.m_mdbMetaDataList.end())
						{
							ExchangeMDBMetaData &exchangeMDBMetadata = *exchangeMDBIter;
							if(strSelItem.Compare(exchangeMDBMetadata.m_mdbName.c_str()) == 0)
							{
								std::list<std::pair<std::string, std::string> > propertyList = exchangeMDBMetadata.getProperties();
								std::list<std::pair<std::string, std::string> >::iterator propListIter = propertyList.begin();
								while(propListIter != propertyList.end())
								{
									m_InmappDetails.InsertItem(nRowCount,propListIter->first.c_str());
									m_InmappDetails.SetItemText(nRowCount,1,propListIter->second.c_str());
									nRowCount++;
									propListIter++;
								}
								break;
							}
							exchangeMDBIter++;
						}
					}
				}
				else
				{
				}
				exchangeVersionDiscoveryInfoIter++;
			}
		}
		break;
	case POPULATE_APPLICATION_DETAILS :
		nRowCount = 0;
		if(strSelItem.Compare(appdata.appType(INM_APP_MSSQL)) == 0)//MS SQL discovery information
		{
			populateServiceNprocessInfo(nRowCount,appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_services,appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_processes);
		}
		else if(strSelItem.Compare(appdata.appType(INM_APP_EXCHNAGE)) == 0)//EXCHANGE discovery information
		{
			break;
		}
        else if(strSelItem.Compare(appdata.appType(INM_APP_BES)) == 0)
        {
            break;
        }
        else 
			break;
		break;
	case POPULATE_INSTANCE_DETAILS :
		sqlVersionDiscoveryInfoIter = appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin() ;
		while( sqlVersionDiscoveryInfoIter != appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end() )
		{
		    instanceDiscoveryInfoIter = sqlVersionDiscoveryInfoIter->m_sqlInstanceDiscoveryInfo.begin() ;
		    while( instanceDiscoveryInfoIter != sqlVersionDiscoveryInfoIter->m_sqlInstanceDiscoveryInfo.end() )
		    {
				nRowCount = 0;
		        MSSQLInstanceDiscoveryInfo& discInfo = *instanceDiscoveryInfoIter;
				if(strSelItem.Compare(CString(discInfo.m_instanceName.c_str())) == 0)
				{
                    /*
					m_InmappDetails.InsertItem(nRowCount,_T("Application Type"));
					m_InmappDetails.SetItemText(nRowCount,1,appdata.appType(discInfo.m_appType));
					nRowCount++;
                    */
					m_InmappDetails.InsertItem(nRowCount,_T("Version"));
					m_InmappDetails.SetItemText(nRowCount,1,CString(discInfo.m_version.c_str()));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T("Edition"));
					m_InmappDetails.SetItemText(nRowCount,1,CString(discInfo.m_edition.c_str()));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T("Instance name"));
					m_InmappDetails.SetItemText(nRowCount,1,CString(discInfo.m_instanceName.c_str()));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T("Clusterd"));
					if(discInfo.m_isClustered == 0)
				        	m_InmappDetails.SetItemText(nRowCount,1,_T("False"));
					else
							m_InmappDetails.SetItemText(nRowCount,1,_T("True"));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T("Virtual Server Name"));
					m_InmappDetails.SetItemText(nRowCount,1,CString(discInfo.m_virtualSrvrName.c_str()));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T("Installation Path"));
					m_InmappDetails.SetItemText(nRowCount,1,CString(discInfo.m_installPath.c_str()));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T("Data Directory Path"));
					m_InmappDetails.SetItemText(nRowCount,1,CString(discInfo.m_dataDir.c_str()));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T("Dump Directory Path"));
					m_InmappDetails.SetItemText(nRowCount,1,CString(discInfo.m_dumpDir.c_str()));
					nRowCount++;
					m_InmappDetails.InsertItem(nRowCount,_T("Error Log Path"));
					m_InmappDetails.SetItemText(nRowCount,1,CString(discInfo.m_errorLogPath.c_str()));
					nRowCount++;

					populateServiceNprocessInfo(nRowCount,discInfo.m_services,discInfo.m_processes);
					return;
				}
				instanceDiscoveryInfoIter++;
			}
			sqlVersionDiscoveryInfoIter++;
		}
		break;
	case POPULATE_VERSION_DETAILS :
		nRowCount = 0;
		exchangeVersionDiscoveryInfoIter = appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
		while(exchangeVersionDiscoveryInfoIter != appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.end())
		{
			ExchAppVersionDiscInfo &exchVersionInfo = exchangeVersionDiscoveryInfoIter->second;
			if(strSelItem.Compare(exchangeVersionDiscoveryInfoIter->first.c_str()) == 0)
			{
				std::list<std::pair<std::string, std::string> > propertyList = exchVersionInfo.getPropertyList();
				std::list<std::pair<std::string, std::string> >::iterator propListIter = propertyList.begin();
				while(propListIter != propertyList.end())
				{
					m_InmappDetails.InsertItem(nRowCount,propListIter->first.c_str());
					m_InmappDetails.SetItemText(nRowCount,1,propListIter->second.c_str());
					nRowCount++;
					propListIter++;
				}
				populateServiceNprocessInfo(nRowCount,exchVersionInfo.m_services,exchVersionInfo.m_processes);
				return;
			}
			exchangeVersionDiscoveryInfoIter++;
		}
		sqlVersionDiscoveryInfoIter = appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin() ;
		while( sqlVersionDiscoveryInfoIter != appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end() )
		{
			MSSQLVersionDiscoveryInfo& tempSQLInstanceInfo = *sqlVersionDiscoveryInfoIter;
			if(appdata.appType(tempSQLInstanceInfo.m_type).Compare(strSelItem) == 0)
			{
				populateServiceNprocessInfo(nRowCount,tempSQLInstanceInfo.m_services,tempSQLInstanceInfo.m_processes);
				return;
			}
			sqlVersionDiscoveryInfoIter++;
		}
		break;
	case POPULATE_SYS_DETAILS :
	{
		nRowCount = 0;
		m_InmappDetails.InsertItem(nRowCount,_T("Host Details"),1);
		nRowCount++;
		pairList = appdata.m_hldiscInfo.m_sysInfo.m_hostInfo.getPropertyList();
		pairIter = pairList.begin();
		while(pairIter != pairList.end())
		{
			m_InmappDetails.InsertItem(nRowCount,_T("        ")+CString(pairIter->first.c_str()));
			m_InmappDetails.SetItemText(nRowCount,1,CString(pairIter->second.c_str()));
			nRowCount++;
			pairIter++;
		}
		m_InmappDetails.InsertItem(nRowCount,_T(" "));
		nRowCount++;

		//Network Info
		
		m_InmappDetails.InsertItem(nRowCount,_T("System Network Details"));
		nRowCount++;
		m_InmappDetails.InsertItem(nRowCount,_T(" "));
		nRowCount++;
		std::list<NetworkAdapterConfig>::iterator nwAdapterListIter = appdata.m_hldiscInfo.m_sysInfo.m_nwAdapterList.begin();
		while( nwAdapterListIter != appdata.m_hldiscInfo.m_sysInfo.m_nwAdapterList.end() )
		{
			pairList = nwAdapterListIter->getPropertyList();
			pairIter = pairList.begin();
			while(pairIter != pairList.end())
			{
				m_InmappDetails.InsertItem(nRowCount,_T("        ")+CString(pairIter->first.c_str()));
				m_InmappDetails.SetItemText(nRowCount,1,CString(pairIter->second.c_str()));
				nRowCount++;
				pairIter++;
			}
		m_InmappDetails.InsertItem(nRowCount,_T(" "));
		nRowCount++;
			nwAdapterListIter++;
		}
		m_InmappDetails.InsertItem(nRowCount,_T(" "));
		nRowCount++;
		//end of network info 

		m_InmappDetails.InsertItem(nRowCount,_T("OS Details"));
		nRowCount++;
		pairList = appdata.m_hldiscInfo.m_sysInfo.m_OperatingSystemInfo.getPropertyList();
		pairIter = pairList.begin();
		while(pairIter != pairList.end())
		{
			m_InmappDetails.InsertItem(nRowCount,_T("        ")+CString(pairIter->first.c_str()));
			m_InmappDetails.SetItemText(nRowCount,1,CString(pairIter->second.c_str()));
			nRowCount++;
			pairIter++;
		}

		m_InmappDetails.InsertItem(nRowCount,_T(" "));
		nRowCount++;

		m_InmappDetails.InsertItem(nRowCount,_T("Processor Details"));
		nRowCount++;
		pairList = appdata.m_hldiscInfo.m_sysInfo.m_processorInfo.getPropertyList();
		pairIter = pairList.begin();
		while(pairIter != pairList.end())
		{
			m_InmappDetails.InsertItem(nRowCount,_T("        ")+CString(pairIter->first.c_str()));
			m_InmappDetails.SetItemText(nRowCount,1,CString(pairIter->second.c_str()));
			nRowCount++;
			pairIter++;
		}

		if( appdata.m_hldiscInfo.isThisClusterNode )
		{
			m_InmappDetails.InsertItem(nRowCount,_T(" "));
			nRowCount++;
			m_InmappDetails.InsertItem(nRowCount,_T("Cluster Details"));
			nRowCount++;
			m_InmappDetails.InsertItem(nRowCount,_T("        ")+CString("Cluster Name"));
			m_InmappDetails.SetItemText(nRowCount,1,CString(appdata.m_hldiscInfo.m_clusterInfo.m_clusterName.c_str()));
			nRowCount++;
			m_InmappDetails.InsertItem(nRowCount,_T("        ")+CString("Cluster IP"));
			m_InmappDetails.SetItemText(nRowCount,1,CString(appdata.m_hldiscInfo.m_clusterInfo.m_clusterIP.c_str()));
			nRowCount++;
			m_InmappDetails.InsertItem(nRowCount,_T("        ")+CString("Cluster ID"));
			m_InmappDetails.SetItemText(nRowCount,1,CString(appdata.m_hldiscInfo.m_clusterInfo.m_clusterID.c_str()));
			nRowCount++;
		}
		
		m_InmappDetails.InsertItem(nRowCount,_T(" "));
		nRowCount++;

		m_InmappDetails.InsertItem(nRowCount,_T("VSS Providers Details"));
		nRowCount++;
		vssIter = appdata.m_hldiscInfo.m_vssProviderList.begin();
		while(vssIter != appdata.m_hldiscInfo.m_vssProviderList.end())
		{
			pairList = vssIter->getPropList();
			pairIter = pairList.begin();
			while(pairIter != pairList.end())
			{
				m_InmappDetails.InsertItem(nRowCount,_T("        ")+CString(pairIter->first.c_str()));
				m_InmappDetails.SetItemText(nRowCount,1,CString(pairIter->second.c_str()));
				nRowCount++;
				pairIter++;
			}
			m_InmappDetails.InsertItem(nRowCount,_T(" "));
			nRowCount++;
			vssIter++;
		}

    m_InmappDetails.InsertItem(nRowCount,_T("VSS Writer Instance Details"));
		nRowCount++;
		populateServiceNprocessInfo(nRowCount,appdata.m_hldiscInfo.m_svcList,appdata.m_hldiscInfo.m_procList);
	}	
		break;
	case POPULATE_DB_DETAILS :
    {
		nRowCount = 0;
		instanceMetaData = appdata.m_sqlApp.m_sqlmetaData.m_serverInstanceMap[(LPCTSTR)strSelItemParent] ;
        std::map<std::string, MSSQLDBMetaData>::const_iterator dbMapIter = instanceMetaData.m_dbsMap.begin() ;
            while( dbMapIter != instanceMetaData.m_dbsMap.end() )
            {
				if(CString(dbMapIter->first.c_str()).Compare(strSelItem) == 0)
				{
                    std::list<std::string> dbFilesList = dbMapIter->second.m_dbFiles;
					std::list<std::string>::const_iterator dbFilesIter = dbFilesList.begin();
					m_InmappDetails.InsertItem(nRowCount,_T("Database Files"));
					while(dbFilesIter != dbFilesList.end())
					{
						m_InmappDetails.SetItemText(nRowCount,1,CString(dbFilesIter->c_str()));
						nRowCount++;
						m_InmappDetails.InsertItem(nRowCount,_T(" "));
						dbFilesIter++;
					}
                    m_InmappDetails.InsertItem(nRowCount,_T("DataBase Status"));
					if( dbMapIter->second.m_dbOnline )
					{
						m_InmappDetails.SetItemText(nRowCount,1,CString("ON LINE"));
						nRowCount++;
					}
					else
					{
						m_InmappDetails.SetItemText(nRowCount,1,CString("OFF LINE"));
						nRowCount++;
					}
					m_InmappDetails.InsertItem(nRowCount,_T("DataBase Type"));
					std::string dbType;
					dbType = appdata.m_sqlApp.getSQLDataBaseNameFromType( dbMapIter->second.m_dbType ); 
					m_InmappDetails.SetItemText(nRowCount,1,CString(dbType.c_str()));
					nRowCount++;
					break;
				}
				dbMapIter++;
			}
			dbMapIter = instanceMetaData.m_dbsMap.begin() ;
			while( dbMapIter != instanceMetaData.m_dbsMap.end())
			{
				if(CString(dbMapIter->first.c_str()).Compare(strSelItem) == 0)
				{
                    std::list<std::string> dbVolumesList = dbMapIter->second.m_volumes;
					std::list<std::string>::const_iterator dbVolumesIter = dbVolumesList.begin();
					m_InmappDetails.InsertItem(nRowCount,_T("Volumes"));
					while(dbVolumesIter != dbVolumesList.end())
					{

						m_InmappDetails.SetItemText(nRowCount,1,CString(dbVolumesIter->c_str()));
						nRowCount++;
						m_InmappDetails.InsertItem(nRowCount,_T(" "));
						dbVolumesIter++;
					}
					break;
				}
				dbMapIter++;
			}
			nRowCount++;
		break;
        }
        case POPULATE_BES_DETAILS:
           nRowCount = 0;
           m_InmappDetails.InsertItem( nRowCount, _T( "SETUP DETAILS" ) ) ; 
           nRowCount++;
           m_InmappDetails.InsertItem(nRowCount,_T("------------------"));
           nRowCount++;
           std::list<std::pair<std::string, std::string> > properties = appdata.m_besApp.m_discoveredData.getProperties();
           std::list<std::pair<std::string,std::string> >::iterator iterMapProperty = properties.begin();
		   while(iterMapProperty != properties.end())
		   {
				m_InmappDetails.InsertItem(nRowCount,iterMapProperty->first.c_str());
				m_InmappDetails.SetItemText(nRowCount,1,iterMapProperty->second.c_str());
				nRowCount++;
				iterMapProperty++;
		   }
	       m_InmappDetails.InsertItem(nRowCount,_T(" "));
           nRowCount++;
           m_InmappDetails.InsertItem( nRowCount, _T( "CONFIG DETAILS" ) ) ;
           nRowCount++;
           m_InmappDetails.InsertItem(nRowCount,_T("-------------------"));
           nRowCount++;
           properties = appdata.m_besApp.m_discoveredMetaData.getProperties();
           iterMapProperty = properties.begin();
		   while(iterMapProperty != properties.end())
		   {
				m_InmappDetails.InsertItem(nRowCount,iterMapProperty->first.c_str());
				m_InmappDetails.SetItemText(nRowCount,1,iterMapProperty->second.c_str());
				nRowCount++;
				iterMapProperty++;
		   }
	       m_InmappDetails.InsertItem(nRowCount,_T(" "));
           nRowCount++;
           list<InmProcess> tempProcList;
           populateServiceNprocessInfo(nRowCount, appdata.m_besApp.m_discoveredData.m_services, tempProcList);
           break;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void CAppWizardDlg::OnBnClickedButtonValidate()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	AppData& appdata = *AppData::GetInstance();
	CString btnText;
	m_InmbtnValidate.GetWindowText(btnText);
	if(btnText.Compare(_T("Next ")) == 0)
	{
        for(HTREEITEM hItem = m_Inmapptree.GetNextItem(m_Inmapptree.GetRootItem(),TVGN_CHILD) ; hItem != NULL ; hItem = m_Inmapptree.GetNextItem(hItem,TVGN_NEXT))
        {
            if( m_Inmapptree.GetCheck(hItem) )
            {
                std::string selectedApp = std::string(m_Inmapptree.GetItemText(hItem)) ;
                if(selectedApp.compare("Windows File Server") == 0 )
                {
                    ConfigValueObj::getInstance().setApplication(INM_APP_FILESERVER) ;
                }
                if(selectedApp.compare("MS Exchange Server") == 0 )
                {
                    ConfigValueObj::getInstance().setApplication(INM_APP_EXCHNAGE) ;
                }
                if(selectedApp.compare("MS SQL Server") == 0 )
                {
                    ConfigValueObj::getInstance().setApplication(INM_APP_MSSQL) ;
                }
                if(selectedApp.compare("BlackBerry Server") == 0 )
                {
                    ConfigValueObj::getInstance().setApplication(INM_APP_BES) ;
                }
            }
        }
		BeginWaitCursor();
		AfxBeginThread(discAppInfoThreadFunc,NULL);
		if(m_dlgProcessing.DoModal() == IDCANCEL)
		{
			exit(0);
		}
		
		displayDiscInfo();
		EndWaitCursor();
		std::string discErrorMsg;
		DiscoveryError discError = appdata.isDiscoverySuccess(discErrorMsg);
		if(discError != DISCOVERY_SUCCESSFUL)
		{
			CString ErrorMsg = "Failed to discover some applications data. The informaion shown is partial.\n";
			ErrorMsg.Append("Error: ");
			ErrorMsg.Append(discErrorMsg.c_str());
			AfxMessageBox(ErrorMsg,MB_ICONINFORMATION);
		}
	}
	else if(btnText.Compare(_T("Next")) == 0)
	{
		BeginWaitCursor();

		m_flagValidate = true;
		AfxBeginThread(ValidateAppThreadFunc,NULL);
		if(m_dlgProcessing.DoModal() == IDCANCEL)
		{
            appdata.ValidateRules();
		}
		m_InmappDetails.DeleteAllItems();
		LVCOLUMN lvclmn;
		lvclmn.mask = LVCF_TEXT|LVCF_WIDTH;
		lvclmn.pszText = _T("Rule Name");
		lvclmn.cx = 200;
		m_InmappDetails.SetColumn(0,&lvclmn);
		lvclmn.pszText = _T("Status");
		lvclmn.cx = 335;
		m_InmappDetails.SetColumn(1,&lvclmn);
		m_InmbtnValidate.SetWindowText(_T("Summary"));
		updateTree();
		m_Inmapptree.SelectItem(m_Inmapptree.GetRootItem());
		m_btnBack.EnableWindow();
		m_groupWzrdState.SetWindowText(_T("Validation Results"));

		EndWaitCursor();
	}
	else if(btnText.Compare(_T("Summary")) == 0)
	{
		m_flagValidate = false;
		m_InmappDetails.ShowWindow(SW_HIDE);
		m_Inmapptree.ShowWindow(SW_HIDE);
		GetDlgItem(IDC_RICHEDIT2_COMMENTS)->ShowWindow(SW_HIDE);
		m_groupWzrdState.SetWindowText(_T("Validation Summary"));

		displaySummary();

		GetDlgItem(IDC_RICHEDIT2_SUMMARY)->ShowWindow(SW_SHOW);
		m_InmbtnValidate.SetWindowText(_T("Report To CX"));
		appdata.CreateSummaryTextFile();
	}
	else if(btnText.Compare(_T("Report To CX")) == 0)
	{
		if(appdata.prepareDiscoveryUpdate())
		{
			AfxBeginThread(UpdateToCXThreadFunc,NULL);
			if(m_dlgProcessing.DoModal() == IDCANCEL)
			{
				UpdateToCX();
			}
			
			if( ( appdata.m_error == RWIZARD_CX_UPATE_DELETED )||
				( appdata.m_error == RWIZARD_CX_UPATE_DUPLICATE )||
				( appdata.m_error == RWIZARD_CX_UPATE_SUCCESS ) )
			{
				m_InmbtnValidate.EnableWindow(FALSE);
				AfxMessageBox(appdata.m_ErrorMsg.c_str(),MB_ICONINFORMATION | MB_OK);
				appdata.m_error = RWIZARD_CX_UPATE_SUCCESS;
				appdata.m_ErrorMsg = "";
			}
			else
			{
				CString errMsg = "Failed to update the discovery information to CX. Please try again.";
				if( (appdata.m_error == RWIZARD_CX_UPATE_TRASPORT_EXCEPTION) ||
					(appdata.m_error == RWIZARD_CX_UPATE_TRASPORT_NO_RESULT) )
				{
					errMsg += " CX is not reachable.";
				}
				else if (appdata.m_error == RWIZARD_CX_UPATE_UNMARSHAL_EXCEPTION)
				{
					errMsg += " Got exception in umarshaling update result.";
				}
				else if ( appdata.m_error == RWIZARD_CX_UPATE_UNKNOWN_EXCEPTION )
				{
					errMsg += " Got unknown exception.";
				}
				if(!appdata.m_ErrorMsg.empty())
				{
					errMsg += "\nException : ";
					errMsg += appdata.m_ErrorMsg.c_str();
				}
				AfxMessageBox(errMsg,MB_ICONINFORMATION | MB_OK);
				appdata.m_error = RWIZARD_CX_UPATE_SUCCESS;
				appdata.m_ErrorMsg = "";
				appdata.m_retryCount++;
				if(appdata.m_retryCount >= 3)
				{
					appdata.m_retryCount = 0;
					m_InmbtnValidate.EnableWindow(FALSE);
				}
			}
		}
		else
		{
			AfxMessageBox("Inavlied disocvery information found. Please try again.",MB_OK);
		}
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void CAppWizardDlg::populateValidateInfo(UINT choice,CString strSelItem,CString strSelItemParent,CString strSelItemGrandParent)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	std::list<ValidatorPtr>::iterator ruleIter ;
    std::list<ValidatorPtr> rules ;
	MSSQLInstanceMetaData metaData ;
	AppData& appdata = *AppData::GetInstance();
	//InmProtectedAppType type ;
	m_InmappDetails.DeleteAllItems();
	std::list<InmService>::iterator iterServ;
	int nRowCount = 0;
	CString tempStr;
	USES_CONVERSION;
    		
	switch(choice)
	{
	case POPULATE_SYS_DETAILS :
		nRowCount = 0;
		rules = appdata.m_sysLevelRules;
		ruleIter = rules.begin() ;
		while(ruleIter != rules.end() )
		{
            m_InmappDetails.InsertItem(nRowCount,CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str()));
			m_InmappDetails.SetItemText(nRowCount,1,CString((*ruleIter)->statusToString().c_str()));
			nRowCount++;
			ruleIter ++ ;
		}
		break;
	case POPULATE_APPLICATION_DETAILS:
		nRowCount = 0;
		rules = appdata.m_appLevelRules[strSelItem] ;
			ruleIter = rules.begin() ;
			while(ruleIter != rules.end() )
			{
				m_InmappDetails.InsertItem(nRowCount,CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str()));
				m_InmappDetails.SetItemText(nRowCount,1,CString((*ruleIter)->statusToString().c_str()));
				nRowCount++;
				ruleIter ++ ;
			}            
		break;
	case POPULATE_VERSION_DETAILS:
		nRowCount = 0;
		//MS SQL Server version level validation
		//MS Exchange Server level validation
		{
			std::map<std::string, ExchAppVersionDiscInfo>::iterator exchDiscIter = appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.begin() ;
			while( exchDiscIter != appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.end() )
			{
				if(strSelItem.Compare(exchDiscIter->first.c_str()) == 0)
				{
					rules = appdata.m_versionLevelRules[strSelItem];
					ruleIter = rules.begin();
					while(ruleIter != rules.end())
					{
						m_InmappDetails.InsertItem(nRowCount,CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str())) ;
						m_InmappDetails.SetItemText(nRowCount,1,CString((*ruleIter)->statusToString().c_str()));
						nRowCount++;
						ruleIter ++ ;
					}
				}
				exchDiscIter++;
			}
			std::list<MSSQLVersionDiscoveryInfo>::iterator MSSQLVersionDiscoveryInfoBegIter;
			std::list<MSSQLVersionDiscoveryInfo>::iterator MSSQLVersionDiscoveryInfoEndIter;
			MSSQLVersionDiscoveryInfoBegIter = appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin();
			MSSQLVersionDiscoveryInfoEndIter = appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end();
			while(MSSQLVersionDiscoveryInfoBegIter != MSSQLVersionDiscoveryInfoEndIter)
			{
				if(strSelItem.Compare(appdata.appType(MSSQLVersionDiscoveryInfoBegIter->m_type)) == 0)
				{
					rules = appdata.m_versionLevelRules[strSelItem];
					ruleIter = rules.begin();
					while(ruleIter != rules.end())
					{
						m_InmappDetails.InsertItem(nRowCount,CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str())) ;
						m_InmappDetails.SetItemText(nRowCount,1,CString((*ruleIter)->statusToString().c_str()));
						nRowCount++;
						ruleIter ++ ;
					}
					break;
				}
				MSSQLVersionDiscoveryInfoBegIter++;
			}
		}
		break;
	case POPULATE_STORAGEGROUP_DETAILS:
		nRowCount = 0;
		break;
	case POPULATE_INSTANCE_DETAILS:
        nRowCount = 0;
		rules = appdata.m_instanceRulesMap[(char *)(LPCTSTR)strSelItem] ;
		ruleIter = rules.begin() ;
		while(ruleIter != rules.end() )
			{
				m_InmappDetails.InsertItem(nRowCount,CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str()));
				m_InmappDetails.SetItemText(nRowCount,1,CString((*ruleIter)->statusToString().c_str()));
				nRowCount++;
				ruleIter ++ ;
			}
		break;
	case POPULATE_SERVNTNAME_DETAILS:
        nRowCount = 0;
		rules = appdata.m_instanceRulesMap[(char *)(LPCTSTR)strSelItem] ;
		ruleIter = rules.begin() ;
		while(ruleIter != rules.end() )
			{
				m_InmappDetails.InsertItem(nRowCount,CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str()));
				m_InmappDetails.SetItemText(nRowCount,1,CString((*ruleIter)->statusToString().c_str()));
				nRowCount++;
				ruleIter ++ ;
			}
		break;
	default:
		break;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void CAppWizardDlg::OnLvnItemchangedListApp(NMHDR *pNMHDR, LRESULT *pResult)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	if(m_flagValidate && (pNMLV->uNewState == 3) && (pNMLV->uOldState == 0))
	{
		CString strSelTreeItem,strSelListItem;
		strSelTreeItem = m_Inmapptree.GetItemText(m_Inmapptree.GetSelectedItem());
		strSelListItem = m_InmappDetails.GetItemText(pNMLV->iItem,pNMLV->iSubItem);
		showComments(strSelTreeItem,strSelListItem);
	}

	*pResult = 0;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}
void CAppWizardDlg::showComments(CString strTreeItem, CString strListItem)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	AppData& appdata = *AppData::GetInstance();
    RuleEnginePtr enginePtr = RuleEngine::getInstance() ;
    std::list<ValidatorPtr>::iterator ruleIter ;
    std::list<ValidatorPtr> rules ;
	MSSQLInstanceMetaData metaData ;
	USES_CONVERSION;
	CString tempStr,strSelTVIParent;
	strSelTVIParent = m_Inmapptree.GetItemText(m_Inmapptree.GetParentItem(m_Inmapptree.GetSelectedItem()));

	if(appdata.isVersion(strTreeItem))//For version
	{
		m_commentsEdit.Format(_T("**Infomation not available**"));
		//MS SQL Version
		//==============
		//MS Exchange Version
		bool bFound = false;
		std::map<std::string,ExchAppVersionDiscInfo>::iterator exchIter =  appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.begin();
		while(exchIter != appdata.m_pExchangeApp.m_exchDiscoveryInfoMap.end())
		{
			if(strTreeItem.Compare(exchIter->first.c_str()) == 0)
			{
				rules = appdata.m_versionLevelRules[strTreeItem];
				ruleIter = rules.begin();
				while(ruleIter != rules.end())
				{
					if(strListItem.Compare(CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str())) == 0)
					{
						std::pair<std::string,std::string> RuleErrFix;
						std::string RuleErr =  Validator::getErrorString( (*ruleIter)->getRuleExitCode() );
						std::string RuleId = boost::lexical_cast<std::string>( (*ruleIter)->getRuleId() );
						std::string RuleDescr = Validator::GetNameAndDescription((*ruleIter)->getRuleId()).second ;
						RuleErrFix = Validator::GetErrorData((*ruleIter)->getRuleId(),(*ruleIter)->getRuleExitCode());
						m_commentsEdit.Format( _T("RULE ID: %s\n\nDESCRIPTION: %s\n\nRULE RESULT: %s\n\n%s\n\n %s\n\n%s" ),
						CString(RuleId.c_str()),
						CString(RuleDescr.c_str()),
						CString(RuleErr.c_str()),
						CString((*ruleIter)->getRuleMessage().c_str()),
						CString(RuleErrFix.first.c_str()),
						CString(RuleErrFix.second.c_str()));
						bFound = true;
					}
					ruleIter ++ ;
				}
			}
			exchIter++;
		}
		if(bFound == false)
		{
			std::list<MSSQLVersionDiscoveryInfo>::iterator sqlVersionDiscInfoBegIter = appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.begin();
			std::list<MSSQLVersionDiscoveryInfo>::iterator sqlVersionDiscInfoEndIter = appdata.m_sqlApp.m_mssqlDiscoveryInfo.m_sqlVersionDiscoveryInfo.end();
			while(sqlVersionDiscInfoBegIter != sqlVersionDiscInfoEndIter)
			{
				if(strTreeItem.Compare(appdata.appType(sqlVersionDiscInfoBegIter->m_type)) == 0)
				{
					rules = appdata.m_versionLevelRules[strTreeItem];
					ruleIter = rules.begin();
					while(ruleIter != rules.end())
					{
						if(strListItem.Compare(CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str())) == 0)
						{
							std::pair<std::string,std::string> RuleErrFix;
							std::string RuleErr =  Validator::getErrorString( (*ruleIter)->getRuleExitCode() );
							std::string RuleId = boost::lexical_cast<std::string>( (*ruleIter)->getRuleId() );
							std::string RuleDescr = Validator::GetNameAndDescription((*ruleIter)->getRuleId()).second ;
							RuleErrFix = Validator::GetErrorData((*ruleIter)->getRuleId(),(*ruleIter)->getRuleExitCode());
							m_commentsEdit.Format( _T("RULE ID: %s\n\nDESCRIPTION: %s\n\nRULE RESULT: %s\n\n%s\n\n %s\n\n%s" ),
								CString(RuleId.c_str()),
								CString(RuleDescr.c_str()),
								CString(RuleErr.c_str()),
								CString((*ruleIter)->getRuleMessage().c_str()),
								CString(RuleErrFix.first.c_str()),
								CString(RuleErrFix.second.c_str()));
						}
						ruleIter ++ ;
					}
				}			
				sqlVersionDiscInfoBegIter++;
			}
		}
		UpdateData(false);
	}
	else if(appdata.isInstance(strTreeItem) || strSelTVIParent.Compare(appdata.appType(INM_APP_FILESERVER)) == 0 )//For instance
	{
		rules = appdata.m_instanceRulesMap[(char *)(LPCTSTR)strTreeItem] ;
		DebugPrintf(SV_LOG_DEBUG, "there in map1%s\n", strTreeItem) ;

		ruleIter = rules.begin() ;
		while(ruleIter != rules.end() )
			{
				if(strListItem.Compare(CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str()))== 0)
				{
						std::pair<std::string,std::string> RuleErrFix;
						std::string RuleErr =  Validator::getErrorString( (*ruleIter)->getRuleExitCode() );
						std::string RuleId = Validator::getInmRuleIdStringFromID((*ruleIter)->getRuleId() );
						std::string RuleDescr = Validator::GetNameAndDescription((*ruleIter)->getRuleId()).second;
						RuleErrFix = Validator::GetErrorData((*ruleIter)->getRuleId(),(*ruleIter)->getRuleExitCode());
						m_commentsEdit.Format( _T("RULE ID: %s\n\nDESCRIPTION: %s\n\nRULE RESULT: %s\n\n%s\n\n %s\n\n%s" ),
						CString(RuleId.c_str()),
						CString(RuleDescr.c_str()),
						CString(RuleErr.c_str()),
						CString((*ruleIter)->getRuleMessage().c_str()),
						CString(RuleErrFix.first.c_str()),
						CString(RuleErrFix.second.c_str()));
				}
				ruleIter ++ ;
			}
		UpdateData(false);
	}
	else if(strTreeItem.Compare(m_Inmapptree.GetItemText(m_Inmapptree.GetRootItem())) == 0)// For Host System info
	{
		rules = appdata.m_sysLevelRules ;
		ruleIter = rules.begin() ;
		while(ruleIter != rules.end() )
		{
			if(strListItem.Compare(CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str())) == 0)
			{
						std::pair<std::string,std::string> RuleErrFix;
						std::string RuleErr =  Validator::getErrorString( (*ruleIter)->getRuleExitCode() );
						std::string RuleId = Validator::getInmRuleIdStringFromID((*ruleIter)->getRuleId() );
						std::string RuleDescr = Validator::GetNameAndDescription((*ruleIter)->getRuleId()).second;
						RuleErrFix = Validator::GetErrorData((*ruleIter)->getRuleId(),(*ruleIter)->getRuleExitCode());
						m_commentsEdit.Format( _T("RULE ID: %s\n\nDESCRIPTION: %s\n\nRULE RESULT: %s\n\n%s\n\n %s\n\n%s" ),
						CString(RuleId.c_str()),
						CString(RuleDescr.c_str()),
						CString(RuleErr.c_str()),
						CString((*ruleIter)->getRuleMessage().c_str()),
						CString(RuleErrFix.first.c_str()),
						CString(RuleErrFix.second.c_str()));
			}
			ruleIter ++ ;
		}
		UpdateData(false);
	}
	else if(strTreeItem.Compare(appdata.appType(INM_APP_MSSQL)) == 0 || strTreeItem.Compare(appdata.appType(INM_APP_EXCHNAGE)) == 0)// For MS SQL Serrver or MS Exchage Server Applications
	{
		rules = appdata.m_appLevelRules[strTreeItem] ;
		ruleIter = rules.begin() ;
		while(ruleIter != rules.end() )
		{
			if(strListItem.Compare(CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str())) == 0)
			{
						std::pair<std::string,std::string> RuleErrFix;
						std::string RuleErr =  Validator::getErrorString( (*ruleIter)->getRuleExitCode() );
						std::string RuleId = Validator::getInmRuleIdStringFromID((*ruleIter)->getRuleId() );
						std::string RuleDescr = Validator::GetNameAndDescription((*ruleIter)->getRuleId()).second ;
						RuleErrFix = Validator::GetErrorData((*ruleIter)->getRuleId(),(*ruleIter)->getRuleExitCode());
						m_commentsEdit.Format( _T("RULE ID: %s\n\nDESCRIPTION: %s\n\nRULE RESULT: %s\n\n%s\n\n %s\n\n%s" ),
						CString(RuleId.c_str()),
						CString(RuleDescr.c_str()),
						CString(RuleErr.c_str()),
						CString((*ruleIter)->getRuleMessage().c_str()),
						CString(RuleErrFix.first.c_str()),
						CString(RuleErrFix.second.c_str()));
			}
			ruleIter ++ ;
		}
		UpdateData(false);
	}
	else if(appdata.isStorageGroup(strTreeItem,strSelTVIParent))// For Storage Group
	{
		rules = appdata.m_sgLevelRules[strTreeItem+strSelTVIParent] ;
		ruleIter = rules.begin() ;
		while(ruleIter != rules.end() )
		{
			if(strListItem.Compare(CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str())) == 0)
			{
						std::pair<std::string,std::string> RuleErrFix;
						std::string RuleErr =  Validator::getErrorString( (*ruleIter)->getRuleExitCode() );
						std::string RuleId = Validator::getInmRuleIdStringFromID((*ruleIter)->getRuleId() );
						std::string RuleDescr = Validator::GetNameAndDescription((*ruleIter)->getRuleId()).second;
						RuleErrFix = Validator::GetErrorData((*ruleIter)->getRuleId(),(*ruleIter)->getRuleExitCode());
						m_commentsEdit.Format( _T("RULE ID: %s\n\nDESCRIPTION: %s\n\nRULE RESULT: %s\n\n%s\n\n %s\n\n%s" ),
						CString(RuleId.c_str()),
						CString(RuleDescr.c_str()),
						CString(RuleErr.c_str()),
						CString((*ruleIter)->getRuleMessage().c_str()),
						CString(RuleErrFix.first.c_str()),
						CString(RuleErrFix.second.c_str()));
			}
			ruleIter ++ ;
		}
		UpdateData(false);
	}
	else if(strTreeItem.Compare(appdata.appType(INM_APP_FILESERVER)) == 0)
	{
		rules = appdata.m_appLevelRules[strTreeItem];
		ruleIter = rules.begin() ;
		while(ruleIter != rules.end() )
		{
			std::string str = boost::lexical_cast<std::string>( (*ruleIter)->getRuleExitCode() );
			std::string RuleId = boost::lexical_cast<std::string>( (*ruleIter)->getRuleId() );
			if(strListItem.Compare(CString(Validator::GetNameAndDescription((*ruleIter)->getRuleId()).first.c_str())) == 0)
			{
						std::pair<std::string,std::string> RuleErrFix;
						std::string RuleErr =  Validator::getErrorString( (*ruleIter)->getRuleExitCode() );
						std::string RuleId = Validator::getInmRuleIdStringFromID((*ruleIter)->getRuleId() );
						std::string RuleDescr = Validator::GetNameAndDescription((*ruleIter)->getRuleId()).second;
						RuleErrFix = Validator::GetErrorData((*ruleIter)->getRuleId(),(*ruleIter)->getRuleExitCode());
						m_commentsEdit.Format( _T("RULE ID: %s\n\nDESCRIPTION: %s\n\nRULE RESULT: %s\n\n%s\n\n %s\n\n%s" ),
						CString(RuleId.c_str()),
						CString(RuleDescr.c_str()),
						CString(RuleErr.c_str()),
						CString((*ruleIter)->getRuleMessage().c_str()),
						CString(RuleErrFix.first.c_str()),
						CString(RuleErrFix.second.c_str()));
			}
			ruleIter ++ ;
		}
		UpdateData(false);
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void CAppWizardDlg::OnBnClickedButtonBack()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	AppData& appdata = *AppData::GetInstance();
	CString btnText;
	m_InmbtnValidate.GetWindowText(btnText);
	BeginWaitCursor();
	if(btnText.Compare(_T("Next")) == 0)
	{
		displayDiscApps();
		appdata.Clear();
	}
	if(btnText.Compare(_T("Summary")) == 0)
	{
		displayDiscInfo();
	}
	else if(btnText.Compare(_T("Report To CX")) == 0)
	{
		m_flagValidate = true;
		m_InmappDetails.ShowWindow(SW_SHOW);
		m_Inmapptree.ShowWindow(SW_SHOW);
		GetDlgItem(IDC_RICHEDIT2_COMMENTS)->ShowWindow(SW_SHOW);
		m_InmbtnValidate.EnableWindow();
		m_InmbtnValidate.SetWindowText(_T("Summary"));
		m_groupWzrdState.SetWindowText(_T("Validation Information"));
		GetDlgItem(IDC_RICHEDIT2_SUMMARY)->ShowWindow(SW_HIDE);
		appdata.m_strSummary.Empty();

	}
	EndWaitCursor();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void CAppWizardDlg::OnBnClickedButtonRediscover()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	if(AfxMessageBox(_T("If you re-discover, you will loose curreent data. Are you sure to rediscover ? "),MB_YESNO) == IDNO)
	{
		return;
	}
	ShowWindow(SW_HIDE);

	//clearing the old data STARTED=====>
	AppData::GetInstance()->Clear();
	//=========>ENDED
	
	//Re-discovering applications
	CString strTitle = _T("Re Discovering Applications ...");
	AfxBeginThread(InitDiscAppsThreadFunc,&strTitle);
	if(m_dlgProcessing.DoModal() == IDCANCEL)
	{
		exit(0);
	}

	ShowWindow(SW_SHOW);

	m_Inmapptree.ShowWindow(SW_SHOW);
	m_InmappDetails.ShowWindow(SW_SHOW);
	GetDlgItem(IDC_RICHEDIT2_COMMENTS)->ShowWindow(SW_SHOW);
	GetDlgItem(IDC_RICHEDIT2_SUMMARY)->ShowWindow(SW_HIDE);

    displayDiscApps() ;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}
void CAppWizardDlg::displaySummary(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	AppData& appdata = *AppData::GetInstance();
	BeginWaitCursor();

	AfxBeginThread(GenerateSummaryThreadFunc,NULL);
	if(m_dlgProcessing.DoModal() == IDCANCEL)
	{
		appdata.GenerateSummary();//Temporary Logic
	}
	m_validatonSummary.Format(appdata.m_strSummary);
	UpdateData(false);

	EndWaitCursor();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void CAppWizardDlg::OnBnClickedClose()
{
	EndDialog(0);
}

void CAppWizardDlg::OnCancel()
{
}
int CAppWizardDlg::populateServiceNprocessInfo(int nRowCount,std::list<InmService>& listInmServices,std::list<InmProcess>& listInmProcess)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	std::list<InmProcess>::iterator iterInmProcesses = listInmProcess.begin();
	std::list<InmService>::iterator iterInmServices = listInmServices.begin();
	if(iterInmServices != listInmServices.end())
	{
		m_InmappDetails.InsertItem(nRowCount,_T("Services"));
		nRowCount++;
        m_InmappDetails.InsertItem(nRowCount,_T("----------"));
		nRowCount++;
	}	
	while(iterInmServices != listInmServices.end())
	{
        std::list<std::pair<std::string, std::string> > proeprties = iterInmServices->getProperties() ;
        std::list<std::pair<std::string, std::string> >::iterator propIter = proeprties.begin() ;
        while( propIter != proeprties.end() )
        {
            CString PropertyName = "        " ;
            PropertyName += propIter->first.c_str() ;
            m_InmappDetails.InsertItem(nRowCount,_T(PropertyName));
            m_InmappDetails.SetItemText(nRowCount,1,CString(propIter->second.c_str()));
		    nRowCount++;
            propIter++ ;
        }
		m_InmappDetails.InsertItem(nRowCount,_T(" "));
		nRowCount++;
		iterInmServices++;
	}

	if(iterInmProcesses != listInmProcess.end())
	{
		m_InmappDetails.InsertItem(nRowCount,_T("Processes"));
		nRowCount++;
	}
	while(iterInmProcesses != listInmProcess.end())
	{
		InmProcess& tempInmProcess = *iterInmProcesses;
		m_InmappDetails.InsertItem(nRowCount,_T("        Path"));
		m_InmappDetails.SetItemText(nRowCount,1,CString(tempInmProcess.m_path.c_str()));
		nRowCount++;
		iterInmProcesses++;
	}
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
    return nRowCount;
}
void CAppWizardDlg::OnNMClickTreeApp(NMHDR *pNMHDR, LRESULT *pResult)
{
	SetTimer(10, 100, NULL);
	*pResult = 0;
}

void CAppWizardDlg::OnTimer(UINT_PTR nIDEvent)
{
	if(nIDEvent == 10)
	{
		HTREEITEM tvItem = m_Inmapptree.GetRootItem();
		if(m_Inmapptree.GetCheck(tvItem))
		{
			m_InmbtnValidate.EnableWindow(true);
			return;
		}
		tvItem = m_Inmapptree.GetChildItem(tvItem);
		while(tvItem != NULL)
		{
			if(m_Inmapptree.GetCheck(tvItem))
			{
				m_InmbtnValidate.EnableWindow(true);
				return;
			}
			tvItem = m_Inmapptree.GetNextItem(tvItem,TVGN_NEXT);
		}
		m_InmbtnValidate.EnableWindow(false);
	}

	CDialog::OnTimer(nIDEvent);
}
