#pragma once
#include "afxcmn.h"
#include "Resource.h"

// CInitializerDlg dialog

class CInitializerDlg : public CDialog
{
	DECLARE_DYNAMIC(CInitializerDlg)

public:
	CInitializerDlg(CWnd* pParent = NULL);
	virtual ~CInitializerDlg();

// Dialog Data
	enum { IDD = IDD_DIALOG_INITIALIZER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
	void CloseDialoge(void);
	CString m_discStatusNote;
public:
	virtual BOOL OnInitDialog();
	LRESULT OnDiscMsgStepFinish(WPARAM wParam,LPARAM lParaam);
	LRESULT OnDiscMsgStepProcess(WPARAM wParam,LPARAM lParaam);
};
