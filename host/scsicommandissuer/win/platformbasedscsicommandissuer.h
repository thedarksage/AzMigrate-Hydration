#ifndef PLATFORM_BASED_SCSI_COMMAND_ISSUER_H
#define PLATFORM_BASED_SCSI_COMMAND_ISSUER_H


#include <string>

#include <windows.h>
#include <ntddscsi.h>

#include "scsicmd.h"
#include "sharedalignedbuffer.h"

class PlatformBasedScsiCommandIssuer
{
public:
    PlatformBasedScsiCommandIssuer();
    ~PlatformBasedScsiCommandIssuer();
    bool Issue(ScsiCmd_t *scmd, bool &shouldinspectscsistatus);
    bool StartSession(const std::string &device);
    void EndSession(void);
    std::string GetErrorMessage(void);

private:
	std::string GetNameToIssueScsiCommand(const std::string &device);
	bool AllocateXferBufferIfNeeded(const size_t &inputxferbuflen);
	bool GetXferBufferAlignment(int *palignment);

	/* structure referrred from C:\WinDDK\6001.18000\src\storage\tools\spti\spti.h
     * The reason direct pass through is being used, is to allow scsi driver to directly
     * update user space buffer. Refer spti.htm for this */
    typedef struct _SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER {
        SCSI_PASS_THROUGH_DIRECT sptd;
        ULONG Filler;      // realign buffer to double word boundary
        UCHAR ucSenseBuf[SENSELEN];
    } SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

private:
    std::string m_Device;
    HANDLE m_Handle;
	SharedAlignedBuffer m_Xfer;
    std::string m_ErrMsg;
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER m_ScsiPassThruDirectInput;
};

#endif /* PLATFORM_BASED_SCSI_COMMAND_ISSUER_H */
