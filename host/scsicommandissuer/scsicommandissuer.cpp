#include <cstring>
#include "scsicommandissuer.h"
#include "inmsafecapis.h"


bool ScsiCommandIssuer::StartSession(const std::string &device)
{
    bool bstarted = m_PlatformBasedScsiCommandIssuer.StartSession(device);

    if (!bstarted)
    {
        m_ErrMsg = m_PlatformBasedScsiCommandIssuer.GetErrorMessage();
    }

    return bstarted;
}


void ScsiCommandIssuer::EndSession(void)
{
    m_PlatformBasedScsiCommandIssuer.EndSession();
}


std::string ScsiCommandIssuer::GetErrorMessage(void)
{
    return m_ErrMsg;
}


bool ScsiCommandIssuer::Issue(ScsiCmd_t *scmd)
{
    bool bissued = false;
    bool shouldinspectstatus;
    bool bissuedplatformbasedcommand = m_PlatformBasedScsiCommandIssuer.Issue(scmd, shouldinspectstatus);
    if (bissuedplatformbasedcommand)
    {
        bissued = shouldinspectstatus ? IsScsiCmdSuccess(scmd->m_ScsiStatus, scmd->m_Sense, scmd->m_RealSenseLen) : true;
    }
    else
    {
        m_ErrMsg = m_PlatformBasedScsiCommandIssuer.GetErrorMessage();
    }

    return bissued;
}


bool ScsiCommandIssuer::DoInquiryCmd(INQUIRY_DETAILS *pInqDetails, const unsigned char &evpd,
                                     const unsigned char &pagecode, unsigned char *pinqdata,
                                     const unsigned int &pinqdatalen)
{
    bool bretval = false;
    unsigned char sense[SENSELEN];
    unsigned char inqcmd[INQCMDLEN] = {INQCMD, evpd, pagecode, 0, pinqdatalen, 0};

    if (pinqdatalen <= INQLEN) 
    {
        ScsiCmd_t scmd;
        scmd.setinput(pInqDetails->device.c_str(), 
                      inqcmd, sizeof inqcmd, 
                      pinqdata, pinqdatalen,
                      sense, sizeof sense,
                      DEF_TIMEOUT);
        bretval = Issue(&scmd);
    }
    else
    {
        m_ErrMsg = "inquiry buffer length is greater than maximum allowed";
    }

    return bretval;
}


bool ScsiCommandIssuer::GetStdInqValues(INQUIRY_DETAILS *pInqDetails)
{
    unsigned char stdinqdata[INQLEN] = "\0";
    bool bgot = DoInquiryCmd(pInqDetails, 0, 0, stdinqdata, sizeof stdinqdata);

    if (bgot)
    {        
		inm_memcpy_s(pInqDetails->m_Vendor, sizeof(pInqDetails->m_Vendor), stdinqdata + VENDOROFFSET, VENDORLEN);
        pInqDetails->m_Vendor[VENDORLEN] = '\0';     
		inm_memcpy_s(pInqDetails->m_Product, sizeof(pInqDetails->m_Product), stdinqdata + VENDOROFFSET + VENDORLEN, PRODUCTLEN);
        pInqDetails->m_Product[PRODUCTLEN] = '\0';
    }

    return bgot;
}


bool ScsiCommandIssuer::IsScsiCmdSuccess(const int &scsistatus, const unsigned char *sensebuf, const int &sensebuflen)
{
    bool bissuccess = false;

    if (0 == scsistatus)
    {
        bissuccess = true;
    }
    else if (INMCHECK_CONDITION == scsistatus)
    {
        if ((NULL != sensebuf) && (sensebuflen >= 3)) 
        {
            /* 7th bit is not included in response code of
             * sense data for both fixed and descriptor format */
            unsigned char resp = (sensebuf[0] & 0x7f);
            int sensekey = 0;
            bool bcheckkey = true;

            switch (resp)
            {
                case 0x72:
                case 0x73:
                    /* descriptor format sense data: 0x72 or 0x73 */
                    sensekey = (sensebuf[1] & 0xf);
                    break;
                case 0x70:
                case 0x71:
                    /* fixed format sense data: 0x70 or 0x71; */
                    sensekey = (sensebuf[2] & 0xf);
                    break;
                default:
                    bcheckkey = false;
                    break;
            }
            
            if (bcheckkey && (RECOVERABLE_ERROR == sensekey))
            {
                bissuccess = true;
            }
        }
    }

    return bissuccess;
}
