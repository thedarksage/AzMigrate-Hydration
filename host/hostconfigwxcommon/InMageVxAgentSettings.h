#pragma once
#include "InmConfigData.h"

class CInMageVxAgentSettings : public CPropertyPage
{
	DECLARE_DYNAMIC(CInMageVxAgentSettings)

	bool m_bChangeAttempted;

public:
	CInMageVxAgentSettings();
	virtual ~CInMageVxAgentSettings();

	bool ValidateAgentConfigChange();
	bool IsAgentConfigChanged();
	BOOL SyncAgentConfigData(AGENT_SETTINGS& vxAgentSettings);
	void SyncAgentConfigData();
	void ResetConfigChangeStatus(bool bStatus = false);

	enum { IDD = IDD_PROPPAGE_AGENT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
public:

	//Control variables
	CString m_cacheDirPath;
	CString m_stCacheDirChangeNote;

	//Control event handlers
	afx_msg void OnEnChangeEditCacheDirPath();
	afx_msg void OnBnClickedBtnBrowseCacheDir();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

	virtual BOOL OnKillActive();
};
