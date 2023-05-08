#ifndef _SVD_STREAM_H_
#define _SVD_STREAM_H_

#include "ReplicationManager.h"
#include <string>

void ValidateSVDtimeStampOfFirstChangeV2(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP_V2 &svdTimeStamp);
void ValidateSVDtimeStampOfFirstChange(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP &svdTimeStamp);
void ValidateSVDtimeStampOfLastChangeV2(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP_V2 &svdTimeStamp);
void ValidateSVDtimeStampOfLastChange(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP &svdTimeStamp);
void ValidateStreamRecTypeTimeStampV2(SVD_TIME_STAMP_V2 &svdTimeStamp);
void ValidateStreamRecTypeTimeStamp(SVD_TIME_STAMP &svdTimeStamp);
void ValidateSVDheaderV2(SVD_PREFIX &svdPrefix, SVD_HEADER_V2 &svdHeader1);
void ValidateSVDheader(SVD_PREFIX &svdPrefix, SVD_HEADER1 &svdHeader1);
void ValidateSVDPrefix(SVD_PREFIX &svdPrefix, SV_ULONG ulTag, SV_ULONG ulFlags, char *StructName, char *TagName);

class SVDtag 
{
public:
	SVDtag(SV_ULONG ulTag);
	~SVDtag();

	const char *c_str() { return m_sTag.c_str(); }

private:
	SV_ULONG   m_ulTag;
	std::string m_sTag;
};

#endif