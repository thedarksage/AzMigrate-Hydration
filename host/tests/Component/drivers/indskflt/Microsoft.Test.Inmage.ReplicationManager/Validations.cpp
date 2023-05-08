#ifdef SV_WINDOWS
#include "stdafx.h"
#endif
#include "ReplicationManager.h"
#include "ReplicationWorker.h"

void CReplicationWorker::ValidateTimeStampSeqV2(PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK_V2 pDirtyBlock, PTSSNSEQIDV2 ptempPrevTSSequenceNumberV2)
{
    // TODO:
}

void CReplicationWorker::ValidateAndSyncDirtyBlock(PGET_DB_TRANS_DATA pDBTranData, PUDIRTY_BLOCK_V2  DirtyBlock, CDataStream &DataStream)
{
    size_t  StreamOffset = 0;
    size_t  StreamSize = DataStream.size();

    // Every SVD Stream starts with SV_UINT and then SVD_PREFIX followed by SVD_HEADER1;
    SV_UINT         svdEndian;
    SVD_PREFIX      svdPrefix;
    SVD_HEADER1     svdHeader1;

    if ((StreamSize - StreamOffset) < (sizeof(SV_UINT)+sizeof(SVD_PREFIX)+sizeof(SVD_HEADER1)))
    {
        throw InvalidSVDStreamException("%s: Bytes remaining in stream(%#d) < size of SVD_PREFIX(%#d) + size of SVD_PREFIX(%#d) + SVD_HEADER1(%#d)\n",
            __FUNCTION__, StreamSize - StreamOffset, sizeof(SV_UINT), sizeof(SVD_PREFIX), sizeof(SVD_HEADER1));
    }

    DataStream.copy(StreamOffset, sizeof(SV_UINT), &svdEndian);

    if ((SVD_TAG_LEFORMAT != svdEndian) && (SVD_TAG_BEFORMAT != svdEndian))
    {
        throw InvalidSVDStreamException("%s: failed for Endian Data\n", __FUNCTION__);
    }

    StreamOffset += sizeof(SV_UINT);
    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);

    StreamOffset += sizeof(SVD_PREFIX);
    DataStream.copy(StreamOffset, sizeof(SVD_HEADER1), &svdHeader1);

    ValidateSVDheader(svdPrefix, svdHeader1);

    // Following SVD_HEADER is SVD_TIME_STAMP_V2
    StreamOffset = sizeof(SV_UINT)+sizeof(SVD_PREFIX)+sizeof(SVD_HEADER1);

    if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX)+sizeof(SVD_TIME_STAMP_V2)))
    {
        throw InvalidSVDStreamException("%s: Bytes remaining in stream(%#d) for first change timestamp < size of SVD_PREFIX(%#d) + SVD_TIME_STAMP_V2(%#d)\n",
            __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_TIME_STAMP_V2));
    }

    SVD_TIME_STAMP_V2  svdTimeStampFC;
    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    DataStream.copy(StreamOffset, sizeof(SVD_TIME_STAMP_V2), &svdTimeStampFC);

    ValidateSVDtimeStampOfFirstChangeV2(svdPrefix, svdTimeStampFC);

    StreamOffset = sizeof(SV_UINT)+sizeof(SVD_PREFIX)+sizeof(SVD_HEADER1)+sizeof(SVD_PREFIX)+sizeof(SVD_TIME_STAMP_V2);
    if ((StreamSize - StreamOffset) < sizeof(SVD_PREFIX))
    {
        throw InvalidSVDStreamException("%s: Check for user tags - Bytes remaining in stream(%#d) < size of SVD_PREFIX(%#d)\n",
            __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX));
    }

    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);

    SV_ULONG   ulNumberOfUserDefinedTags = 0;
    while (SVD_TAG_USER == svdPrefix.tag)
    {
        ulNumberOfUserDefinedTags++;
        if (0x01 != svdPrefix.count)
        {
            SVDtag  HeaderTag(SVD_TAG_USER);
            throw InvalidSVDStreamException("%s: count(%#d) in svdPrefix for tag(%s) is not equal to 0x01\n",
                __FUNCTION__, svdPrefix.count, HeaderTag.c_str());
        }

        const unsigned char *buffer = (const unsigned char *)DataStream.buffer(StreamOffset, svdPrefix.Flags);

        StreamOffset += svdPrefix.Flags;
        DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
        StreamOffset += sizeof(SVD_PREFIX);
    }

    // Either there are no Tags, or Tags have been listed
    ValidateSVDPrefix(svdPrefix, SVD_TAG_LENGTH_OF_DRTD_CHANGES, 0x00, "Total dirty changes", "SVD_TAG_LENGTH_OF_DRTD_CHANGES");

    SV_ULONGLONG   OffsetFromTotalChangesToLCtimeStamp;
    DataStream.copy(StreamOffset, sizeof(SV_ULONGLONG), &OffsetFromTotalChangesToLCtimeStamp);
    StreamOffset += sizeof(SV_ULONGLONG);

    size_t  OffsetToFirstChange = StreamOffset;
    StreamOffset += (size_t)OffsetFromTotalChangesToLCtimeStamp;
    size_t  OffsetToLCtimeStamp = StreamOffset;

    // StreamOffset now points to last change time stamp
    if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX)+sizeof(SVD_TIME_STAMP_V2)))
    {
        throw InvalidSVDStreamException("%s: Bytes remaining in stream(%#d) for last change time stamp < size of SVD_PREFIX(%#d) + SVD_TIME_STAMP_V2(%#d)\n",
            __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_TIME_STAMP_V2));
    }

    SVD_TIME_STAMP_V2  svdTimeStampLC;
    DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
    StreamOffset += sizeof(SVD_PREFIX);
    DataStream.copy(StreamOffset, sizeof(SVD_TIME_STAMP_V2), &svdTimeStampLC);

    ValidateSVDtimeStampOfLastChangeV2(svdPrefix, svdTimeStampLC);

    StreamOffset = OffsetToFirstChange;
    SV_ULONG   ulTotalNumberOfChanges = 0;
    SV_ULONGLONG   ullcbChanges = 0;
    SV_ULONGLONG               TotalTime = 0;
