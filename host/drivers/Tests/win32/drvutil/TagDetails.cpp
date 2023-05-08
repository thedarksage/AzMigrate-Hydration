#include <windows.h>
#include <stdio.h>
#include "TagDetails.h"

void
PrintUnicodeStreamRecData(PSTREAM_REC_DATA_TYPE_UNICODE pData, ULONG ulMaxDataLenToParse)
{
    if (ulMaxDataLenToParse < (sizeof(USHORT) + pData->usLen)) {
        _tprintf(_T("Error: PrintUnicodeStreamRecData found data to parse(%#x) is less than sum of len size(%#x) and strlen(%#x)\n"),
                 ulMaxDataLenToParse, sizeof(USHORT), pData->usLen);
    }
    wprintf(L"Unicode string size = %#hx value = %s\n", pData->usLen, pData->wString);
    return;
}

void
PrintBinaryData(const unsigned char *pData, ULONG ulLen)
{
    const unsigned long ulCharsInOneLine = 10;
    ULONG   ulBytesPrinted = 0;

    for (ULONG i = 0; i < ulLen; i++, pData++) {
        if (isprint(*pData)) {
            printf("%#02x(%c) ", *pData,  *pData);
        } else {
            printf("%#02x(.) ", *pData);
        }
        ulBytesPrinted++;
        if (0 == ulBytesPrinted % ulCharsInOneLine)
            printf("\n");
    }
    if (0 != ulBytesPrinted % ulCharsInOneLine)
            printf("\n");

    return;
}

void
PrintUnidentifiedTag(PSTREAM_REC_HDR_4B pTag, ULONG ulMaxDataLenToParse)
{
    USHORT  usRecType = STREAM_REC_TYPE(pTag);
    ULONG   ulRecLen = STREAM_REC_SIZE(pTag);
    ULONG   ulHdrLen = STREAM_REC_HEADER_SIZE(pTag);
    USHORT  usRecTag = pTag->usStreamRecType;
    ULONG   ulDataLen = ulRecLen - ulHdrLen;

    // Check the type of Tag.
    if (ulMaxDataLenToParse < ulRecLen) {
        _tprintf(_T("Data to parse is less(%#x) than stream record size(%#x)\n"), ulMaxDataLenToParse, ulRecLen);
        return;
    }

    ulMaxDataLenToParse -= ulHdrLen;

    switch (usRecType) {
    case TAG_TYPE_UNICODE:
        _tprintf(_T("Unknown unicode data tag = %#hx, ulDataLen = %#x\n"), usRecTag, ulDataLen);
        PrintUnicodeStreamRecData((PSTREAM_REC_DATA_TYPE_UNICODE)STREAM_REC_DATA(pTag), ulMaxDataLenToParse);
        break;
    case TAG_TYPE_BINARY:
        _tprintf(_T("Unknown Binary Record tag %#hx, ulDataLen %#x\n"), usRecTag, ulDataLen);
        PrintBinaryData(STREAM_REC_DATA(pTag), ulDataLen);
        break;
    case TAG_TYPE_NAMESPACE:
        _tprintf(_T("Unknown Namespace Tag = %#hx\n"), usRecTag);
        PrintUnidentifiedTag((PSTREAM_REC_HDR_4B)STREAM_REC_DATA(pTag), ulMaxDataLenToParse);
        break;
    default:
    case TAG_TYPE_UNKNOWN:
        _tprintf(_T("Unknown type Tag = %#hx, DataLen = %#x\n"), usRecTag, ulDataLen);
        PrintBinaryData(STREAM_REC_DATA(pTag), ulDataLen);
        break;
    }
}

