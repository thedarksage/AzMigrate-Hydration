// AppWizard.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "AppWizard.h"
#include "AppWizardDlg.h"
#include "AppData.h"
#include <iostream>
#include ".\appwizard.h"
#include "ruleengine.h"
#include "appglobals.h"
#include "configvalueobj.h"
#include "cdputil.h"
#include "appexception.h"
#include "logger.h"
#include <ace/Thread_Manager.h>
#include <boost/shared_ptr.hpp>
#include "controller.h"

ACE_Thread_Manager g_ThreadManager;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
std::list<std::string> installedApps ;

// CAppWizardApp

BEGIN_MESSAGE_MAP(CAppWizardApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


// CAppWizardApp construction

CAppWizardApp::CAppWizardApp()
{
}


// The one and only CAppWizardApp object

CAppWizardApp g_theApp;

CInitializerDlg intlDlg;
CDialogeProcessing m_dlgProcessing;
extern CEvent g_dlgEvent;
UINT InitDiscAppsThreadFunc (LPVOID pParam)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	g_dlgEvent.Lock();
    getExistedAppList() ;
	if(pParam)
		m_dlgProcessing.m_strTitle = *((CString*)pParam);/*_T("Re-discovering Applications ...")*/
	else
        m_dlgProcessing.m_strTitle = _T("InMage Scout - Discovering Applications ...");

	m_dlgProcessing.PostMessage(SET_TITLE,(WPARAM)0,(LPARAM)0);
	m_dlgProcessing.PostMessage(DISC_MSG_STEP_FINISH,(WPARAM)0,(LPARAM)0);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);

    return 1;
}


// CAppWizardApp initialization

BOOL CAppWizardApp::InitInstance()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	CDPUtil::InitQuitEvent() ; 

    SetLogLevel( 7 );   
    SetLogFileName( "AppWizard.log" );
	InitCommonControls();

	CWinApp::InitInstance();
	AfxInitRichEdit2();

	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	ControllerPtr controler = Controller::createInstance(&g_ThreadManager);
	if(controler.get() != NULL)
	{
		std::set<std::string> plcHolder;
		controler->init(plcHolder);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Application Readiness wizard failed to initialize controler object.\n");
		return FALSE;
	}

	//Create AppData Instance
	if (!AppData::GetInstance())
	{
		if (ConfigValueObj::getInstance().getRunOnconsole())
			DebugPrintf(SV_LOG_FATAL, "Could not create AppData object. Out of memory");
		else
			AfxMessageBox("Fatal Error: Could not create AppData object. Out of memory");
		return FALSE;//terminates the applcation
	}

	//Handling command-line arguments
    ConfigValueObj conf_obj= ConfigValueObj::getInstance();
    if( conf_obj.getRunOnconsole() == true )
	{
        performDiscovery();
	    return FALSE;//terminates the applcation
    }

	CAppWizardDlg dlg;

	AfxBeginThread(InitDiscAppsThreadFunc,NULL);
	if(m_dlgProcessing.DoModal() == IDCANCEL)
	{
		exit(0);
	}
	Sleep(100);// For user visibility

	DebugPrintf(SV_LOG_DEBUG, "EXITED discAppInfoThreadFunc & STARTED Main DLG\n") ;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
	}
	else if (nResponse == IDCANCEL)
	{
	}

	//Destroy AppData Instance
	AppData::DestroyInstance();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
	CDPUtil::UnInitQuitEvent();
	return FALSE;
}

SVERROR performDiscovery()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    std::set<InmProtectedAppType> appTypeList = ConfigValueObj::getInstance().getApplications() ;
    std::set<InmProtectedAppType>::iterator appTypeListIter = appTypeList.begin();
	AppData& appdata = *AppData::GetInstance();
    SVERROR retStatus = SVS_OK;
    getHostLevelDiscoveryInfo(appdata.m_hldiscInfo);
    while(appTypeListIter != appTypeList.end())
    {
       switch(*appTypeListIter)
       {
           case INM_APP_EXCHNAGE:
              if(appdata.m_pExchangeApp.init(_T("")) == SVS_OK)
		        {
                    appdata.m_pExchangeApp.discoverApplication();
                    appdata.m_pExchangeApp.discoverMetaData();
                    //appdata.m_pExchangeApp.validateRules();
		        }
                break;
            case INM_APP_MSSQL:
                appdata.getAppData();
		        appdata.m_sqlApp.discoverApplication() ;
		        appdata.m_sqlApp.discoverMetaData();
		        appdata.initAppTreeVector();
               // appdata.m_sqlApp.validateRules();
                break;
           case INM_APP_FILESERVER: 
                appdata.m_fileservApp.discoverApplication();
                //appdata.m_fileservApp.validateRules();
                break;
           case INM_APP_BES:
               appdata.m_besApp.discoverApplication();
               appdata.m_besApp.discoverMetaData(); 
       }
       appTypeListIter++;        
    }
    if( ConfigValueObj::getInstance().getRunOnconsole() )
    {
        appdata.ValidateRules() ;
        appdata.GenerateSummary();
        CStdioFile summaryFile;
	    summaryFile.Open("Summary.txt",CFile::modeWrite | CFile::modeCreate);
	    summaryFile.WriteString(appdata.m_strSummary);
	    summaryFile.Close();
    }
    return retStatus;
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}

void getExistedAppList()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	AppData& appdata = *AppData::GetInstance();
    installedApps.clear() ;
    appdata.Clear() ;
    ConfigValueObj::getInstance().clearApplications(); 
    if( appdata.m_fileservApp.isInstalled() )
    {
        installedApps.push_back("Windows File Server") ;        
    }
	try
	{
		if( appdata.m_pExchangeApp.isInstalled() )
		{
			installedApps.push_back("MS Exchange Server") ;
		}
	}
    catch(AppException& exception)
    {
        DebugPrintf(SV_LOG_ERROR, "getExistedAppList caught exception %s\n", exception.to_string().c_str()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "getExistedAppList caught exception %s\n", ex.what()) ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Unavoidable error while getExistedAppList\n") ;
    }
    if( appdata.m_sqlApp.isInstalled()) 
    {
        installedApps.push_back("MS SQL Server") ;
    }
    if( appdata.m_besApp.isInstalled()) 
    {
        installedApps.push_back("BlackBerry Server") ;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}
