#ifndef _DRVUTIL_SVD_STREAM_H_
#define _DRVUTIL_SVD_STREAM_H_

#include <windows.h>
#include <string>
#include "InmFltInterface.h"
#include "svdparse.h"
#include "SegStream.h"
#include "VFltCmdsExec.h"
 
bool
PrintSVDstreamInfoV2(PGET_DB_TRANS_DATA  pDBTranData, PUDIRTY_BLOCK_V2 DirtyBlock);

bool
PrintSVDstreamInfo(PGET_DB_TRANS_DATA  pDBTranData, PUDIRTY_BLOCK DirtyBlock);

bool
PrintSVDSegStreamInfoV2(PGET_DB_TRANS_DATA  pDBTranData, PUDIRTY_BLOCK_V2 DirtyBlock, CDataStream &DataStream);

bool
PrintSVDSegStreamInfo(PGET_DB_TRANS_DATA  pDBTranData, PUDIRTY_BLOCK DirtyBlock, CDataStream &DataStream);

bool
ValidateSVDheaderV2(SVD_PREFIX &svdPrefix, SVD_HEADER_V2 &svdHeader1);

bool
ValidateSVDheader(SVD_PREFIX &svdPrefix, SVD_HEADER1 &svdHeader1);

bool
ValidateSVDPrefix(SVD_PREFIX &svdPrefix, ULONG ulTag, ULONG ulFlags, char *StructName, char *TagName);

bool
ValidateSVDtimeStampOfFirstChangeV2(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP_V2 &svdTimeStamp);

bool
ValidateSVDtimeStampOfFirstChange(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP &svdTimeStamp);

bool
ValidateSVDtimeStampOfLastChangeV2(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP_V2 &svdTimeStamp);

bool
ValidateSVDtimeStampOfLastChange(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP &svdTimeStamp);

bool
ValidateStreamRecTypeTimeStampV2(SVD_TIME_STAMP_V2 &svdTimeStamp);

bool
ValidateStreamRecTypeTimeStamp(SVD_TIME_STAMP &svdTimeStamp);

void
PrintTimeStampV2(ULONGLONG TimeInHundNanoSecondsFromJan1601, ULONGLONG ulSequenceNumber, char *Description);

void
PrintTimeStamp(ULONGLONG TimeInHundNanoSecondsFromJan1601, ULONG ulSequenceNumber, char *Description);

class SVDtag {
public:
    SVDtag(ULONG ulTag);
    ~SVDtag();

    const char *c_str() {return m_sTag.c_str();}

private:
    ULONG   m_ulTag;
    std::string m_sTag;
};

#endif _DRVUTIL_SVD_STREAM_H_

