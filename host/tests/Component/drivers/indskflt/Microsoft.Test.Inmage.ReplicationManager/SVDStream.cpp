#include "stdafx.h"
#include "ReplicationManager.h"

//Define for per io time stamp changes
void
ValidateSVDtimeStampOfFirstChangeV2(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP_V2 &svdTimeStamp)
{

	ValidateSVDPrefix(svdPrefix, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2, 0x00, "Time stamp of first change", "SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2");
	ValidateStreamRecTypeTimeStampV2(svdTimeStamp);
}

void
ValidateSVDtimeStampOfFirstChange(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP &svdTimeStamp)
{
	
	ValidateSVDPrefix(svdPrefix, SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE, 0x00, "Time stamp of first change", "SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE");
	ValidateStreamRecTypeTimeStamp(svdTimeStamp);
}

//Define for per io time stamp changes
void
ValidateSVDtimeStampOfLastChangeV2(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP_V2 &svdTimeStamp)
{
	ValidateSVDPrefix(svdPrefix, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2, 0x00, "Time stamp of last change", "SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2");
	ValidateStreamRecTypeTimeStampV2(svdTimeStamp);
}


void
ValidateSVDtimeStampOfLastChange(SVD_PREFIX &svdPrefix, SVD_TIME_STAMP &svdTimeStamp)
{
	ValidateSVDPrefix(svdPrefix, SVD_TAG_TIME_STAMP_OF_LAST_CHANGE, 0x00, "Time stamp of last change", "SVD_TAG_TIME_STAMP_OF_LAST_CHANGE");
	ValidateStreamRecTypeTimeStamp(svdTimeStamp);
}

//Defined for per io time stamp changes
void
ValidateStreamRecTypeTimeStampV2(SVD_TIME_STAMP_V2 &svdTimeStamp)
{
	if (STREAM_REC_TYPE_TIME_STAMP_TAG != svdTimeStamp.Header.usStreamRecType) 
	{
		throw InvalidSVDStreamException("%s: StreamRecType(%hd) does not match STREAM_REC_TYPE_TIME_STAMP_TAG(%hd)\n",
			__FUNCTION__, svdTimeStamp.Header.usStreamRecType, STREAM_REC_TYPE_TIME_STAMP_TAG);
	}

	if (0 != svdTimeStamp.Header.ucFlags) 
	{
		printf("%s: StreamRec Flags(%d) are not zero\n", __FUNCTION__, svdTimeStamp.Header.ucFlags);
	}
}

void
ValidateStreamRecTypeTimeStamp(SVD_TIME_STAMP &svdTimeStamp)
{
	if (STREAM_REC_TYPE_TIME_STAMP_TAG != svdTimeStamp.Header.usStreamRecType) 
	{
		throw InvalidSVDStreamException("%s: StreamRecType(%hd) does not match STREAM_REC_TYPE_TIME_STAMP_TAG(%hd)\n",
			__FUNCTION__, svdTimeStamp.Header.usStreamRecType, STREAM_REC_TYPE_TIME_STAMP_TAG);
	}

	if (0 != svdTimeStamp.Header.ucFlags)
	{
		printf("%s: StreamRec Flags(%d) are not zero\n", __FUNCTION__, svdTimeStamp.Header.ucFlags);
	}

	if (sizeof(SVD_TIME_STAMP) != svdTimeStamp.Header.ucLength) 
	{
		throw InvalidSVDStreamException("%s: StreamRec Length(%d) does not match sizeof SVD_TIME_STAMP(%#d)\n",
			__FUNCTION__, svdTimeStamp.Header.ucLength, sizeof(SVD_TIME_STAMP));
	}
}

//Defined for per io time stamp changes
void
ValidateSVDheaderV2(SVD_PREFIX &svdPrefix, SVD_HEADER_V2 &svdHeader1)
{
	ValidateSVDPrefix(svdPrefix, SVD_TAG_HEADER_V2, 0x00, "SVD_HEADER_V2", "SVD_TAG_HEADER_V2");

	SVD_HEADER_V2     svdHeaderInitialized = { 0 };

	if (memcmp(&svdHeaderInitialized, &svdHeader1, sizeof(SVD_HEADER_V2)) != 0) 
	{
		// TODO: printf("%s: SVD_TAG_HEADER_V2 contains non zero data\n", __FUNCTION__);
		// PrintBinaryData((const unsigned char *)&svdHeader1, sizeof(SVD_HEADER_V2));
	}
}


void
ValidateSVDheader(SVD_PREFIX &svdPrefix, SVD_HEADER1 &svdHeader1)
{
	ValidateSVDPrefix(svdPrefix, SVD_TAG_HEADER1, 0x00, "SVD_HEADER1", "SVD_TAG_HEADER1");

	SVD_HEADER1     svdHeaderInitialized = { 0 };

	// TODO:
	if (memcmp(&svdHeaderInitialized, &svdHeader1, sizeof(SVD_HEADER1)) != 0) 
	{
	//	printf("%s: SVD_HEADER1 contains non zero data\n", __FUNCTION__);
	//	PrintBinaryData((const unsigned char *)&svdHeader1, sizeof(SVD_HEADER1));
	}
}

void
ValidateSVDPrefix(SVD_PREFIX &svdPrefix, SV_ULONG ulTag, SV_ULONG ulFlags, char *StructName, char *TagName)
{
	SVDtag  HeaderTag(ulTag);

	if (svdPrefix.tag != ulTag) 
	{
		SVDtag  HeaderTagInStream(svdPrefix.tag);
		throw InvalidSVDStreamException("%s: SVD Prefix tag %s for %s did not match %s(%s)\n",
			__FUNCTION__, HeaderTag.c_str(), StructName, TagName, HeaderTagInStream.c_str());
	}

	if (0x01 != svdPrefix.count) 
	{
		throw InvalidSVDStreamException("%s: count(%#d) in svdPrefix for tag(%s) is not equal to 0x01\n",
			__FUNCTION__, svdPrefix.count, HeaderTag.c_str());
	}

	if (ulFlags != svdPrefix.Flags) 
	{
	// TODO:	printf("%s: Flags(%#d) in svdPrefix for tag(%s) are not equal to 0x00\n",
	//		__FUNCTION__, svdPrefix.Flags, HeaderTag.c_str());
	}
}

// SVDtag class
// 
SVDtag::SVDtag(SV_ULONG ulTag) : m_ulTag(ulTag)
{
	m_sTag.push_back(char(ulTag & 0xFF));
	m_sTag.push_back(char((ulTag & 0xFF00) >> 8));
	m_sTag.push_back(char((ulTag & 0xFF0000) >> 16));
	m_sTag.push_back(char((ulTag & 0xFF000000) >> 24));
}

SVDtag::~SVDtag()
{

}