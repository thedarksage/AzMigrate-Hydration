#include "SVDstream.h"
#include "SegStream.h"
#include "assert.h"
#include "TagDetails.h"
#include "VFltCmdsExec.h"
#include <strsafe.h>

#define SYSTEM_PATH     "\\systemroot"

//Defined for per io time stamp changes
bool
PrintSVDstreamInfoV2(PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK_V2 DirtyBlock)
{
    assert(DirtyBlock);    
    bool bReturn = false;
    
    if(DirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM)
    {
        size_t  StreamSize = DirtyBlock->uHdr.Hdr.ulcbChangesInStream;
        CSegStream  SegStream(DirtyBlock->uHdr.Hdr.ppBufferArray, DirtyBlock->uHdr.Hdr.usNumberOfBuffers, StreamSize, DirtyBlock->uHdr.Hdr.ulBufferSize);

        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
            printf("%s: Printing buffer Segment info\n", __FUNCTION__);
            printf("BufferArray = %#p\n", DirtyBlock->uHdr.Hdr.ppBufferArray);
            printf("Max number of buffers = %#d\n", DirtyBlock->uHdr.Hdr.usMaxNumberOfBuffers);
            printf("Number of buffers = %#d\n", DirtyBlock->uHdr.Hdr.usNumberOfBuffers);
            if (NULL != DirtyBlock->uHdr.Hdr.ppBufferArray) {
                for (unsigned long i = 0; i < DirtyBlock->uHdr.Hdr.usNumberOfBuffers; i++) {
                    printf("Index = %d, Address = %#p\n", i, DirtyBlock->uHdr.Hdr.ppBufferArray[i]);
                }
            }        
            printf("\n");
        

        _tprintf(_T("PrintSVDstreamInfoV2: Data source = %s\n"), DataSourceToString(DirtyBlock->uTagList.TagList.TagDataSource.ulDataSource));
        }
        bReturn = PrintSVDSegStreamInfoV2(pDBTranData, DirtyBlock, SegStream);
    }
    else if(DirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE)
    {
        PCHAR astring = new CHAR[DirtyBlock->uTagList.DataFile.usLength/2 + 1];
        PCHAR systempath = SYSTEM_PATH;
        astring[DirtyBlock->uTagList.DataFile.usLength/2] = '\0';
        wcstombs(astring, DirtyBlock->uTagList.DataFile.FileName, DirtyBlock->uTagList.DataFile.usLength/2 + 1);
        if(strnicmp(astring, systempath, strlen(systempath)) == 0){
            PCHAR buffer = NULL;
            ULONG length = 0;
            length = GetEnvironmentVariableA((char*)&systempath[1],buffer,length);
            if(length !=0){
                buffer = new CHAR[(length+1)+(DirtyBlock->uTagList.DataFile.usLength/2 + 1)];
                if(GetEnvironmentVariableA((char*)&systempath[1],buffer,length)){
                    StringCchCatA(buffer,((length+1)+(DirtyBlock->uTagList.DataFile.usLength/2 + 1)),&astring[strlen(systempath)]);
                    delete astring;
                    astring = buffer;
                }
                else{
                    delete buffer;
                }
            }
        }
        CFileStream  FileStream(astring);
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
            _tprintf(_T("PrintSVDstreamInfoV2: Data source = %s\n"), DataSourceToString(DirtyBlock->uTagList.TagList.TagDataSource.ulDataSource));
        }

        bReturn = PrintSVDSegStreamInfoV2(pDBTranData, DirtyBlock, FileStream);
        delete astring;
    }
    return bReturn;
}

