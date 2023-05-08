#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "InmConfigData.h"

class CInMageCXSettings : public CPropertyPage
{
	DECLARE_DYNAMIC(CInMageCXSettings)
	bool m_bChangeAttempted;
	bool m_bVerifyPassphraseOnly;
	bool m_bValidatePassphrase;
public:
	CInMageCXSettings();
	virtual ~CInMageCXSettings();

	bool ValidateAgentConfigChange();
	bool IsAgentConfigChanged();
	BOOL SyncAgentConfigData(CX_SETTINGS& cxSettings);
	void SyncAgentConfigData();
	void ResetConfigChangeStatus(bool bStatus = false);

	enum { IDD = IDD_PROPPAGE_GLOBAL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
public:

	//Control variables
	CString m_CxIPAddress;
	BOOL m_stEnableHTTPS;
	BOOL m_stBtnNATHostname;
	CString m_NatHostname;
	BOOL m_stBtnNatIP;
	CIPAddressCtrl m_NatIPAddress;
	CStatic m_staticInitialize;
	CStatic m_staticInProgress;

private:
	//Control event handlers
	afx_msg void OnIpnFieldchangedCxIpaddress();
	afx_msg void OnEnChangeEditCxPort();
	afx_msg void OnBnClickedCheckEnableHttps();
	afx_msg void OnBnClickedCheckNatHostname();
	afx_msg void OnEnChangeEditNatHostname();
	afx_msg void OnBnClickedCheckNatIp();
	afx_msg void OnIpnFieldchangedNatCxIpaddress(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

	//Override default property page events for host config
	virtual BOOL OnKillActive();
	virtual BOOL OnInitDialog();
	CString m_cxPort;
	CEdit m_cxPortEditCtrl;
	CStatic mandatory_asteric;

	CFont m_cFont;
	void UnderlineText(int dlgItem);
	afx_msg void OnStnClickedStaticModifypassphrase();
	CEdit m_passphraseCtrl;
	CString m_passphraseVal;
	afx_msg void OnEnChangeEditPassphrase();
};
