#ifndef _TAG_DETAILS_H_
#define _TAG_DETAILS_H_

#include <InmFltInterface.h>

void
PrintUnicodeStreamRecData(PSTREAM_REC_DATA_TYPE_UNICODE pData, ULONG ulMaxDataLenToParse);

void
PrintBinaryData(const unsigned char *pData, ULONG ulLen);

void
PrintUnidentifiedTag(PSTREAM_REC_HDR_4B pTag, ULONG ulMaxDataLenToParse);

#endif //_TAG_DETAILS_H_