#ifdef SV_WINDOWS
    TIME_ZONE_INFORMATION   TimeZone;
    SYSTEMTIME              SystemTime, TimeDelta;
	GetTimeZoneInformation(&TimeZone);
#endif
    bool old = 0;
    SV_UINT uiSequenceNumberDelta_old;
    SV_UINT uiTimeDelta_old;

    while (StreamOffset != OffsetToLCtimeStamp)
    {
        SVD_DIRTY_BLOCK_V2 svdDirtyChange;

        // There are changes.
        if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX)+sizeof(SVD_DIRTY_BLOCK_V2)))
        {
            throw InvalidSVDStreamException("%s: Bytes remaining in stream(%#d) for dirty change < size of SVD_PREFIX(%#d) + SVD_DIRTY_BLOCK_V2(%#d)\n",
                __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_DIRTY_BLOCK_V2));
        }

        DataStream.copy(StreamOffset, sizeof(SVD_PREFIX), &svdPrefix);
        StreamOffset += sizeof(SVD_PREFIX);
        DataStream.copy(StreamOffset, sizeof(SVD_DIRTY_BLOCK_V2), &svdDirtyChange);
        StreamOffset += sizeof(SVD_DIRTY_BLOCK_V2);

        ValidateSVDPrefix(svdPrefix, SVD_TAG_DIRTY_BLOCK_DATA_V2, 0x00, "Dirty change", "SVD_TAG_DIRTY_BLOCK_DATA_V2");


        //Write data to target. Src is SVD stream
        SOURCESTREAM 	SourceStream(&DataStream, StreamOffset);
        m_dataProcessor->ProcessChanges(&SourceStream, svdDirtyChange.ByteOffset, svdDirtyChange.Length);

        StreamOffset += (size_t)svdDirtyChange.Length;
		
        TotalTime = svdTimeStampFC.TimeInHundNanoSecondsFromJan1601 + DirtyBlock->TimeDeltaArray[ulTotalNumberOfChanges];
#ifdef SV_WINDOWS
        FileTimeToSystemTime((FILETIME *)&TotalTime, &SystemTime);
        SystemTimeToTzSpecificLocalTime(&TimeZone, &SystemTime, &TimeDelta);
