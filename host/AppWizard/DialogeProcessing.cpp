// DialogeProcessing.cpp : implementation file
//

#include "stdafx.h"
#include "AppWizard.h"
#include "DialogeProcessing.h"
#include ".\dialogeprocessing.h"
#define MAX_RECT_WIDTH 60

CEvent g_dlgEvent;

IMPLEMENT_DYNAMIC(CDialogeProcessing, CDialog)
CDialogeProcessing::CDialogeProcessing(CWnd* pParent /*=NULL*/)
	: CDialog(CDialogeProcessing::IDD, pParent)
{
	bTimerOK = false;
	m_strTitle = _T("");
}
CDialogeProcessing::~CDialogeProcessing()
{
}

void CDialogeProcessing::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDialogeProcessing, CDialog)
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_MESSAGE(DISC_MSG_STEP_FINISH,ValidateAppFinish)
	ON_MESSAGE(VALIDATE_APP_FINISH,ValidateAppFinish)
	ON_MESSAGE(GENERATE_SUMMARY_FINISH,ValidateAppFinish)
	ON_MESSAGE(REPORT_TO_CX_FINISHED,ValidateAppFinish)
	ON_MESSAGE(SET_TITLE,SetTitle)
END_MESSAGE_MAP()


// CDialogeProcessing message handlers

void CDialogeProcessing::OnPaint()
{
	CPaintDC dc(this);

	dc.Draw3dRect(&m_rectLayout,::GetSysColor(COLOR_3DSHADOW),::GetSysColor(COLOR_3DDKSHADOW));
	dc.FillSolidRect(m_rectProcessCtrl,RGB(250,250,250));
	if(bTimerOK)
	{
		if(m_xPos >= m_rectProcessCtrl.right)
			m_xPos = m_rectProcessCtrl.left-40;

		int nSubRect = 0;
		CRect subRect;
		subRect.left = m_xPos;
		subRect.top = m_rectProcessCtrl.top + 1;
		subRect.bottom = m_rectProcessCtrl.top + 15;
		while(nSubRect < 4)
		{
			subRect.right = subRect.left + 18;
			dc.FillSolidRect(&subRect,RGB(190,190,190));
			subRect.left += 20;
			nSubRect++;
		}

		m_xPos += 10;
		bTimerOK = false;
	}
}

BOOL CDialogeProcessing::OnInitDialog()
{
	CDialog::OnInitDialog();

	g_dlgEvent.SetEvent();

	CRect rectClient;
	GetClientRect(&rectClient);
	m_rectLayout.left = rectClient.left + 10;
	m_rectLayout.top = rectClient.top + 10;
	m_rectLayout.bottom = m_rectLayout.top + 20;
	m_rectLayout.right = rectClient.right - 10;

	m_rectProcessCtrl.left = m_rectLayout.left + 2;
	m_rectProcessCtrl.top = m_rectLayout.top + 2;
	m_rectProcessCtrl.bottom = m_rectProcessCtrl.top + 16;
	m_rectProcessCtrl.right = m_rectLayout.right - 2;

	m_xPos = m_rectProcessCtrl.left-40;
	SetTimer(1,70,NULL);
	return TRUE;
}

void CDialogeProcessing::OnTimer(UINT nIDEvent)
{
	CClientDC dc(this);
	if(!bTimerOK)
		bTimerOK = true;
	InvalidateRect(&m_rectProcessCtrl);
	CDialog::OnTimer(nIDEvent);
}

void CDialogeProcessing::OnClose()
{
	KillTimer(1);
	CDialog::OnClose();
}
LRESULT CDialogeProcessing::ValidateAppFinish(WPARAM,LPARAM)
{
	EndDialog(IDOK);
	return 0;
}
LRESULT CDialogeProcessing::SetTitle(WPARAM,LPARAM)
{
	SetWindowText(m_strTitle);
	return 0;
}
