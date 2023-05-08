#pragma once
#include "atltypes.h"
#include "afxwin.h"
#include "afxmt.h"

// CDialogeProcessing dialog
class CDialogeProcessing : public CDialog
{
	DECLARE_DYNAMIC(CDialogeProcessing)

public:
	CDialogeProcessing(CWnd* pParent = NULL);
	virtual ~CDialogeProcessing();

// Dialog Data
	enum { IDD = IDD_DIALOG_PROCESSING };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
public:
	void OnPaint();
protected:
	CRect m_rectProcessCtrl;
	CRect m_rectLayout;
	bool bTimerOK;
	int m_xPos;
public:
	virtual BOOL OnInitDialog();
	void OnTimer(UINT nIDEvent);
	void OnClose();
	LRESULT ValidateAppFinish(WPARAM wParam,LPARAM lParam);
	CString m_strTitle;
	LRESULT SetTitle(WPARAM wParam,LPARAM lParam);
};