bool
PrintSVDstreamInfo(PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK DirtyBlock)
{
    assert(DirtyBlock);    
	bool bReturn = false;

	if(DirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM)
	{
		size_t  StreamSize = DirtyBlock->uHdr.Hdr.ulcbChangesInStream;
		CSegStream  SegStream(DirtyBlock->uHdr.Hdr.ppBufferArray, DirtyBlock->uHdr.Hdr.usNumberOfBuffers, StreamSize, DirtyBlock->uHdr.Hdr.ulBufferSize);

		if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
			printf("%s: Printing buffer Segment info\n", __FUNCTION__);
			printf("BufferArray = %#p\n", DirtyBlock->uHdr.Hdr.ppBufferArray);
			printf("Max number of buffers = %#d\n", DirtyBlock->uHdr.Hdr.usMaxNumberOfBuffers);
			printf("Number of buffers = %#d\n", DirtyBlock->uHdr.Hdr.usNumberOfBuffers);
			if (NULL != DirtyBlock->uHdr.Hdr.ppBufferArray) {
				for (unsigned long i = 0; i < DirtyBlock->uHdr.Hdr.usNumberOfBuffers; i++) {
					printf("Index = %d, Address = %#p\n", i, DirtyBlock->uHdr.Hdr.ppBufferArray[i]);
				}
			}        
			printf("\n");
		}

                if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_BASIC) {
                    _tprintf(_T("PrintSVDstreamInfo: Data source = %s\n"), DataSourceToString(DirtyBlock->uTagList.TagList.TagDataSource.ulDataSource));
                }
                bReturn = PrintSVDSegStreamInfo(pDBTranData, DirtyBlock, SegStream);
    }
    else if(DirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_DATA_FILE)
    {
        PCHAR astring = new CHAR[DirtyBlock->uTagList.DataFile.usLength/2 + 1];
        PCHAR systempath = SYSTEM_PATH;
        astring[DirtyBlock->uTagList.DataFile.usLength/2] = '\0';
        wcstombs(astring, DirtyBlock->uTagList.DataFile.FileName, DirtyBlock->uTagList.DataFile.usLength/2 + 1);
        if(strnicmp(astring, systempath, strlen(systempath)) == 0){
            PCHAR buffer = NULL;
            ULONG length = 0;
            length = GetEnvironmentVariableA((char*)&systempath[1],buffer,length);
            if(length !=0){
                buffer = new CHAR[(length+1)+(DirtyBlock->uTagList.DataFile.usLength/2 + 1)];
                if(GetEnvironmentVariableA((char*)&systempath[1],buffer,length)){
                    StringCchCatA(buffer,((length+1)+(DirtyBlock->uTagList.DataFile.usLength/2 + 1)),&astring[strlen(systempath)]);
                    delete astring;
                    astring = buffer;
                }
                else{
                    delete buffer;
                }
            }
        }
        CFileStream  FileStream(astring);
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_BASIC) {
		    _tprintf(_T("PrintSVDstreamInfo: Data source = %s\n"), DataSourceToString(DirtyBlock->uTagList.TagList.TagDataSource.ulDataSource));
		}
        bReturn = PrintSVDSegStreamInfo(pDBTranData, DirtyBlock, FileStream);
        delete astring;
    }
    return bReturn;
}

//defined for per io time stamp changes
bool
PrintSVDSegStreamInfoV2(PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK_V2  DirtyBlock, CDataStream &DataStream)
{
    assert(DirtyBlock);

    size_t  StreamOffset = 0;
    size_t  StreamSize = DataStream.size();

    // Every SVD Stream starts with SV_UINT and then SVD_PREFIX followed by SVD_HEADER1;
    SV_UINT         svdEndian;
    SVD_PREFIX      svdPrefix;
    SVD_HEADER1     svdHeader1;
    
    if ((StreamSize - StreamOffset) < (sizeof(SV_UINT)+sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1))) {
        printf("%s: Bytes remaining in stream(%#d) < size of SVD_PREFIX(%#d) + size of SVD_PREFIX(%#d) + SVD_HEADER1(%#d)\n", 
               __FUNCTION__, StreamSize - StreamOffset, sizeof(SV_UINT), sizeof(SVD_PREFIX), sizeof(SVD_HEADER1));
        return false;
    }

    DataStream.copy(StreamOffset, sizeof(SV_UINT), &svdEndian);
    if( svdEndian == SVD_TAG_LEFORMAT)
    {
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {	    
            printf("Endian of the system: Little Endian\n");
        }
    }
    else if( svdEndian == SVD_TAG_BEFORMAT)
    {
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
            printf("Endian of the system: Big Endian\n");
        }
    }
    else
    {
        printf("%s: failed for Endian Data\n", __FUNCTION__);
        return false;
    }
    StreamOffset += sizeof(SV_UINT);
    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    DataStream.copy(StreamOffset, sizeof(SVD_HEADER1), &svdHeader1);

    if (!ValidateSVDheader(svdPrefix, svdHeader1)) {
        printf("%s: ValidataSVDheader failed\n", __FUNCTION__);
        return false;
    }
    
    // Following SVD_HEADER is SVD_TIME_STAMP_V2
    StreamOffset = sizeof(SV_UINT) + sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1);
    if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP_V2))) {
        printf("%s: Bytes remaining in stream(%#d) for first change timestamp < size of SVD_PREFIX(%#d) + SVD_TIME_STAMP_V2(%#d)\n",
               __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_TIME_STAMP_V2));
        return false;
    }

    SVD_TIME_STAMP_V2  svdTimeStampFC;
    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    DataStream.copy(StreamOffset, sizeof(SVD_TIME_STAMP_V2), &svdTimeStampFC);

    if (!ValidateSVDtimeStampOfFirstChangeV2(svdPrefix, svdTimeStampFC)) {
        printf("%s: ValidateSVDtimeStampOfFirstChange failed\n", __FUNCTION__);
        return false;
    }

    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_BASIC) {	
        PrintTimeStampV2(svdTimeStampFC.TimeInHundNanoSecondsFromJan1601, svdTimeStampFC.SequenceNumber, "Time stamp for first change ");
    }	

    StreamOffset = sizeof(SV_UINT) + sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1) + sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP_V2);
    if ((StreamSize - StreamOffset) < sizeof(SVD_PREFIX)) {
        printf("%s: Check for user tags - Bytes remaining in stream(%#d) < size of SVD_PREFIX(%#d)\n",
               __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX));
        return false;
    }

    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    ULONG   ulNumberOfUserDefinedTags = 0;
    while (SVD_TAG_USER == svdPrefix.tag) {
        ulNumberOfUserDefinedTags++;
        if (0x01 != svdPrefix.count) {
            SVDtag  HeaderTag(SVD_TAG_USER);
            printf("%s: count(%#d) in svdPrefix for tag(%s) is not equal to 0x01\n",
                   __FUNCTION__, svdPrefix.count, HeaderTag.c_str());
            return false;
        }

        const unsigned char *buffer = (const unsigned char *)DataStream.buffer(StreamOffset, svdPrefix.Flags);
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
            printf("User Defined Tag%ld:\n", ulNumberOfUserDefinedTags);
            printf("Length of Tag:%d\n", svdPrefix.Flags);
            printf("Tag data is\n");
            PrintBinaryData(buffer, svdPrefix.Flags);
        }

        StreamOffset += svdPrefix.Flags;
        DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
        StreamOffset += sizeof(SVD_PREFIX);
    }

    if (ulNumberOfUserDefinedTags) {
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {	    
            printf("\n");
        }
    }

    // Either there are no Tags, or Tags have been listed
    if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_LENGTH_OF_DRTD_CHANGES, 0x00, "Total dirty changes", "SVD_TAG_LENGTH_OF_DRTD_CHANGES")) {
        printf("%s: ValidateSVDPrefix failed for total dirty changes(SVD_TAG_LENGTH_OF_DRTD_CHANGES)\n", __FUNCTION__);
        return false;
    }

    ULONGLONG   OffsetFromTotalChangesToLCtimeStamp;
    DataStream.copy(StreamOffset, sizeof(ULONGLONG), &OffsetFromTotalChangesToLCtimeStamp);
    StreamOffset += sizeof(ULONGLONG);

    size_t  OffsetToFirstChange = StreamOffset;
    StreamOffset += (size_t)OffsetFromTotalChangesToLCtimeStamp;
    size_t  OffsetToLCtimeStamp = StreamOffset;

    // StreamOffset now points to last change time stamp
    if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP_V2))) {
        printf("%s: Bytes remaining in stream(%#d) for last change time stamp < size of SVD_PREFIX(%#d) + SVD_TIME_STAMP_V2(%#d)\n",
               __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_TIME_STAMP_V2));
        return false;
    }

    SVD_TIME_STAMP_V2  svdTimeStampLC;
    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    DataStream.copy(StreamOffset, sizeof(SVD_TIME_STAMP_V2), &svdTimeStampLC);

    if (!ValidateSVDtimeStampOfLastChangeV2(svdPrefix, svdTimeStampLC)) {
        printf("%s: ValidateSVDtimeStampOfLastChange failed\n", __FUNCTION__);
        return false;
    }

    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
        PrintTimeStampV2(svdTimeStampLC.TimeInHundNanoSecondsFromJan1601, svdTimeStampLC.SequenceNumber, "Time stamp for last change  ");
    }

    StreamOffset = OffsetToFirstChange;
    ULONG   ulTotalNumberOfChanges = 0;
    ULONGLONG   ullcbChanges = 0;
    TIME_ZONE_INFORMATION   TimeZone;
