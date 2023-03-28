// UnLockDrivesDlg.h : header file
//

#pragma once
#include "afxwin.h"


// CUnLockDrivesDlg dialog
class CUnLockDrivesDlg : public CDialog
{
// Construction
public:
	CUnLockDrivesDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_UNLOCKDRIVES_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;
	CString m_strSelectedDrives;
	CListBox DrvSelDrives;
	int m_IsDisplayDialog;


	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedOk();
	afx_msg void OnLbnSelchangeSeldrvList();
    void Tokenize(const std::string& input,
        std::vector<std::string>& outtoks,
        const std::string& separators = ",");
	void SetNewHExtent(LPCTSTR lpszNewString);
	int GetTextLen(LPCTSTR lpszText);

	DECLARE_MESSAGE_MAP()
public:
	//afx_msg void OnBnClickedOk();
	//afx_msg void OnLbnSelchangeSeldrvList();
};
