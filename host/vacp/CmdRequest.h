#ifndef _INMAGE_CMD_REQUEST__H
#define _INMAGE_CMD_REQUEST__H

#pragma once 

#include "stdafx.h"
#include "util.h"
#include "common.h"

class ACSRequestGenerator
{
public:
    ~ACSRequestGenerator()
    {
        if (vrPtr.get() != NULL)
            vrPtr->Reset();
        vrPtr.reset();
    }

    InMageVssRequestorPtr GetVssRequestorPtr()
    {
        return vrPtr;
    }

    HRESULT GenerateACSRequest(CLIRequest_t &CLIRequest, ACSRequest_t &Request, TAG_TYPE TagType);
    HRESULT GenerateTagIdentifiers(ACSRequest_t &Request);

private:

	HRESULT ValidateVolume(const char *volumeName);
	HRESULT ValidateApplication(const char *appName);
    HRESULT FillACSRequest(CLIRequest_t &CLIRequest, ACSRequest_t &Request, TAG_TYPE TagType);
	HRESULT BuildTagIdentifiers(ACSRequest_t &Request, 
								USHORT TagId, 
								const char *TagName, 
								GUID commonTagGuid);
  HRESULT BuildRevocationTagIdentifiers(ACSRequest_t &Request, USHORT TagId, const char *TagName);
	HRESULT BuildRevocationTag(ACSRequest_t &Request);
	HRESULT ValidateCLIRequest(CLIRequest_t &CLIRequest);
	bool IsVolMountAlreadyExist(VOLMOUNTPOINT_INFO& targetVolMP_info,VOLMOUNTPOINT& addVolMP);
    HRESULT FillSharedDiskTagInfo(std::string& clusTag, bool isDistributed, TAG_TYPE TagType);

private:

    InMageVssRequestorPtr vrPtr;
};

#endif /* _INMAGE_CMD_REQUEST_H */