//    SYSTEMTIME              SystemTime, TimeDelta;
    ULONGLONG               TotalTime = 0;
    GetTimeZoneInformation(&TimeZone);
	bool old = 0;
	SV_UINT uiSequenceNumberDelta_old;
    SV_UINT uiTimeDelta_old;
    while (StreamOffset != OffsetToLCtimeStamp) {
        SVD_DIRTY_BLOCK_V2 svdDirtyChange;

        // There are changes.
        if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX) + sizeof(SVD_DIRTY_BLOCK_V2))) {
            printf("%s: Bytes remaining in stream(%#d) for dirty change < size of SVD_PREFIX(%#d) + SVD_DIRTY_BLOCK_V2(%#d)\n",
                   __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_DIRTY_BLOCK_V2));
            return false;
        }
        DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
        StreamOffset += sizeof(SVD_PREFIX);
        DataStream.copy(StreamOffset, sizeof(SVD_DIRTY_BLOCK_V2), &svdDirtyChange);
        StreamOffset += sizeof(SVD_DIRTY_BLOCK_V2);

	if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_DIRTY_BLOCK_DATA_V2, 0x00, "Dirty change", "SVD_TAG_DIRTY_BLOCK_DATA_V2")) {
            printf("%s: ValidateSVDPrefix failed for dirty change(SVD_TAG_DIRTY_BLOCK)\n", __FUNCTION__);
            return false;
        }


	//This is the place where the dirty block data is written to
	//Persistent store. For this sync parameter must be true.
	//By default it is.
        if (pDBTranData->sync) {

            //Write data to target. Src is SVD stream
            SOURCESTREAM SourceStream;
	    SourceStream.StreamOffset = StreamOffset;
	    SourceStream.DataStream = &DataStream;
            if (DataSync(pDBTranData, &SourceStream, svdDirtyChange.ByteOffset, svdDirtyChange.Length)) {
                printf("Error in DataSync\n");				 
                return false;                         				 
            }            
	}
        StreamOffset += (size_t)svdDirtyChange.Length;

        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
            printf("Index = %4d, Offset = %#12I64x, Len = %#6x,\n              TimeDelta = %#9x, SeqDelta = %x\n", ulTotalNumberOfChanges, 
                            svdDirtyChange.ByteOffset,
                            svdDirtyChange.Length,
                            svdDirtyChange.uiTimeDelta,
                            svdDirtyChange.uiSequenceNumberDelta);
        } 

        // Space for last time stamp should be in here.
        if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP_V2))) {
            printf("%s: Bytes remaining in stream(%#d) after incrementing for change < size of SVD_PREFIX(%#d) + SVD_TIME_STAMP_V2(%#d)\n",
                   __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_TIME_STAMP_V2));
            return false;
        }
        //Compare prev delta value with curr delta value
        if (old) {
            ULONG   ulTotalNumberOfChanges_old = ulTotalNumberOfChanges - 1;
            if ((((uiSequenceNumberDelta_old) > (svdDirtyChange.uiSequenceNumberDelta)) && 
                    ((0xFFFFFFFFF8200000) > (uiSequenceNumberDelta_old))) || 
                    ((uiTimeDelta_old) > 
                    (svdDirtyChange.uiTimeDelta))) {
                printf("%s: Prev delta change Seq Num (%#I64x.%#d) is greater than Current delta change Seq num(%#I64x.%#d) at index %u\n",
                      __FUNCTION__, uiTimeDelta_old, 
                      uiSequenceNumberDelta_old,
                      svdDirtyChange.uiTimeDelta, svdDirtyChange.uiSequenceNumberDelta, 
                      ulTotalNumberOfChanges);
                return false;
            }
        }
        uiSequenceNumberDelta_old = svdDirtyChange.uiSequenceNumberDelta;
        uiTimeDelta_old = svdDirtyChange.uiTimeDelta;
		old = 1;
        ulTotalNumberOfChanges++;
        ullcbChanges += svdDirtyChange.Length;
    }

    // Check if changes are bytes of changes match.
    if (ulTotalNumberOfChanges != DirtyBlock->uHdr.Hdr.cChanges) {
        printf("%s: Changes in stream(%#x) does not match from header(%#x)\n", __FUNCTION__, 
               ulTotalNumberOfChanges, DirtyBlock->uHdr.Hdr.cChanges);
        return false;
    }

    if (ullcbChanges != DirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart) {
        printf("%s: cbChanges in stream(%#I64x) does not match from header(%#I64x)\n", __FUNCTION__, 
               ullcbChanges, DirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart);
	return false;
    }
    
    if (DirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM) {
        if (svdTimeStampFC.TimeInHundNanoSecondsFromJan1601 != DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
            printf("%s: svdTimeStampFC.TimeInHundNanoSecondsFromJan1601 (%#I64x) is not equal to "
                    "TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601(%#I64x)\n",
                    __FUNCTION__, svdTimeStampFC.TimeInHundNanoSecondsFromJan1601, 
                    DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
            return false;
        }
        if (svdTimeStampLC.TimeInHundNanoSecondsFromJan1601 != DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
            printf("%s: svdTimeStampLC.TimeInHundNanoSecondsFromJan1601 (%#I64x) is not equal to "
                    "TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601(%#I64x)\n",
                    __FUNCTION__, svdTimeStampLC.TimeInHundNanoSecondsFromJan1601, 
                    DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);		
            return false;
        }
        if (svdTimeStampFC.SequenceNumber != DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber) {
            printf("%s: svdTimeStampFC.SequenceNumber (%#I64x) is not equal to "
                    "TagList.TagTimeStampOfFirstChange.SequenceNumber(%#I64x)\n",
                    __FUNCTION__, svdTimeStampFC.SequenceNumber, 
                    DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber);
            return false;
        }
        if (svdTimeStampLC.SequenceNumber != DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber) {
            printf("%s: svdTimeStampLC.SequenceNumber (%#I64x) is not equal to "
                    "TagList.TagTimeStampOfLastChange.SequenceNumber(%#I64x)\n",
                    __FUNCTION__, svdTimeStampLC.SequenceNumber, 
                    DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber);
            return false;
        }
    }
    pDBTranData->FCSequenceNumberV2.ullSequenceNumber = svdTimeStampFC.SequenceNumber;
    pDBTranData->FCSequenceNumberV2.ullTimeStamp = svdTimeStampFC.TimeInHundNanoSecondsFromJan1601;
    pDBTranData->LCSequenceNumberV2.ullSequenceNumber = svdTimeStampLC.SequenceNumber;
    pDBTranData->LCSequenceNumberV2.ullTimeStamp = svdTimeStampLC.TimeInHundNanoSecondsFromJan1601;
    return true;
}


