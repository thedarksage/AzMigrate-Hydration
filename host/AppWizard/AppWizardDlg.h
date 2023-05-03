// AppWizardDlg.h : header file
//
#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "atltypes.h"
#include "dialogeprocessing.h"
#include "appglobals.h"
#include "service.h"
#include "MyTreeCtrl.h"

// CAppWizardDlg dialog
class CAppWizardDlg : public CDialog
{
// Construction
public:
	CAppWizardDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_APPWIZARD_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hInmIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	void displayDiscApps();
	void displayDiscInfo();
	void OnSysCommand(UINT nID, LPARAM lParam);
	void OnPaint();
	HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
	CMyTreeCtrl m_Inmapptree;
	CImageList m_InmappTreeImageList,m_appListCtrlImageList;
	CListCtrl m_InmappDetails;
	CString m_commentsEdit;
	CButton m_InmbtnValidate;
	HTREEITEM m_InmsysInfo;
	HTREEITEM m_InmMSSQL;
	HTREEITEM m_InmExchange;
	HTREEITEM m_InmFileServer;
    HTREEITEM m_InmBES;
	CRect m_InmbtnAreaRect;
protected:
	void updateTree();
public:
	void OnTvnSelchangedTreeApp(NMHDR *pNMHDR, LRESULT *pResult);
protected:
	void populateAppDetailsList(UINT choice,CString strSelItem = NULL,CString strSelItemParent = NULL,CString strSelItemGrandParent = NULL);
public:
	void OnBnClickedButtonValidate();
	void populateValidateInfo(UINT choice = 0, CString strSelItem = NULL,CString strSelItemParent = NULL,CString strSelItemGrandParent = NULL);
protected:
	bool m_flagValidate;
	bool m_flagStep1;
public:
	void OnLvnItemchangedListApp(NMHDR *pNMHDR, LRESULT *pResult);
	void showComments(CString strTreeItem, CString strListItem);
protected:
	CButton m_btnBack;
public:
	void OnBnClickedButtonBack();
protected:
	CButton m_btnRediscover;
public:
	void OnBnClickedButtonRediscover();
	CStatic m_groupWzrdState;
public:
	CString m_validatonSummary;
protected:
	void displaySummary(void);
public:
	void OnBnClickedClose();
protected:
	virtual void OnCancel();
protected:
	int populateServiceNprocessInfo(int nRowCount,std::list<InmService>& listInmServices,std::list<InmProcess>& listInmProcess);
public:
	void OnNMClickTreeApp(NMHDR *pNMHDR, LRESULT *pResult);
	void OnTimer(UINT_PTR nIDEvent);
};
