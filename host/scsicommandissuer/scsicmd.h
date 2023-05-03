#ifndef _SCSI__CMD__H_
#define _SCSI__CMD__H_

#include <cstddef>
#include <string>
#include <cstring>

#include "platformscsicmd.h"

#define TYPICALMODEPAGELEN 252
#define MODESENSE6CODE      0x1a
#define LENMODESENSE6CMD   6
#define MODESENSE10CODE     0x5a
#define LENMODESENSE10CMD  10
#define SENSELEN  32
#define SECTOMSECFACTOR 1000
/*
 * Default 5 second timeout
 */
#define DEF_TIMEOUT 5

#define INMCHECK_CONDITION 0x2
#define TYPICALMODEDATALEN 1024
#define MODESENSE10HDRLEN 8
#define MODESENSE6HDRLEN 4

/* generic scsi command structure */
struct ScsiCmd
{
    const char *m_TgtDev;        /* scsi target */
    unsigned char *m_Cmd;        /* scsi command */
    size_t m_CmdLen;             /* length of command */
    unsigned char *m_Xfer;       /* xfer buffer */
    size_t m_XferLen;            /* xfer buffer length */
    unsigned char *m_Sense;      /* sense buffer */
    size_t m_SenseLen;           /* sense buffer length */
    size_t m_RealXferLen;        /* real number of bytes xferred */
    size_t m_RealSenseLen;       /* real number of sense bytes sent */
    unsigned int m_TimeOut;      /* time out in seconds */
    int m_ScsiStatus;            /* scsi status to inspect */

    ScsiCmd()
    {
        m_TgtDev = NULL;
        m_Cmd = NULL;
        m_CmdLen = 0;
        m_Xfer = NULL;
        m_XferLen = 0;
        m_Sense = NULL;
        m_SenseLen = 0;
        m_RealXferLen = 0;
        m_RealSenseLen = 0;
        m_TimeOut = DEF_TIMEOUT;
        m_ScsiStatus = 0;
    }

    void setinput(const char *tgtdev, 
             unsigned char *cmd, const size_t cmdlen, 
             unsigned char *xfer, const size_t xferlen,
             unsigned char *sense, const size_t senselen,
             const unsigned int timeout)
    {
        m_TgtDev = tgtdev;
        m_Cmd = cmd;
        m_CmdLen = cmdlen;
        m_Xfer = xfer;
        m_XferLen = xferlen;
        m_Sense = sense;
        m_SenseLen = senselen;
        m_TimeOut = timeout;
    }
};
typedef struct ScsiCmd ScsiCmd_t;


#define INQCMDLEN 6
#define INQCMD 0x12
#define VENDORLEN 8
#define PRODUCTLEN 16
#define IDLENMAX 256
#define VENDOROFFSET 8
#define INQLEN   254

typedef struct InqDetails
{
    char m_Vendor[VENDORLEN + 1];
    char m_Product[PRODUCTLEN + 1];
    char m_Id[IDLENMAX];
    std::string device;

    InqDetails()
    {
        memset(m_Vendor, 0, sizeof m_Vendor);
        memset(m_Product, 0, sizeof m_Product);
        memset(m_Id, 0, sizeof m_Id);
    }

    std::string GetVendor(void)
    {
        return m_Vendor;
    }

    std::string GetProduct(void)
    {
        return m_Product;
    }

    std::string GetID(void)
    {
        return m_Id;
    }

} INQUIRY_DETAILS;

/* TODO: somewhere this is InMageDR instead of just InMage ? */
/* TODO: should this be from drscout.conf or CX settings ? */
#define INMATLUNVENDOR "InMage"

#endif /* _SCSI__CMD__H_ */
