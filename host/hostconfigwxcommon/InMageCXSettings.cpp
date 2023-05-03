#include "stdafx.h"
#include "hostconfigwxcommon.h"
#include "InMageCXSettings.h"
#include "LocalConfigurator.h"
#include "inmageex.h"
#include "globs.h"
#include "csgetfingerprint.h"
using namespace std;

IMPLEMENT_DYNAMIC(CInMageCXSettings, CPropertyPage)

CInMageCXSettings::CInMageCXSettings()
: CPropertyPage(CInMageCXSettings::IDD)
, m_stEnableHTTPS(FALSE)
, m_stBtnNATHostname(FALSE)
, m_NatHostname(_T(""))
, m_stBtnNatIP(FALSE)
, m_cxPort(_T(""))
, m_passphraseVal(_T(""))
{
    m_bChangeAttempted = false;
    m_bVerifyPassphraseOnly = false;
}

CInMageCXSettings::~CInMageCXSettings()
{
}

void CInMageCXSettings::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_CX_IPADDRESS, m_CxIPAddress);
	DDV_MaxChars(pDX, m_CxIPAddress, 255);
    DDX_Check(pDX, IDC_CHECK_ENABLE_HTTPS, m_stEnableHTTPS);
    DDX_Check(pDX, IDC_CHECK_NAT_HOSTNAME, m_stBtnNATHostname);
    DDX_Text(pDX, IDC_EDIT_NAT_HOSTNAME, m_NatHostname);
    DDV_MaxChars(pDX, m_NatHostname, 15);
    DDX_Check(pDX, IDC_CHECK_NAT_IP, m_stBtnNatIP);
    DDX_Control(pDX, IDC_NAT_CX_IPADDRESS, m_NatIPAddress);
    DDX_Control(pDX, IDC_STATIC_INITIALIZING, m_staticInitialize);
    DDX_Control(pDX, IDC_STATIC_IN_PROGRESS, m_staticInProgress);
    DDX_Text(pDX, IDC_EDIT_CX_PORT, m_cxPort);
    DDV_MaxChars(pDX, m_cxPort, 5);
    DDX_Control(pDX, IDC_EDIT_CX_PORT, m_cxPortEditCtrl);
    DDX_Control(pDX, IDC_ASTERIC, mandatory_asteric);
    DDX_Control(pDX, IDC_EDIT_PASSPHRASE, m_passphraseCtrl);
    DDX_Text(pDX, IDC_EDIT_PASSPHRASE, m_passphraseVal);
}


BEGIN_MESSAGE_MAP(CInMageCXSettings, CPropertyPage)
	ON_EN_CHANGE(IDC_CX_IPADDRESS, &CInMageCXSettings::OnIpnFieldchangedCxIpaddress)
    ON_EN_CHANGE(IDC_EDIT_CX_PORT, &CInMageCXSettings::OnEnChangeEditCxPort)
    ON_BN_CLICKED(IDC_CHECK_ENABLE_HTTPS, &CInMageCXSettings::OnBnClickedCheckEnableHttps)
    ON_BN_CLICKED(IDC_CHECK_NAT_HOSTNAME, &CInMageCXSettings::OnBnClickedCheckNatHostname)
    ON_EN_CHANGE(IDC_EDIT_NAT_HOSTNAME, &CInMageCXSettings::OnEnChangeEditNatHostname)
    ON_BN_CLICKED(IDC_CHECK_NAT_IP, &CInMageCXSettings::OnBnClickedCheckNatIp)
    ON_NOTIFY(IPN_FIELDCHANGED, IDC_NAT_CX_IPADDRESS, &CInMageCXSettings::OnIpnFieldchangedNatCxIpaddress)
    ON_WM_CTLCOLOR()
    ON_STN_CLICKED(IDC_STATIC_MODIFYPASSPHRASE, &CInMageCXSettings::OnStnClickedStaticModifypassphrase)
    ON_EN_CHANGE(IDC_EDIT_PASSPHRASE, &CInMageCXSettings::OnEnChangeEditPassphrase)
END_MESSAGE_MAP()

