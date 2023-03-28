#include "stdafx.h"
#include "hostconfigwxcommon.h"
#include "hostconfigwxcommonDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

ChostconfigwxcommonDlg::ChostconfigwxcommonDlg(CWnd* pParent)
	: CDialog(ChostconfigwxcommonDlg::IDD, pParent)
{
	
}

void ChostconfigwxcommonDlg::DoDataExchange(CDataExchange* pDX)
{
	//Do data exchange between controls and variables
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(ChostconfigwxcommonDlg, CDialog)
	//event handler for repainting host config dialog
	ON_WM_PAINT()

	//handles the event when host config is minimized
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL ChostconfigwxcommonDlg::OnInitDialog()
{
	//Will be executed when the host config dialog is initialized
	CDialog::OnInitDialog();	

	return TRUE;  
}

void ChostconfigwxcommonDlg::OnPaint()
{
	//Checks whether host config ui is minimized or not
	if (IsIconic())
	{
		CPaintDC dc(this); 

		//send message to prepare background when hostconfig ui is minimized
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		//Get the default height and width of hostconfig icon
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);

		//Get the host config client area coordinates
		CRect rect;
		GetClientRect(&rect);

		//Calculate xy coordinates from height and width
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
		
	}
	else
	{
		//Call default 
		CDialog::OnPaint();
	}
}

HCURSOR ChostconfigwxcommonDlg::OnQueryDragIcon()
{
	//Handle case if minimized host config UI is not having icon
	return NULL;
}