bool
PrintSVDSegStreamInfo(PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK DirtyBlock, CDataStream &DataStream)
{
    assert(DirtyBlock);

    size_t  StreamOffset = 0;
    size_t  StreamSize = DataStream.size();

    // Every SVD Stream starts with SV_UINT and then SVD_PREFIX followed by SVD_HEADER1;
    SV_UINT         svdEndian;
    SVD_PREFIX      svdPrefix;
    SVD_HEADER1     svdHeader1;

    if ((StreamSize - StreamOffset) < (sizeof(SV_UINT)+sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1))) {
        printf("%s: Bytes remaining in stream(%#d) < size of SVD_PREFIX(%#d) + size of SVD_PREFIX(%#d) + SVD_HEADER1(%#d)\n", 
               __FUNCTION__, StreamSize - StreamOffset, sizeof(SV_UINT), sizeof(SVD_PREFIX), sizeof(SVD_HEADER1));
        return false;
    }

    DataStream.copy(StreamOffset, sizeof(SV_UINT), &svdEndian);
    if( svdEndian == SVD_TAG_LEFORMAT)
    {
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_BASIC) {	    
            printf("Endian of the system: Little Endian\n");
        }
    }
    else if( svdEndian == SVD_TAG_BEFORMAT)
    {
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_BASIC) {
            printf("Endian of the system: Big Endian\n");
        }
    }
    else
    {
        printf("%s: failed for Endian Data\n", __FUNCTION__);
        return false;
    }
    StreamOffset += sizeof(SV_UINT);
    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    DataStream.copy(StreamOffset, sizeof(SVD_HEADER1), &svdHeader1);

    if (!ValidateSVDheader(svdPrefix, svdHeader1)) {
        printf("%s: ValidataSVDheader failed\n", __FUNCTION__);
        return false;
    }
    
    // Following SVD_HEADER is SVD_TIME_STAMP
    StreamOffset = sizeof(SV_UINT) + sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1);
    if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP))) {
        printf("%s: Bytes remaining in stream(%#d) for first change timestamp < size of SVD_PREFIX(%#d) + SVD_TIME_STAMP(%#d)\n",
               __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_TIME_STAMP));
        return false;
    }

    SVD_TIME_STAMP  svdTimeStampFC;
    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    DataStream.copy(StreamOffset, sizeof(SVD_TIME_STAMP), &svdTimeStampFC);

    if (!ValidateSVDtimeStampOfFirstChange(svdPrefix, svdTimeStampFC)) {
        printf("%s: ValidateSVDtimeStampOfFirstChange failed\n", __FUNCTION__);
        return false;
    }

    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_BASIC) {	
        PrintTimeStamp(svdTimeStampFC.TimeInHundNanoSecondsFromJan1601, svdTimeStampFC.ulSequenceNumber, "Time stamp for first change ");
    }

    StreamOffset = sizeof(SV_UINT) + sizeof(SVD_PREFIX) + sizeof(SVD_HEADER1) + sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP);
    if ((StreamSize - StreamOffset) < sizeof(SVD_PREFIX)) {
        printf("%s: Check for user tags - Bytes remaining in stream(%#d) < size of SVD_PREFIX(%#d)\n",
               __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX));
        return false;
    }

    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    ULONG   ulNumberOfUserDefinedTags = 0;
    while (SVD_TAG_USER == svdPrefix.tag) {
        ulNumberOfUserDefinedTags++;
        if (0x01 != svdPrefix.count) {
            SVDtag  HeaderTag(SVD_TAG_USER);
            printf("%s: count(%#d) in svdPrefix for tag(%s) is not equal to 0x01\n",
                   __FUNCTION__, svdPrefix.count, HeaderTag.c_str());
            return false;
        }

        const unsigned char *buffer = (const unsigned char *)DataStream.buffer(StreamOffset, svdPrefix.Flags);
        if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_BASIC) {
            printf("User Defined Tag%ld:\n", ulNumberOfUserDefinedTags);
            printf("Length of Tag:%d\n", svdPrefix.Flags);
            printf("Tag data is\n");
            PrintBinaryData(buffer, svdPrefix.Flags);
        }

        StreamOffset += svdPrefix.Flags;
        DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
        StreamOffset += sizeof(SVD_PREFIX);
    }
    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_BASIC) {
        if (ulNumberOfUserDefinedTags) {
            printf("\n");
        }
    }

    // Either there are no Tags, or Tags have been listed
    if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_LENGTH_OF_DRTD_CHANGES, 0x00, "Total dirty changes", "SVD_TAG_LENGTH_OF_DRTD_CHANGES")) {
        printf("%s: ValidateSVDPrefix failed for total dirty changes(SVD_TAG_LENGTH_OF_DRTD_CHANGES)\n", __FUNCTION__);
        return false;
    }

    ULONGLONG   OffsetFromTotalChangesToLCtimeStamp;
    DataStream.copy(StreamOffset, sizeof(ULONGLONG), &OffsetFromTotalChangesToLCtimeStamp);
    StreamOffset += sizeof(ULONGLONG);

    size_t  OffsetToFirstChange = StreamOffset;
    StreamOffset += (size_t)OffsetFromTotalChangesToLCtimeStamp;
    size_t  OffsetToLCtimeStamp = StreamOffset;

    // StreamOffset now points to last change time stamp
    if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP))) {
        printf("%s: Bytes remaining in stream(%#d) for last change time stamp < size of SVD_PREFIX(%#d) + SVD_TIME_STAMP(%#d)\n",
               __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_TIME_STAMP));
        return false;
    }

    SVD_TIME_STAMP  svdTimeStampLC;
    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    DataStream.copy(StreamOffset, sizeof(SVD_TIME_STAMP), &svdTimeStampLC);

    if (!ValidateSVDtimeStampOfLastChange(svdPrefix, svdTimeStampLC)) {
        printf("%s: ValidateSVDtimeStampOfLastChange failed\n", __FUNCTION__);
        return false;
    }

    if (pDBTranData->ulLevelOfDetail >= DETAIL_PRINT_CHANGES) {
        PrintTimeStamp(svdTimeStampLC.TimeInHundNanoSecondsFromJan1601, svdTimeStampLC.ulSequenceNumber, "Time stamp for last change ");
    }	

    StreamOffset = OffsetToFirstChange;
    ULONG   ulTotalNumberOfChanges = 0;
    ULONGLONG   ullcbChanges = 0;
    while (StreamOffset != OffsetToLCtimeStamp) {
        SVD_DIRTY_BLOCK svdDirtyChange;

        // There are changes.
        if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX) + sizeof(SVD_DIRTY_BLOCK))) {
            printf("%s: Bytes remaining in stream(%#d) for dirty change < size of SVD_PREFIX(%#d) + SVD_DIRTY_BLOCK(%#d)\n",
                   __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_DIRTY_BLOCK));
            return false;
        }

        DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
        StreamOffset += sizeof(SVD_PREFIX);	
        if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_DIRTY_BLOCK_DATA, 0x00, "Dirty change", "SVD_TAG_DIRTY_BLOCK_DATA")) {
            printf("%s: ValidateSVDPrefix failed for dirty change(SVD_TAG_DIRTY_BLOCK)\n", __FUNCTION__);
            return false;
        }
	
	
        DataStream.copy(StreamOffset, sizeof(SVD_DIRTY_BLOCK), &svdDirtyChange);
        StreamOffset += sizeof(SVD_DIRTY_BLOCK);

        if (pDBTranData->sync) {
            //Write data to target. Src is SVD stream
            SOURCESTREAM SourceStream;
	    SourceStream.StreamOffset = StreamOffset;
	    SourceStream.DataStream = &DataStream;
            if (DataSync(pDBTranData, &SourceStream, svdDirtyChange.ByteOffset, svdDirtyChange.Length)) {
                printf("Error in DataSync\n");				 
                return false;                         				 
            }	    

            
	}
        StreamOffset += (size_t)svdDirtyChange.Length;
			    	
        // Space for last time stamp should be in here.
        if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX) + sizeof(SVD_TIME_STAMP))) {
            printf("%s: Bytes remaining in stream(%#d) after incrementing for change < size of SVD_PREFIX(%#d) + SVD_TIME_STAMP(%#d)\n",
                   __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_TIME_STAMP));
            return false;
        }

        ulTotalNumberOfChanges++;
        ullcbChanges += svdDirtyChange.Length;
    }

    // Check if changes are bytes of changes match.
    if (ulTotalNumberOfChanges != DirtyBlock->uHdr.Hdr.cChanges) {
        printf("%s: Changes in stream(%#x) does not match from header(%#x)\n", __FUNCTION__, 
               ulTotalNumberOfChanges, DirtyBlock->uHdr.Hdr.cChanges);
        return false;
    }

    if (ullcbChanges != DirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart) {
        printf("%s: cbChanges in stream(%#I64x) does not match from header(%#I64x)\n", __FUNCTION__, 
               ullcbChanges, DirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart);
	return false;
    }

    if (DirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM) {
        if (svdTimeStampFC.TimeInHundNanoSecondsFromJan1601 != DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601) {
            printf("%s: svdTimeStampFC.TimeInHundNanoSecondsFromJan1601 (%#I64x) is not equal to "
                    "TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601(%#I64x)\n",
                    __FUNCTION__, svdTimeStampFC.TimeInHundNanoSecondsFromJan1601, 
                    DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
            return false;
        }
        if (svdTimeStampLC.TimeInHundNanoSecondsFromJan1601 != DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601) {
            printf("%s: svdTimeStampLC.TimeInHundNanoSecondsFromJan1601 (%#I64x) is not equal to "
                    "TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601(%#I64x)\n",
                    __FUNCTION__, svdTimeStampLC.TimeInHundNanoSecondsFromJan1601, 
                    DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);		
            return false;
        }
        if (svdTimeStampFC.ulSequenceNumber != DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ulSequenceNumber) {
            printf("%s: svdTimeStampFC.SequenceNumber (%u) is not equal to "
                    "TagList.TagTimeStampOfFirstChange.SequenceNumber(%u)\n",
                    __FUNCTION__, svdTimeStampFC.ulSequenceNumber, 
                    DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ulSequenceNumber);
            return false;
        }
        if (svdTimeStampLC.ulSequenceNumber != DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ulSequenceNumber) {
            printf("%s: svdTimeStampLC.SequenceNumber (%u) is not equal to "
                    "TagList.TagTimeStampOfLastChange.SequenceNumber(%u)\n",
                    __FUNCTION__, svdTimeStampLC.ulSequenceNumber, 
                    DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ulSequenceNumber);
            return false;
        }
    }
    pDBTranData->FCSequenceNumber.ulSequenceNumber = svdTimeStampFC.ulSequenceNumber;
    pDBTranData->FCSequenceNumber.ullTimeStamp = svdTimeStampFC.TimeInHundNanoSecondsFromJan1601;
    pDBTranData->LCSequenceNumber.ulSequenceNumber = svdTimeStampLC.ulSequenceNumber;
    pDBTranData->LCSequenceNumber.ullTimeStamp = svdTimeStampLC.TimeInHundNanoSecondsFromJan1601;
    return true;
}

