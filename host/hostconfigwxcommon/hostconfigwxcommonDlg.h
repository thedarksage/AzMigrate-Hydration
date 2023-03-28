#pragma once

class ChostconfigwxcommonDlg : public CDialog
{
public:
	ChostconfigwxcommonDlg(CWnd* pParent = NULL);

	//ID of host config common dialog
	enum { IDD = IDD_HOSTCONFIGWXCOMMON_DIALOG };

protected:
	//Exchange data between host config controls and control variables
	virtual void DoDataExchange(CDataExchange* pDX);

protected:

	//Override default init dialog for host config 
	virtual BOOL OnInitDialog();

	//Host config main dialog message handler functions
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();

	DECLARE_MESSAGE_MAP()
};