/********************************************
* Event handler for cx ip address change    *
* Enable Apply button if change attempted   *
********************************************/
void CInMageCXSettings::OnIpnFieldchangedCxIpaddress()
{
    CString modifiedCxIp;
	GetDlgItem(IDC_CX_IPADDRESS)->GetWindowText(modifiedCxIp);
	if (modifiedCxIp.Compare(CInmConfigData::GetInmConfigInstance().GetCxSettings().cxIP) != 0)
    {
        m_bChangeAttempted = true;
        SetModified();
    }
}

/********************************************
* Event handler for cx port number change   *
* Enable Apply button if change attempted   *
********************************************/
void CInMageCXSettings::OnEnChangeEditCxPort()
{
    CString modifiedCxPort;
    GetDlgItem(IDC_EDIT_CX_PORT)->GetWindowText(modifiedCxPort);
    if (modifiedCxPort.CompareNoCase(CInmConfigData::GetInmConfigInstance().GetCxSettings().inmCxPort) != 0)
    {
        m_bChangeAttempted = true;
        SetModified();
    }
}

/********************************************
* Event handler for cx ip address change    *
* Enable Apply button if change attempted   *
********************************************/
void CInMageCXSettings::OnBnClickedCheckEnableHttps()
{
    BOOL httpsCheckState = ((CButton*)GetDlgItem(IDC_CHECK_ENABLE_HTTPS))->GetCheck();
    if (httpsCheckState != m_stEnableHTTPS)
    {
        m_bChangeAttempted = true;
        SetModified();
    }

    if ((m_stEnableHTTPS && httpsCheckState) || ((m_stEnableHTTPS == BST_UNCHECKED) && (httpsCheckState == BST_UNCHECKED)))
    {
        if (CInmConfigData::GetInmConfigInstance().scoutAgentInstallCode == SCOUT_FX)
        {
            HKEY svSysKey;
            DWORD cxSettingsDwordValue;
            LSTATUS svSysRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, INM_SVSYSTEMS_REGKEY, 0, KEY_READ, &svSysKey);

            if (svSysRet == ERROR_SUCCESS)
            {
                //Read ServerHttpPort
                DWORD cxSettingsValueSize = sizeof(DWORD);
                DWORD cxSettingsValueType = REG_DWORD;
                svSysRet = RegQueryValueEx(svSysKey, SV_SERVER_HTTP_PORT_VALUE_NAME, 0, &cxSettingsValueType, (LPBYTE)&cxSettingsDwordValue, &cxSettingsValueSize);
                if (svSysRet == ERROR_SUCCESS)
                {
                    char cxPort[6] = {};
                    _ultoa(cxSettingsValueType, cxPort, 10);
                    m_cxPortEditCtrl.SetWindowText((LPCTSTR)cxPort);
                }
                RegCloseKey(svSysKey);
            }
        }
        else
        {
            try
            {
                LocalConfigurator localConfig;
                HTTP_CONNECTION_SETTINGS httpConSettings = localConfig.getHttp();
                char cxPort[6] = {};
                _itoa(httpConSettings.port, cxPort, 10);
                m_cxPortEditCtrl.SetWindowText((LPCTSTR)cxPort);
            }
            catch (ContextualException& cexp)
            {
                DebugPrintf(SV_LOG_ERROR, "Caught contextual exception = %s\n", cexp.what());
            }
            catch (std::exception const& exp)
            {
                DebugPrintf(SV_LOG_ERROR, "Caught exception = %s\n", exp.what());
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "Encountered unknown exception.\n");
            }
        }
    }
    else if (m_stEnableHTTPS && (httpsCheckState == BST_UNCHECKED))
    {
        m_cxPortEditCtrl.SetWindowText("80");
    }
    else if ((m_stEnableHTTPS == BST_UNCHECKED) && (httpsCheckState == BST_CHECKED))
    {
        m_cxPortEditCtrl.SetWindowText("443");
    }
}

