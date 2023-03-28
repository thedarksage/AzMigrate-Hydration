// InitializerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "AppWizard.h"
#include "InitializerDlg.h"
#include ".\initializerdlg.h"

// CInitializerDlg dialog

IMPLEMENT_DYNAMIC(CInitializerDlg, CDialog)
CInitializerDlg::CInitializerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CInitializerDlg::IDD, pParent)
	, m_discStatusNote(_T(""))
{
}

CInitializerDlg::~CInitializerDlg()
{
}

void CInitializerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_DISCOVERY, m_discStatusNote);
}

BEGIN_MESSAGE_MAP(CInitializerDlg, CDialog)
	ON_MESSAGE(DISC_MSG_STEP_FINISH,OnDiscMsgStepFinish)
	ON_MESSAGE(DISC_MSG_STEP_PROCESS,OnDiscMsgStepProcess)
END_MESSAGE_MAP()


// CInitializerDlg message handlers

void CInitializerDlg::CloseDialoge(void)
{
}

BOOL CInitializerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText(_T("Discovering Application Information"));
	m_discStatusNote.Format(_T("Discovering Applications ... It takes few minutes to complete"));
	UpdateData(false);
	// TODO:  Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
LRESULT CInitializerDlg::OnDiscMsgStepFinish(WPARAM wParam,LPARAM lParaam)
{
	EndDialog(IDOK);
	return 0;
}
LRESULT CInitializerDlg::OnDiscMsgStepProcess(WPARAM wParam,LPARAM lParaam)
{
	Sleep(100);
	PostMessage(DISC_MSG_STEP_PROCESS,(WPARAM)0,(LPARAM)0);
	return 0;
}