//Define for per io time stamp changes
bool
ValidateSVDtimeStampOfFirstChangeV2(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP_V2 &svdTimeStamp)
{
    if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2, 0x00, "Time stamp of first change", "SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2")) {
        printf("%s: ValidateSVDPrefix failed for SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2\n", __FUNCTION__);
        return false;
    }

    if (!ValidateStreamRecTypeTimeStampV2(svdTimeStamp)) {
        printf("%s: ValidateStreamREcTypeTimeStamp for first change failed\n", __FUNCTION__);
        return false;
    }

    return true;
}

bool
ValidateSVDtimeStampOfFirstChange(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP &svdTimeStamp)
{
    if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE, 0x00, "Time stamp of first change", "SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE")) {
        printf("%s: ValidateSVDPrefix failed for SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE\n", __FUNCTION__);
        return false;
    }

    if (!ValidateStreamRecTypeTimeStamp(svdTimeStamp)) {
        printf("%s: ValidateStreamREcTypeTimeStamp for first change failed\n", __FUNCTION__);
        return false;
    }

    return true;
}

//Define for per io time stamp changes
bool
ValidateSVDtimeStampOfLastChangeV2(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP_V2 &svdTimeStamp)
{
    if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2, 0x00, "Time stamp of last change", "SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2")) {
        printf("%s: ValidateSVDPrefix failed for SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2\n", __FUNCTION__);
        return false;
    }

    if (!ValidateStreamRecTypeTimeStampV2(svdTimeStamp)) {
        printf("%s: ValidateStreamREcTypeTimeStamp for last change failed\n", __FUNCTION__);
        return false;
    }

    return true;
}


