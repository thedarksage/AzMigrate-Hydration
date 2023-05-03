#pragma once
#include "InMageCXSettings.h"
#include "InMageVxAgentSettings.h"
#include "InMageAgentLoggingSettings.h"
#include "InMageLogonSettings.h"


class CInMageConfigPropertySheet : public CPropertySheet
{
	DECLARE_DYNAMIC(CInMageConfigPropertySheet)

	bool m_bAgentConfigThreadRunning;
	bool m_bSaveConfigAndExit;

public:
	CInMageConfigPropertySheet(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CInMageConfigPropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	virtual ~CInMageConfigPropertySheet();
	virtual BOOL OnInitDialog();

	void SaveHostAgentConfig();
	enum INM_BACKEND_STATUS
	{
		INM_BACKEND_STATUS_INITIALIZING,
		INM_BACKEND_STATUS_SAVING
	};
	void ShowBackEndStatus(INM_BACKEND_STATUS status, bool bShow = true);
	void EnabledWizardPages(bool bEnable=true);

	//Message handlers that will execute upon read/save success/failure
	LRESULT OnConfigReadSuccess(WPARAM,LPARAM);
	LRESULT OnConfigReadFailed(WPARAM,LPARAM);
	LRESULT OnConfigSaveSuccess(WPARAM,LPARAM);
	LRESULT OnConfigSaveFailed(WPARAM,LPARAM);

private:
	//Host configuration property page objects
	CInMageCXSettings m_cxSettingsPage;
	CInMageVxAgentSettings m_vxAgentSettingsPage;
	CInMageAgentLoggingSettings m_agentLoggingPage;
	CInMageLogonSettings m_logonPage;

protected:
	DECLARE_MESSAGE_MAP()

	//Override property sheet buttons
	afx_msg void OnApplyNow();
	afx_msg void OnOK();
	afx_msg void OnCancel();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnHelp();
};


