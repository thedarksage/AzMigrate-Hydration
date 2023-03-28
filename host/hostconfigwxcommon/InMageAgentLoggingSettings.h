#pragma once
#include "InmConfigData.h"
#include "afxcmn.h"
#include "afxwin.h"

class CInMageAgentLoggingSettings : public CPropertyPage
{
	DECLARE_DYNAMIC(CInMageAgentLoggingSettings)

	bool m_bChangeAttempted;

public:
	CInMageAgentLoggingSettings();
	virtual ~CInMageAgentLoggingSettings();

	bool ValidateAgentConfigChange();
	bool IsAgentConfigChanged();
	void SyncAgentConfigData(LOGGING_SETTINGS &agentLoggingSettings);
	void SyncAgentConfigData();
	void ResetConfigChangeStatus(bool bStatus = false);

	enum { IDD = IDD_PROPPAGE_LOGGING };

protected:
	virtual void DoDataExchange(CDataExchange* pDX); 

	DECLARE_MESSAGE_MAP()
public:

	//Control variables
	CSpinButtonCtrl m_spLocalLoglevel;
	CSpinButtonCtrl m_spRemoteLoglevel;
	CSpinButtonCtrl m_spFxLoglevel;
	CString m_vxLocalLoglevel;
	CString m_vxRemoteLoglevel;
	CString m_fxLoglevel;

	//Events on controls
	afx_msg void OnEnChangeEditLocalLogLevel();
	afx_msg void OnEnChangeEditRemoteLogLevel();
	afx_msg void OnEnChangeEditFxLogLevel();

	virtual BOOL OnInitDialog();
	virtual BOOL OnKillActive();

	//Control variables used when hiding controls
	CStatic m_vxLogGroupBox;
	CStatic m_fxLogGroupBox;
	CStatic m_vxLocalStaticText;
	CStatic m_vxRemoteStaticText;
	CEdit m_vxLocalEditCtrl;
	CEdit m_vxRemoteEditCtrl;
	CStatic m_fxLogStaticText;
	CEdit m_fxLogEditCtrl;
};