/************************************************
* Event handler for NAT host name check status  *
* Enable Apply button if change attempte        *
************************************************/
void CInMageCXSettings::OnBnClickedCheckNatHostname()
{
    int natHostCheckState = ((CButton*)GetDlgItem(IDC_CHECK_NAT_HOSTNAME))->GetCheck();
    GetDlgItem(IDC_EDIT_NAT_HOSTNAME)->EnableWindow(natHostCheckState == BST_CHECKED);
    if (natHostCheckState != m_stBtnNATHostname)
    {
        m_bChangeAttempted = true;
        SetModified();
    }
}

/********************************************
* Event handler for NAT host name change    *
* Enable Apply button if change attempted   *
********************************************/
void CInMageCXSettings::OnEnChangeEditNatHostname()
{
    CString modifiedNatHostName;
    GetDlgItem(IDC_EDIT_NAT_HOSTNAME)->GetWindowText(modifiedNatHostName);
    if (modifiedNatHostName.CompareNoCase(CInmConfigData::GetInmConfigInstance().GetCxSettings().cx_NatHostName))
    {
        m_bChangeAttempted = true;
        SetModified();
    }
}

/********************************************
* Event handler for NAT Ip check status     *
* Enable Apply button if change attempted   *
********************************************/
void CInMageCXSettings::OnBnClickedCheckNatIp()
{
    int natIpCheckState = ((CButton*)GetDlgItem(IDC_CHECK_NAT_IP))->GetCheck();
    GetDlgItem(IDC_NAT_CX_IPADDRESS)->EnableWindow(natIpCheckState == BST_CHECKED);
    if (natIpCheckState != m_stBtnNatIP)
    {
        m_bChangeAttempted = true;
        SetModified();
    }
}

/********************************************
* Event handler for NAT Ip change           *
* Enable Apply button if change attempted   *
********************************************/
void CInMageCXSettings::OnIpnFieldchangedNatCxIpaddress(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
    CString modifiedCxIp;
    CIPAddressCtrl* pIPCtrl = (CIPAddressCtrl*)GetDlgItem(IDC_NAT_CX_IPADDRESS);
    pIPCtrl->GetWindowText(modifiedCxIp);
    if (modifiedCxIp.CompareNoCase(CInmConfigData::GetInmConfigInstance().GetCxSettings().cx_NatIP) != 0)
    {
        m_bChangeAttempted = true;
        SetModified();
    }
    *pResult = 0;
}

