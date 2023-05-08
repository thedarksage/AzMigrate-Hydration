// UnLockDrivesDlg.cpp : implementation file
//

#include "stdafx.h"
#include <afxtempl.h> //added for CArray
#include "UnLockDrives.h"
#include "UnLockDrivesDlg.h"
#include ".\unlockdrivesdlg.h"
#include "globs.h"
#include "VsnapUser.h"
#include "virtualvolume.h"
#include "inmsafecapis.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <string>
using namespace std;
// CUnLockDrivesDlg dialog



CUnLockDrivesDlg::CUnLockDrivesDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CUnLockDrivesDlg::IDD, pParent)
	, m_strSelectedDrives(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_IsDisplayDialog = -1;
}

void CUnLockDrivesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_LBString(pDX, IDC_SELDRV_LIST, m_strSelectedDrives);
	DDX_Control(pDX, IDC_SELDRV_LIST, DrvSelDrives);
}

BEGIN_MESSAGE_MAP(CUnLockDrivesDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_LBN_SELCHANGE(IDC_SELDRV_LIST, OnLbnSelchangeSeldrvList)
END_MESSAGE_MAP()


// CUnLockDrivesDlg message handlers

BOOL CUnLockDrivesDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	CDPUtil::InitQuitEvent();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	char volumeStrings[MAX_PATH] = {0};	
	bool Selected = false;
	
	if(GetLogicalDriveStrings(sizeof(volumeStrings), volumeStrings)) 
	{
		char szTemp[4] = " :\\";
		char* volumeName = volumeStrings;
		
		do 
		{
			int count = 0;
			VOLUME_STATE vol_state = VOLUME_UNKNOWN;
			while(*volumeName)
			{
				*(szTemp + count) = *volumeName++;
				count++;
			}
			*(szTemp + count) = '\0';
			volumeName++;
            
			if( DRIVE_CDROM     != GetDriveType(szTemp) && 
			    DRIVE_REMOVABLE != GetDriveType(szTemp) )			   
			{
				char mountPointPath[MAX_PATH] = {0};
				char mountPoint[MAX_PATH] = {0};
				do 
				{
					HANDLE mntptHdl = FindFirstVolumeMountPoint(szTemp, mountPointPath, sizeof(mountPointPath));
					if (INVALID_HANDLE_VALUE == mntptHdl)
						break;
					inm_strcpy_s(mountPoint, ARRAYSIZE(mountPoint),szTemp);
					inm_strcat_s(mountPoint, ARRAYSIZE(mountPoint),mountPointPath);
					VOLUME_STATE vol_state_mntpt = GetVolumeState(mountPoint);
					if (  VOLUME_VISIBLE_RW  != vol_state_mntpt  && // Exclude Read/Write Visible Drives and 
						  VOLUME_UNKNOWN     != vol_state_mntpt )   // drives whose state is  unknown.
					{
						if (!IsSystemDrive(mountPoint))
						{
							if (!Selected)
								Selected = true;
							DrvSelDrives.AddString( mountPoint ); 
							DrvSelDrives.SelItemRange(FALSE, 0, DrvSelDrives.GetCount());
							OnLbnSelchangeSeldrvList();
						}
					}
					while (FindNextVolumeMountPoint(mntptHdl, mountPointPath, sizeof(mountPointPath)))
					{
						inm_strcpy_s(mountPoint,ARRAYSIZE(mountPoint), szTemp);
						inm_strcat_s(mountPoint, ARRAYSIZE(mountPoint),mountPointPath);
						VOLUME_STATE vol_state_mntpt = GetVolumeState(mountPoint);
						if (  VOLUME_VISIBLE_RW  != vol_state_mntpt  && // Exclude Read/Write Visible Drives and 
							  VOLUME_UNKNOWN     != vol_state_mntpt )   // drives whose state is  unknown.
						{
							if (!IsSystemDrive(mountPoint))
							{
								if (!Selected)
									Selected = true;
								DrvSelDrives.AddString( mountPoint ); 
								DrvSelDrives.SelItemRange(FALSE, 0, DrvSelDrives.GetCount());
								OnLbnSelchangeSeldrvList();
							}
						}
					}
					FindVolumeMountPointClose(mntptHdl);
				}
				while (false);

				szTemp[2] = '\0';
				VOLUME_STATE vol_state = GetVolumeState(szTemp);
				if (  VOLUME_VISIBLE_RW  != vol_state  && // Exclude Read/Write Visible Drives and 
					  VOLUME_UNKNOWN     != vol_state )   // drives whose state is  unknown.
				{
					if (!IsSystemDrive(szTemp))
					{
						if (!Selected)
							Selected = true;
                        DrvSelDrives.AddString( szTemp ); 
						DrvSelDrives.SelItemRange(FALSE, 0, DrvSelDrives.GetCount());
						OnLbnSelchangeSeldrvList();
					}
				}
			}
		} while (*volumeName);
	}
	
    // Handle -display commandline argument passed 
    // Required to support in the Programs->Inmage Systems menu

    CString CommandLine = GetCommandLine();
    m_IsDisplayDialog = CommandLine.Find(" -display");

	// if we are invoked as part of uninstallation process
	// (ie without m_IsDisplayDialog) option
	// we will unmount all vsnapvolumes

	if((m_IsDisplayDialog == -1))
	{
		std::vector<VsnapPersistInfo> PassedVols;
		std::vector<VsnapPersistInfo> FailedVols;
		string DisplayString = 
			"The following virtual volume(s) are unloaded from the system\n";
		int novvols = 0;
		char *token = NULL;

		VsnapMgr *Vsnap;
		try
		{
		    WinVsnapMgr vsnapshot;
		    Vsnap=&vsnapshot;
			//added second argument for fixing bug #5569
		    Vsnap->UnmountAllVirtualVolumes(PassedVols, FailedVols);

			std::vector<VsnapPersistInfo>::iterator iter = PassedVols.begin();
			for(; iter != PassedVols.end(); ++iter)
			{
				novvols++;
			    char num[8];
                inm_sprintf_s(num, ARRAYSIZE(num), "%d) ", novvols);
	    		DisplayString += "\t";
		    	DisplayString += num;
				DisplayString += iter->mountpoint;
    			DisplayString += "\n";
			}
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED to instantiate vsnap manager.\n");
        }
		// TODO: Modift  virtual volume UnmountAllVirtualVolumes
		// to return the list of volumes that are unmounted

		try
		{
		    VirVolume vvolMgr;
		    bool checkfortarget=false;
		    vvolMgr.UnmountAllVirtualVolumes(checkfortarget);
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED to instantiate VirVolume manager.\n");
        }

		if(novvols)
		{
			AfxMessageBox(DisplayString.c_str());
		}

	}
	CDPUtil::UnInitQuitEvent();
	if (!Selected && (m_IsDisplayDialog == -1) )
	{
		CDialog::OnCancel();		
	}
    else if(!Selected && (m_IsDisplayDialog != -1))
    {
  		MessageBox("There are no Target Drives to be unlocked!","UnLock Target Drives");
        CDialog::OnCancel();
    }
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CUnLockDrivesDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CUnLockDrivesDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CUnLockDrivesDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	SVERROR sve = SVS_OK;
	
	//Find the list of protected drives and release them
	if (m_strSelectedDrives.IsEmpty())
	{
		AfxMessageBox("Please select the Target Drives to be unlocked!");
	}
	else
	{
		
		string   szDelimeters   = ",\r\n";
		string pszbuff         = m_strSelectedDrives;
		int    diff             = 0;
		bool   FailedDrives     = false;
		bool   AddComma         = false;
		vector<string> tokens;
		Tokenize(pszbuff, tokens);
		
		CDialog::OnOK();
		DWORD  dwDrivesToUnlock = 0;
		string Message          = "Outpost Agent was unable to unhide the following drives\n";
		for(unsigned int i =0; i < tokens.size();i++)
		{
		// Lets convert the drives into DWORD
		
            string pszToken = tokens[i];
			if ( !pszToken.empty() && pszToken[0] != '\0' && pszToken[0] != ' ' && 
					pszToken[0] != '\n' && pszToken[0] != '\r' )
			{
				VsnapMgr *Vsnap;
				try
				{
					WinVsnapMgr vsnapshot;
					Vsnap=&vsnapshot;
					//added second argument for fixing bug #5569
					Vsnap->UnMountVsnapVolumesForTargetVolume(pszToken, true);
				}
				catch(...)
				{
					//DebugPrintf(SV_LOG_ERROR,"FAILED to instantiate vsnap manager.\n");
				}
				sve = UnhideDrive_RW(pszToken.c_str(), pszToken.c_str());
				if( sve.failed())
				{	
					FailedDrives = true ;
					
					if (AddComma)
					{
                        Message += ',';
					}
					else
					{
                       AddComma = true;
					}						 
				
					Message += pszToken ;	
				}
			
			}				
	
		}
		if ( FailedDrives )
		{				
			AfxMessageBox(Message.c_str());
		}
		m_strSelectedDrives.Empty();	
		pszbuff.empty();		
	}

}