bool
ValidateSVDtimeStampOfLastChange(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP &svdTimeStamp)
{
    if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE, 0x00, "Time stamp of last change", "SVD_TAG_TIME_STAMP_OF_LAST_CHANGE")) {
        printf("%s: ValidateSVDPrefix failed for SVD_TAG_TIME_STAMP_OF_LAST_CHANGE\n", __FUNCTION__);
        return false;
    }

    if (!ValidateStreamRecTypeTimeStamp(svdTimeStamp)) {
        printf("%s: ValidateStreamREcTypeTimeStamp for last change failed\n", __FUNCTION__);
        return false;
    }

    return true;
}

//Defined for per io time stamp changes
bool
ValidateStreamRecTypeTimeStampV2(SVD_TIME_STAMP_V2 &svdTimeStamp)
{
    if (STREAM_REC_TYPE_TIME_STAMP_TAG != svdTimeStamp.Header.usStreamRecType) {
        printf("%s: StreamRecType(%hd) does not match STREAM_REC_TYPE_TIME_STAMP_TAG(%hd)\n",
               __FUNCTION__, svdTimeStamp.Header.usStreamRecType, STREAM_REC_TYPE_TIME_STAMP_TAG);
        return false;
    }

    if (0 != svdTimeStamp.Header.ucFlags) {
        printf("%s: StreamRec Flags(%d) are not zero\n", __FUNCTION__, svdTimeStamp.Header.ucFlags);
    }

    //if (sizeof(SVD_TIME_STAMP_V2) != svdTimeStamp.Header.ucLength) {
    //    printf("%s: StreamRec Length(%d) does not match sizeof SVD_TIME_STAMP_V2(%#d)\n",
    //           __FUNCTION__, svdTimeStamp.Header.ucLength, sizeof(SVD_TIME_STAMP_V2));
    //    return false;
    //}

    return true;
}