/********************************************************************
* Checks whether the config data is really changed or not          *
********************************************************************/
bool CInMageCXSettings::ValidateAgentConfigChange()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    if (!m_bChangeAttempted)
    {
        const CX_SETTINGS& cxSettings = CInmConfigData::GetInmConfigInstance().GetCxSettings();
        CString modifiedCxIp;
        CString modifiedCxNatIp;
		modifiedCxIp = m_CxIPAddress;
        m_NatIPAddress.GetWindowText(modifiedCxNatIp);
        if ((cxSettings.cxIP.Compare(modifiedCxIp) == 0) &&
            (cxSettings.cx_NatIP.CompareNoCase(modifiedCxNatIp) == 0) &&
            (cxSettings.bCxUseHttps == (m_stEnableHTTPS == BST_CHECKED)) &&
            (cxSettings.inmCxPort.CompareNoCase(m_cxPort) == 0) &&
            (cxSettings.cx_NatHostName.CompareNoCase(m_NatHostname) == 0) &&
            (cxSettings.bCxNatHostEnabled == (m_stBtnNATHostname == BST_CHECKED)) &&
            (cxSettings.bCxNatIpEnabled == (m_stBtnNatIP == BST_CHECKED)))
        {
            DebugPrintf(SV_LOG_DEBUG, "No change attempted in Global tab.\n");
            m_bChangeAttempted = false;
        }
        else
        {
            m_bChangeAttempted = true;
            DebugPrintf(SV_LOG_DEBUG, "Change attempted in Global tab.\n");
            DebugPrintf(SV_LOG_DEBUG, "Old IP address = %s.\n", cxSettings.cxIP.GetString());
            DebugPrintf(SV_LOG_DEBUG, "New IP address = %s.\n", modifiedCxIp.GetString());
            DebugPrintf(SV_LOG_DEBUG, "Old Port number = %s.\n", cxSettings.inmCxPort.GetString());
            DebugPrintf(SV_LOG_DEBUG, "New Port number = %s.\n", m_cxPort.GetString());
            DebugPrintf(SV_LOG_DEBUG, "Old NAT hostname = %s.\n", cxSettings.cx_NatHostName.GetString());
            DebugPrintf(SV_LOG_DEBUG, "New NAT hostname = %s.\n", m_NatHostname.GetString());
            DebugPrintf(SV_LOG_DEBUG, "Old NAT IP = %s.\n", cxSettings.cx_NatIP.GetString());
            DebugPrintf(SV_LOG_DEBUG, "New NAT IP = %s.\n", modifiedCxNatIp.GetString());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return m_bChangeAttempted;
}

/********************************************************************
* Do config data exchange from persisted data to control variables  *
********************************************************************/
void CInMageCXSettings::SyncAgentConfigData()
{
    const CX_SETTINGS& cxSettings = CInmConfigData::GetInmConfigInstance().GetCxSettings();

    m_cxPort = cxSettings.inmCxPort;

	m_CxIPAddress = cxSettings.cxIP;

    m_stEnableHTTPS = cxSettings.bCxUseHttps ? BST_CHECKED : BST_UNCHECKED;

    if (cxSettings.cx_NatIP.CompareNoCase("0") == 0)
        m_NatIPAddress.SetWindowText("0.0.0.0");
    else
        m_NatIPAddress.SetWindowText(cxSettings.cx_NatIP);
    m_stBtnNatIP = cxSettings.bCxNatIpEnabled ? BST_CHECKED : BST_UNCHECKED;
    GetDlgItem(IDC_NAT_CX_IPADDRESS)->EnableWindow(m_stBtnNatIP);

    m_NatHostname = cxSettings.cx_NatHostName;
    m_stBtnNATHostname = cxSettings.bCxNatHostEnabled ? BST_CHECKED : BST_UNCHECKED;
    GetDlgItem(IDC_EDIT_NAT_HOSTNAME)->EnableWindow(m_stBtnNATHostname);
    if (CInmConfigData::GetInmConfigInstance().m_bEnableHttps)
    {
        m_cxPort = _T("443");
        m_stEnableHTTPS = BST_CHECKED;
    }
    m_passphraseVal = cxSettings.cx_Passphrase;
}

/************************************************************************************
* Do config data exchange from control variable to structure to persist the data    *
************************************************************************************/
BOOL CInMageCXSettings::SyncAgentConfigData(CX_SETTINGS &cxSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    int cxPort_t = atoi(m_cxPort.GetString());
    if ((cxPort_t < 1) || (cxPort_t > 65535))
    {
        DebugPrintf(SV_LOG_ERROR, "Invalid port number = %d\n", cxPort_t);
        AfxMessageBox("Port number should range from 1 to 65535", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    if (m_stBtnNATHostname ? true : false)
    {
        if (m_NatHostname.IsEmpty())
        {
            DebugPrintf(SV_LOG_ERROR, "NAT host name cannot be empty.\n");
            AfxMessageBox("NAT host name cannot be empty.", MB_OK | MB_ICONERROR);
            return FALSE;
        }
        else if (m_NatHostname.GetLength() > 15)
        {
            DebugPrintf(SV_LOG_ERROR, "NAT host name length cannot exceed 15 characters.\n");
            AfxMessageBox("NAT host name length cannot exceed 15 characters.", MB_OK | MB_ICONERROR);
            return FALSE;
        }
    }

    
	cxSettings.cxIP = m_CxIPAddress;
    m_NatIPAddress.GetWindowText(cxSettings.cx_NatIP);
    cxSettings.inmCxPort = m_cxPort;
    cxSettings.cx_NatHostName = m_NatHostname;
    cxSettings.bCxUseHttps = m_stEnableHTTPS ? true : false;
    cxSettings.bCxNatHostEnabled = m_stBtnNATHostname ? true : false;
    cxSettings.bCxNatIpEnabled = m_stBtnNatIP ? true : false;
    cxSettings.cx_Passphrase = m_passphraseVal;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return TRUE;
}

/********************************************************************
* Update and validate the config data when tab/page lose focus      *
********************************************************************/
BOOL CInMageCXSettings::OnKillActive()
{
    UpdateData();
    ValidateAgentConfigChange();
    if (!m_bChangeAttempted)
        SetModified(FALSE);
    return CPropertyPage::OnKillActive();
}

/****************************************************************
* This function gets executed when Global tab is initialized    *
*****************************************************************/
BOOL CInMageCXSettings::OnInitDialog()
{
    BOOL bInitCXSettings = CPropertyPage::OnInitDialog();

    if (securitylib::isPassphraseFileExists())
    {
        GetDlgItem(IDC_STATIC_PASSPHRASE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_ASTERIC5)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_EDIT_PASSPHRASE)->ShowWindow(SW_HIDE);
        UnderlineText(IDC_STATIC_MODIFYPASSPHRASE);
        GetDlgItem(IDC_STATIC_MODIFYPASSPHRASE)->ShowWindow(SW_SHOW);
    }
    else
    {
        GetDlgItem(IDC_STATIC_PASSPHRASE)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_ASTERIC5)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_EDIT_PASSPHRASE)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_STATIC_MODIFYPASSPHRASE)->ShowWindow(SW_HIDE);
    }
    return bInitCXSettings;
}