void CUnLockDrivesDlg::Tokenize(const string& input,
                      vector<string>& outtoks,
                      const string& separators )
{
    std::string::size_type pp, cp;

    pp = input.find_first_not_of(separators, 0);
    cp = input.find_first_of(separators, pp);
    while ((std::string::npos != cp) || (std::string::npos != pp)) {
        outtoks.push_back(input.substr(pp, cp-pp));
        pp = input.find_first_not_of(separators, cp);
        cp = input.find_first_of(separators, pp);
    }
}


void CUnLockDrivesDlg::OnLbnSelchangeSeldrvList()
{
	// TODO: Add your control notification handler code here

	int count = DrvSelDrives.GetSelCount();
	CArray< int,int > arrayListSel;
    arrayListSel.SetSize(count);								// make room in array
    DrvSelDrives.GetSelItems(count, arrayListSel.GetData());    // copy data to array
    CString str = _T("");
	
	for( int i=0; i< count; i++ )
    {
        CString tmp = _T("");
        DrvSelDrives.GetText( arrayListSel[i], tmp );
		LPCTSTR lpszNewString = (LPCTSTR)tmp;
		SetNewHExtent(lpszNewString);
        str += (tmp + _T(","));
    }
	
	m_strSelectedDrives = str;	

}
void CUnLockDrivesDlg::SetNewHExtent(LPCTSTR lpszNewString)
{
	int iExt = GetTextLen(lpszNewString);
	if (iExt > DrvSelDrives.GetHorizontalExtent())
		DrvSelDrives.SetHorizontalExtent(iExt);
}

int CUnLockDrivesDlg::GetTextLen(LPCTSTR lpszText)
{
  ASSERT(AfxIsValidString(lpszText));

  CDC *pDC = GetDC();
  ASSERT(pDC);

  CSize size;
  CFont* pOldFont = pDC->SelectObject(GetFont());
  if ((GetStyle() & LBS_USETABSTOPS) == 0)
  {
    size = pDC->GetTextExtent(lpszText, (int) _tcslen(lpszText));
    size.cx += 3;
  }
  else
  {
    // Expand tabs as well
    size = pDC->GetTabbedTextExtent(lpszText, (int)
          _tcslen(lpszText), 0, NULL);
    size.cx += 2;
  }
  pDC->SelectObject(pOldFont);
  ReleaseDC(pDC);

  return size.cx;
}