bool
ValidateStreamRecTypeTimeStamp(SVD_TIME_STAMP &svdTimeStamp)
{
    if (STREAM_REC_TYPE_TIME_STAMP_TAG != svdTimeStamp.Header.usStreamRecType) {
        printf("%s: StreamRecType(%hd) does not match STREAM_REC_TYPE_TIME_STAMP_TAG(%hd)\n",
               __FUNCTION__, svdTimeStamp.Header.usStreamRecType, STREAM_REC_TYPE_TIME_STAMP_TAG);
        return false;
    }

    if (0 != svdTimeStamp.Header.ucFlags) {
        printf("%s: StreamRec Flags(%d) are not zero\n", __FUNCTION__, svdTimeStamp.Header.ucFlags);
    }

    if (sizeof(SVD_TIME_STAMP) != svdTimeStamp.Header.ucLength) {
        printf("%s: StreamRec Length(%d) does not match sizeof SVD_TIME_STAMP(%#d)\n",
               __FUNCTION__, svdTimeStamp.Header.ucLength, sizeof(SVD_TIME_STAMP));
        return false;
    }

    return true;
}

//Defined for per io time stamp changes
bool
ValidateSVDheaderV2(SVD_PREFIX &svdPrefix, SVD_HEADER_V2 &svdHeader1)
{
    if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_HEADER_V2, 0x00, "SVD_HEADER_V2", "SVD_TAG_HEADER_V2")) {
        printf("%s: ValidateSVDPrefix failed for SVD_TAG_HEADER_V2\n", __FUNCTION__);
        return false;
    }

    SVD_HEADER_V2     svdHeaderInitialized = {0};

    if (memcmp(&svdHeaderInitialized, &svdHeader1, sizeof(SVD_HEADER_V2)) != 0) {
        printf("%s: SVD_TAG_HEADER_V2 contains non zero data\n", __FUNCTION__);
        PrintBinaryData((const unsigned char *)&svdHeader1, sizeof(SVD_HEADER_V2));
    }

    return true;
}


