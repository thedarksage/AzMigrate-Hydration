#pragma once
#include "InmConfigData.h"
#include "afxwin.h"

class CInMageLogonSettings : public CPropertyPage
{
	DECLARE_DYNAMIC(CInMageLogonSettings)

	bool m_bChangeAttempted;
	bool m_bPasswdChanged;
	bool m_bShowMandatoryFields;
	bool ShowUserPickDlg(CString &scoutSvcLogonUserName);
	void EnableUserConfigGroup(bool bEnable);
	void ValidateUserConfigGroup(void);

public:
	CInMageLogonSettings();
	virtual ~CInMageLogonSettings();

	bool ValidateAgentConfigChange();
	bool IsAgentConfigChanged();
	BOOL SyncAgentConfigData(LOGON_SETTINGS &agentLogonSettings);
	void SyncAgentConfigData();
	void ResetConfigChangeStatus(bool bStatus = false);

	enum { IDD = IDD_PROPPAGE_LOGON };

protected:
	virtual void DoDataExchange(CDataExchange* pDX); 

	DECLARE_MESSAGE_MAP()
public:
	//Control variables
	CString m_agentSvcLogonName;
	CString m_agentSvcLogonPwd;
	int m_fxAgentGroupBox;
	int m_useUserAccount;
	
	//Control event handlers
	afx_msg void OnEnChangeEditUserName();
	afx_msg void OnEnChangeEditPassword();
	afx_msg void OnBnClickedRadioFxLocalSystem();
	afx_msg void OnBnClickedRadioFxUserAcount();
	afx_msg void OnBnClickedBtUserSearch();
	afx_msg void OnBnClickedCheckAppagentUser();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

	//Override default property page events for host config
	virtual BOOL OnInitDialog();
	virtual BOOL OnKillActive();
	virtual BOOL OnSetActive();

	//Control variables used when hiding controls
	CStatic m_appAgentGroupBox;
	CButton m_appAgentUserCheckBox;
	CStatic m_appAgentNoteStaticText;
	CStatic m_fxAgentGroupBoxCtrl;
	CButton m_fxLocalSysRadioButton;
	CButton m_fxUserAccRadioButton;
};