#endif
        
        // Space for last time stamp should be in here.
        if ((StreamSize - StreamOffset) < (sizeof(SVD_PREFIX)+sizeof(SVD_TIME_STAMP_V2)))
        {
            throw InvalidSVDStreamException("%s: Bytes remaining in stream(%#d) after incrementing for change < size of SVD_PREFIX(%#d) + SVD_TIME_STAMP_V2(%#d)\n",
                __FUNCTION__, StreamSize - StreamOffset, sizeof(SVD_PREFIX), sizeof(SVD_TIME_STAMP_V2));
        }

        //Compare prev delta value with curr delta value
        if (old)
        {
            SV_ULONG   ulTotalNumberOfChanges_old = ulTotalNumberOfChanges - 1;
            if ((((uiSequenceNumberDelta_old) > (svdDirtyChange.uiSequenceNumberDelta)) &&
                ((0xFFFFFFFFF8200000) > (uiSequenceNumberDelta_old))) ||
                ((uiTimeDelta_old) >
                (svdDirtyChange.uiTimeDelta)))
            {
                throw InvalidSVDStreamException("%s: Prev delta change Seq Num (%#I64x.%#d) is greater than Current delta change Seq num(%#I64x.%#d) at index %u\n",
                    __FUNCTION__, uiTimeDelta_old,
                    uiSequenceNumberDelta_old,
                    svdDirtyChange.uiTimeDelta, svdDirtyChange.uiSequenceNumberDelta,
                    ulTotalNumberOfChanges);
            }
        }

        uiSequenceNumberDelta_old = svdDirtyChange.uiSequenceNumberDelta;
        uiTimeDelta_old = svdDirtyChange.uiTimeDelta;
        old = 1;
        ulTotalNumberOfChanges++;
        ullcbChanges += svdDirtyChange.Length;
    }

    // Check if changes are bytes of changes match.
    if (ulTotalNumberOfChanges != DirtyBlock->uHdr.Hdr.cChanges)
    {
        throw InvalidSVDStreamException("%s: Changes in stream(%#x) does not match from header(%#x)\n", __FUNCTION__,
            ulTotalNumberOfChanges, DirtyBlock->uHdr.Hdr.cChanges);
    }

#ifdef SV_WINDOWS
    if (ullcbChanges != DirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart)
    {
        throw InvalidSVDStreamException("%s: cbChanges in stream(%#I64x) does not match from header(%#I64x)\n", __FUNCTION__,
            ullcbChanges, DirtyBlock->uHdr.Hdr.ulicbChanges.QuadPart);
    }
#else
	{
		if (ullcbChanges != DirtyBlock->uHdr.Hdr.ulicbChanges)
		{
			throw InvalidSVDStreamException("%s: cbChanges in stream(%#PRIu64) does not match from header(%#PRIu64)\n", __FUNCTION__,
				ullcbChanges, DirtyBlock->uHdr.Hdr.ulicbChanges);
		}
	}
#endif

    if (DirtyBlock->uHdr.Hdr.ulFlags & UDIRTY_BLOCK_FLAG_SVD_STREAM)
    {
        if (svdTimeStampFC.TimeInHundNanoSecondsFromJan1601 != DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601)
        {
            throw InvalidSVDStreamException("%s: svdTimeStampFC.TimeInHundNanoSecondsFromJan1601 (%#I64x) is not equal to "
                "TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601(%#I64x)\n",
                __FUNCTION__, svdTimeStampFC.TimeInHundNanoSecondsFromJan1601,
                DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.TimeInHundNanoSecondsFromJan1601);
        }

        if (svdTimeStampLC.TimeInHundNanoSecondsFromJan1601 != DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601)
        {
            throw InvalidSVDStreamException("%s: svdTimeStampLC.TimeInHundNanoSecondsFromJan1601 (%#I64x) is not equal to "
                "TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601(%#I64x)\n",
                __FUNCTION__, svdTimeStampLC.TimeInHundNanoSecondsFromJan1601,
                DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.TimeInHundNanoSecondsFromJan1601);
        }

        if (svdTimeStampFC.SequenceNumber != DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber)
        {
            throw InvalidSVDStreamException("%s: svdTimeStampFC.SequenceNumber (%#I64x) is not equal to "
                "TagList.TagTimeStampOfFirstChange.SequenceNumber(%#I64x)\n",
                __FUNCTION__, svdTimeStampFC.SequenceNumber,
                DirtyBlock->uTagList.TagList.TagTimeStampOfFirstChange.ullSequenceNumber);
        }

        if (svdTimeStampLC.SequenceNumber != DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber)
        {
            throw InvalidSVDStreamException("%s: svdTimeStampLC.SequenceNumber (%#I64x) is not equal to "
                "TagList.TagTimeStampOfLastChange.SequenceNumber(%#I64x)\n",
                __FUNCTION__, svdTimeStampLC.SequenceNumber,
                DirtyBlock->uTagList.TagList.TagTimeStampOfLastChange.ullSequenceNumber);
        }
    }

    pDBTranData->FCSequenceNumberV2.ullSequenceNumber = svdTimeStampFC.SequenceNumber;
    pDBTranData->FCSequenceNumberV2.ullTimeStamp = svdTimeStampFC.TimeInHundNanoSecondsFromJan1601;
    pDBTranData->LCSequenceNumberV2.ullSequenceNumber = svdTimeStampLC.SequenceNumber;
    pDBTranData->LCSequenceNumberV2.ullTimeStamp = svdTimeStampLC.TimeInHundNanoSecondsFromJan1601;
}