bool
ValidateSVDheader(SVD_PREFIX &svdPrefix, SVD_HEADER1 &svdHeader1)
{
    if (!ValidateSVDPrefix(svdPrefix, SVD_TAG_HEADER1, 0x00, "SVD_HEADER1", "SVD_TAG_HEADER1")) {
        printf("%s: ValidateSVDPrefix failed for SVD_HEADER1\n", __FUNCTION__);
        return false;
    }

    SVD_HEADER1     svdHeaderInitialized = {0};

    if (memcmp(&svdHeaderInitialized, &svdHeader1, sizeof(SVD_HEADER1)) != 0) {
        printf("%s: SVD_HEADER1 contains non zero data\n", __FUNCTION__);
        PrintBinaryData((const unsigned char *)&svdHeader1, sizeof(SVD_HEADER1));
    }
    return true;
}

bool
ValidateSVDPrefix(SVD_PREFIX &svdPrefix, ULONG ulTag, ULONG ulFlags, char *StructName, char *TagName)
{
    SVDtag  HeaderTag(ulTag);

    if (svdPrefix.tag != ulTag) {
        SVDtag  HeaderTagInStream(svdPrefix.tag);
        printf("%s: SVD Prefix tag %s for %s did not match %s(%s)\n", 
               __FUNCTION__, HeaderTag.c_str(), StructName, TagName, HeaderTagInStream.c_str());
        return false;
    }

    if (0x01 != svdPrefix.count) {
        printf("%s: count(%#d) in svdPrefix for tag(%s) is not equal to 0x01\n",
               __FUNCTION__, svdPrefix.count, HeaderTag.c_str());
        return false;
    }

    if (ulFlags != svdPrefix.Flags) {
        printf("%s: Flags(%#d) in svdPrefix for tag(%s) are not equal to 0x00\n",
               __FUNCTION__, svdPrefix.Flags, HeaderTag.c_str());
    }

    return true;
}

//Define for per io time stamp changes
void
PrintTimeStampV2(ULONGLONG TimeInHundNanoSecondsFromJan1601, ULONGLONG ulSequenceNumber, char *Description)
{
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime, LocalTime;

    GetTimeZoneInformation(&TimeZone);
    FileTimeToSystemTime((FILETIME *)&TimeInHundNanoSecondsFromJan1601, &SystemTime);
    SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);

    printf("%s : SequenceNumber : %#I64x  TimeStamp : %02d/%02d/%04d %02d:%02d:%02d:%04d,  %#I64x\n", Description,ulSequenceNumber,
                        LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                        LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds,
                        TimeInHundNanoSecondsFromJan1601 );
}

void
PrintTimeStamp(ULONGLONG TimeInHundNanoSecondsFromJan1601, ULONG ulSequenceNumber, char *Description)
{
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime, LocalTime;

    GetTimeZoneInformation(&TimeZone);
    FileTimeToSystemTime((FILETIME *)&TimeInHundNanoSecondsFromJan1601, &SystemTime);
    SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &LocalTime);

    printf("%s : SequenceNumber : %#x,TimeStamp :  %02d/%02d/%04d %02d:%02d:%02d:%04d,  %#I64x\n", 
                        Description,
                        ulSequenceNumber,
                        LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear,
                        LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds,
                        TimeInHundNanoSecondsFromJan1601 );
}

// SVDtag class
// 
SVDtag::SVDtag(ULONG ulTag) : m_ulTag(ulTag)
{
    m_sTag.push_back(char(ulTag & 0xFF));
    m_sTag.push_back(char((ulTag & 0xFF00) >> 8));
    m_sTag.push_back(char((ulTag & 0xFF0000) >> 16));
    m_sTag.push_back(char((ulTag & 0xFF000000) >> 24));
}

SVDtag::~SVDtag()
{

}