bool CInMageCXSettings::IsAgentConfigChanged()
{
    //ValidateAgentConfigChange();
    return m_bChangeAttempted;
}

void CInMageCXSettings::ResetConfigChangeStatus(bool bStatus)
{
    m_bChangeAttempted = bStatus;
    m_bValidatePassphrase = bStatus;
}

HBRUSH CInMageCXSettings::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbrushHandle = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    if ((pWnd->GetDlgCtrlID() == IDC_ASTERIC) || (pWnd->GetDlgCtrlID() == IDC_ASTERIC2) || (pWnd->GetDlgCtrlID() == IDC_ASTERIC3) || (pWnd->GetDlgCtrlID() == IDC_ASTERIC5))
    {
        pDC->SetTextColor(RGB(127, 0, 0));
    }
    if (pWnd->GetDlgCtrlID() == IDC_STATIC_MODIFYPASSPHRASE)
    {
        pDC->SetTextColor(RGB(30, 144, 255)); //dodger blue
    }
    return hbrushHandle;
}

void CInMageCXSettings::UnderlineText(int dlgItem)
{
    CWnd *dlgHandle = GetDlgItem(dlgItem);
    CFont *fontHandle = dlgHandle->GetFont();
    LOGFONT logFont;
    fontHandle->GetLogFont(&logFont);
    logFont.lfWeight = FW_THIN;
    logFont.lfUnderline = TRUE;
    m_cFont.CreateFontIndirect(&logFont);
    dlgHandle->SetFont(&m_cFont);
}

void CInMageCXSettings::OnStnClickedStaticModifypassphrase()
{
    try
    {
        GetDlgItem(IDC_STATIC_PASSPHRASE)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_ASTERIC5)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_EDIT_PASSPHRASE)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_STATIC_MODIFYPASSPHRASE)->ShowWindow(SW_HIDE);
        string passphrase = securitylib::getPassphrase();
        m_passphraseCtrl.SetWindowText(passphrase.c_str());
        m_bVerifyPassphraseOnly = true;
    }
    catch (std::exception const& exp)
    {
        DebugPrintf(SV_LOG_ERROR, "Caught exception = %s\n", exp.what());
        AfxMessageBox(exp.what(), MB_OK | MB_ICONERROR);
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "Encountered unknown exception.\n");
        AfxMessageBox("Encountered unknown exception.", MB_OK | MB_ICONERROR);
    }
}

void CInMageCXSettings::OnEnChangeEditPassphrase()
{
    CString modifiedPassphrase;
    m_passphraseCtrl.GetWindowText(modifiedPassphrase);
    if (modifiedPassphrase.CompareNoCase(CInmConfigData::GetInmConfigInstance().GetCxSettings().cx_Passphrase))
    {
        m_bChangeAttempted = true;
        SetModified();
    }
}

