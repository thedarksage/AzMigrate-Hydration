// StreamEngine.cpp : Defines the entry point for the console application.
//
//#include "stdafx.h"
#include "StreamEngine.h"
#include "VacpUtil.h"
#include <iostream>
#include <cstring>
#include <stdio.h>

#include <stdio.h>
#include "baseconvertor.h"
#include "convertorfactory.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "inmsafecapis.h"

StreamEngine::StreamEngine(void)
{
	StreamState = eStreamUninitialized;
	StreamRole = eStreamRoleUninitialized;
	StreamSource = eStreamDataSourceUninitialized;

	pContext = NULL;
	SourceHandlerFn = (bool (*)(void *, void *, unsigned long))NULL;

	pStreamDataBuffer = NULL;
	ulStreamBufferLength = 0;
	ulPendingBufferLength = 0;

	ulPendingDataLength = 0;
}

StreamEngine::StreamEngine(enum tagStreamRole Role)
{
	StreamEngine();
	SetStreamRole(Role);
}

StreamEngine::StreamEngine(void *pCnt)
{
	StreamEngine();
	SetHandlerContext(pCnt);
}

StreamEngine::StreamEngine(void *pCnt, enum tagStreamRole Role)
{
	StreamEngine();
	SetHandlerContext(pCnt);
	SetStreamRole(Role);
}

StreamEngine::StreamEngine(bool (*pHandler)(void *, void *, unsigned long))
{
	StreamEngine();
	RegisterDataSourceHandler(pHandler);
}

StreamEngine::StreamEngine(bool (*pHandler)(void *, void *, unsigned long), enum tagStreamRole Role)
{
	StreamEngine();
	SetStreamRole(Role);
	RegisterDataSourceHandler(pHandler);
}

StreamEngine::StreamEngine(bool (* pHandler)(void *, void *, unsigned long), void *pCnt)
{
	StreamEngine();
	RegisterDataSourceHandler(pHandler, pCnt);
}

StreamEngine::StreamEngine(bool (* pHandler)(void *, void *, unsigned long), void *pCnt, enum tagStreamRole Role)
{
	StreamEngine();
	SetStreamRole(Role);
	RegisterDataSourceHandler(pHandler, pCnt);
}

StreamEngine::~StreamEngine(void)
{
	
}

void StreamEngine::SetHandlerContext(void *pCtx)
{
	pContext = pCtx;
}

bool StreamEngine::RegisterDataSourceHandler(bool (* pHandler)(void *, void *, unsigned long))
{
	if (pHandler != NULL)
	{
		SourceHandlerFn = pHandler;
		StreamState = eStreamWaitingForHeader;
		StreamSource = eStreamDataSourceHandlerFn;
		return true;
	}
	return false;
}

bool StreamEngine::RegisterDataSourceHandler(bool (* pHandler)(void *, void *, unsigned long), void *pCxt)
{
	if (pHandler != NULL)
	{
		pContext = pCxt;
		SourceHandlerFn = pHandler;
		StreamState = eStreamWaitingForHeader;
		StreamSource = eStreamDataSourceHandlerFn;
		return true;
	}
	return false;
}

bool StreamEngine::PushRecordHeader(unsigned short usTag, unsigned long ulDataLength)
{
	STREAM_REC_HDR_4B Hdr4B;
	STREAM_REC_HDR_8B Hdr8B;
	//HRESULT Status = S_OK;
	bool Status=true;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return false;
	
	ASSERT(ulPendingDataLength == 0);
	
	if (ulDataLength <= ((unsigned long)0xFF - sizeof(STREAM_REC_HDR_4B))) {

		FILL_STREAM_HEADER_4B(&Hdr4B, usTag, (unsigned char)(sizeof(STREAM_REC_HDR_4B) + ulDataLength));
		Status = (*SourceHandlerFn)(pContext, &Hdr4B, sizeof(STREAM_REC_HDR_4B));

	}else {

		ASSERT(ulDataLength <= ((unsigned long)(-1) - sizeof(STREAM_REC_HDR_8B)));
		FILL_STREAM_HEADER_8B(&Hdr8B, usTag, sizeof(STREAM_REC_HDR_8B) + ulDataLength);
		Status = (*SourceHandlerFn)(pContext, &Hdr8B, sizeof(STREAM_REC_HDR_8B));
	}

	if (Status != true) {

		printf("@ LINE %d in FILE %s \n", __LINE__, __FILE__);
		printf("FAILED SourceHandlerFn, Status  = %08X\n", Status );

		return (Status);

	}

	//Change the Stream State to Waiting For Data, if the Data length is not zero.
	//If the length is zero, then we would have written an empty stream header record w/o any data,
	//hence, no need to change the stream state from eStreamWaitingForHeader.
	
	if (ulDataLength != 0) {
		StreamState = eStreamWaitingForData;
		ulPendingDataLength = ulDataLength;
	}

	return (Status);
}

bool StreamEngine::PushRecordData(void *pData, unsigned long ulDataLength)
{
	//HRESULT Status = S_OK;
	bool Status=true;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleGenerator) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return false;

	//ASSERT(StreamState == eStreamWaitingForData);

	if (ulPendingDataLength != ulDataLength) {

		printf("FAILED: Received Invalid Data length, Expected Data Length =0x%x, Actual Data Length\n",ulPendingDataLength, ulDataLength);

		return (false);
	}

	Status = (*SourceHandlerFn)(pContext, pData, ulDataLength);

	if (Status != true) {

		printf("@ LINE %d in FILE %s \n", __LINE__, __FILE__);
		printf("FAILED SourceHandlerFn, Status  = %08X\n", Status );

	} else {
		StreamState = eStreamWaitingForHeader;
		ulPendingDataLength = 0;
	}

	return (Status);
}

bool StreamEngine::PushRecordHeaderAndData(unsigned short usTag, void *pData, unsigned long ulDataLength)
{
	//HRESULT Status = S_OK;
	bool Status=true;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return false;

	Status = PushRecordHeader(usTag, ulDataLength);
	
	ASSERT(Status == true);

	if (ulDataLength != 0) {
		Status = PushRecordData(pData, ulDataLength);
	}

	return (Status);
}

bool StreamEngine::PushValidStreamRecord(void *pData, unsigned long ulTotalLength)
{
	STREAM_REC_HDR_4B *Hdr4B;
	//HRESULT Status = S_OK;
	bool Status=true;
	unsigned long	StreamLength = 0;
	unsigned char ucFlags;
	
	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return false;

	//ASSERT(StreamState == eStreamWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(ulTotalLength >= sizeof(STREAM_REC_HDR_4B));
	ASSERT(pData != NULL);
	
	StreamLength = GET_STREAM_LENGTH(pData);

	if (StreamLength != ulTotalLength) {

		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Found Invalid Stream Length Received, Expected Stream Length = 0x%x, Actual Stream Length = 0x%x\n",StreamLength, ulTotalLength);
		return (false);

	}

	// Lower nibble in ucFlags specifies the mandatory fields supported by the
	// the Stream.
	
	Hdr4B = (PSTREAM_REC_HDR_4B)pData;

	ucFlags = Hdr4B->ucFlags & 0x0f;

	if (ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) {
		//Clear the bit field
		ucFlags &= ~STREAM_REC_FLAGS_LENGTH_BIT;
	}

	// Check if there are any unsupported mandatory bit fields are set.
	if (ucFlags & 0x0f) {

		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Found Invalid Mandatory Bit field set in the Stream Header, ucFlags  = %08X\n", Hdr4B->ucFlags );
		return (false);
	}

	//Write Data to the Stream
	Status = (*SourceHandlerFn)(pContext, pData, ulTotalLength);

	if (Status != true) {

		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED SourceHandlerFn, Status  = %08X\n", Status );
	}

	return (Status);
}

bool StreamEngine::RegisterDataSourceBuffer(char *pBuffer, unsigned long ulTotalBufferLength)
{
	if (pBuffer != NULL && ulTotalBufferLength >0)
	{
		pStreamDataBuffer = pBuffer;
		ulStreamBufferLength = ulTotalBufferLength;
		ulPendingBufferLength = ulTotalBufferLength;
		StreamState = eStreamWaitingForHeader;
		StreamSource = eStreamDataSourceBuffer;
		ulPendingDataLength = 0;
		return true;
	}

	return false;
}

bool StreamEngine::BuildStreamRecordHeader(unsigned short usTag, unsigned long ulDataLength)
{
	//HRESULT Status = S_OK;
	bool Status=true;

	if (CheckEOS() != true)
		return false;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;
	
	ASSERT(ulPendingBufferLength >= ulDataLength);
	ASSERT(ulPendingDataLength == 0);
	
	if (ulDataLength <= ((unsigned long)0xFF - sizeof(STREAM_REC_HDR_4B))) {

		FILL_STREAM_HEADER_4B(&pStreamDataBuffer[0], usTag, (unsigned char)(sizeof(STREAM_REC_HDR_4B) + ulDataLength));
		pStreamDataBuffer += sizeof(STREAM_REC_HDR_4B);
		ulPendingBufferLength -= sizeof(STREAM_REC_HDR_4B);

	}else {

		ASSERT(ulDataLength <= ((unsigned long)(-1) - sizeof(STREAM_REC_HDR_8B)));
		FILL_STREAM_HEADER_8B(&pStreamDataBuffer[0], usTag, sizeof(STREAM_REC_HDR_8B) + ulDataLength);
		pStreamDataBuffer += sizeof(STREAM_REC_HDR_8B);
		ulPendingBufferLength -= sizeof(STREAM_REC_HDR_8B);
	}

	//Change the Stream State to Waiting For Data, if the Data length is not zero.
	//If the length is zero, then we would have written an empty stream header record w/o any data,
	//hence, no need to change the stream state from eStreamWaitingForHeader.
	
	if (ulDataLength != 0) {
		StreamState = eStreamWaitingForData;
		ulPendingDataLength = ulDataLength;
	}

	return (Status);	
}

bool StreamEngine::BuildStreamRecordData(void *pData, unsigned long ulDataLength)
{
	//HRESULT Status = S_OK;
	bool Status=true;

	if (CheckEOS() != true)
		return false;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleGenerator) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;

	//ASSERT(StreamState == eStreamWaitingForData);

	ASSERT(ulPendingBufferLength >= ulDataLength);

	if (ulPendingDataLength != ulDataLength) {

		printf("FAILED: Received Invalid Data length, Expected Data Length =0x%x, Actual Data Length\n",ulPendingDataLength, ulDataLength);
		return (false);
	}

	(void)inm_memcpy_s(&pStreamDataBuffer[0], ulPendingBufferLength, pData, ulDataLength);
	pStreamDataBuffer += ulDataLength;
	ulPendingBufferLength -= ulDataLength;
	
	StreamState = eStreamWaitingForHeader;
	ulPendingDataLength = 0;

	return (Status);
}

bool StreamEngine::BuildStreamRecordHeaderAndData(unsigned short usTag, void *pData, unsigned long ulDataLength)
{
	//HRESULT Status = S_OK;
	bool Status=true;

	if (CheckEOS() != true)
		return false;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;

	Status = PushRecordHeader(usTag, ulDataLength);
	
	ASSERT(Status == true);

	if (ulDataLength != 0) {
		Status = PushRecordData(pData, ulDataLength);
	}

	return (Status);
}

bool StreamEngine::BuildValidStreamRecord(void *pData, unsigned long ulTotalLength)
{
	STREAM_REC_HDR_4B *Hdr4B;
	//HRESULT Status = S_OK;
	bool Status=true;
	unsigned long	StreamLength = 0;
	unsigned char ucFlags;

	if (CheckEOS() != true)
		return false;
	
	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;

	ASSERT(ulPendingBufferLength >= ulTotalLength);
	//ASSERT(StreamState == eStreamWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(ulTotalLength >= sizeof(STREAM_REC_HDR_4B));
	ASSERT(pData != NULL);
	
	StreamLength = GET_STREAM_LENGTH(pData);

	if (StreamLength != ulTotalLength) {

		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Found Invalid Stream Length Received, Expected Stream Length = 0x%x, Actual Stream Length = 0x%x\n",StreamLength, ulTotalLength);
		return (false);
	}

	// Lower nibble in ucFlags specifies the mandatory fields supported by the
	// the Stream.
	
	Hdr4B = (PSTREAM_REC_HDR_4B)pData;

	ucFlags = Hdr4B->ucFlags & 0x0f;

	if (ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) {
		//Clear the bit field
		ucFlags &= ~STREAM_REC_FLAGS_LENGTH_BIT;
	}

	// Check if there are any unsupported mandatory bit fields are set.
	if (ucFlags & 0x0f) {

		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Found Invalid Mandatory Bit field set in the Stream Header, ucFlags  = %08X\n", Hdr4B->ucFlags );
		return (false);
	}

	//Write Data to the Stream Buffer
	(void)inm_memcpy_s(&pStreamDataBuffer[0], ulPendingBufferLength, pData, ulTotalLength);
	pStreamDataBuffer += ulTotalLength;
	ulPendingBufferLength -= ulTotalLength;

	return (Status);
}

bool StreamEngine::SetStreamRole(enum tagStreamRole Role)
{
	StreamRole = Role;
	return true;
}

enum tagStreamRole StreamEngine::GetStreamRole()
{
	return StreamRole;
}

enum tagStreamState StreamEngine::GetStreamState()
{
	return StreamState;
}

bool StreamEngine::GetStreamBufferOffset(unsigned long *pOutLength)
{
	if (pOutLength == NULL)
		return false;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;

	*pOutLength = (ulStreamBufferLength - ulPendingBufferLength);

	return true;
}

bool StreamEngine::GetDataBufferAllocationLength(unsigned long ulDataLength, unsigned long *pOutAllocLength)
{
	if (pOutAllocLength == NULL)
		return false;

	unsigned long pTotalLength = 0;

	if (ulDataLength <= ((unsigned long)0xFF - sizeof(STREAM_REC_HDR_4B))) {
		INM_SAFE_ARITHMETIC(pTotalLength = ulDataLength + InmSafeInt<size_t>::Type(sizeof(STREAM_REC_HDR_4B)), INMAGE_EX(ulDataLength)(sizeof(STREAM_REC_HDR_4B)))
	} else {
		ASSERT(ulDataLength <= ((unsigned long)(-1) - sizeof(STREAM_REC_HDR_8B)));
		INM_SAFE_ARITHMETIC(pTotalLength = ulDataLength + InmSafeInt<size_t>::Type(sizeof(STREAM_REC_HDR_8B)), INMAGE_EX(ulDataLength)(sizeof(STREAM_REC_HDR_8B)))
	}

	*pOutAllocLength = pTotalLength;

	return true;
}

bool StreamEngine::IsInitialized()
{
	if (StreamState == eStreamUninitialized || StreamRole == eStreamRoleUninitialized)
	{
		return false;
	}

	return true;
}

bool StreamEngine::IsInitialized(enum tagStreamState State)
{
	if (IsInitialized() == false)
		return false;

	if (GetStreamState() != State)
		return false;

	return true;
}

bool StreamEngine::IsInitialized(enum tagStreamRole Role)
{
	if (IsInitialized() == false)
		return false;

	if (GetStreamRole() != Role)
		return false;

	return true;
}

bool StreamEngine::IsInitialized(enum tagStreamState State, enum tagStreamRole Role)
{
	if (IsInitialized() == false)
		return false;

	if (GetStreamState() != State || GetStreamRole() != Role)
		return false;

	return true;
}

enum tagStreamDataSource StreamEngine::GetStreamDataSource()
{
	return StreamSource;
}

bool StreamEngine::SetStreamDataSource(enum tagStreamDataSource DataSource)
{
	StreamSource = DataSource;
	return true;
}


//Parser specific functions

bool StreamEngine::PopRecordHeader(unsigned short *pTag, unsigned long *pDataLength)
{
	//HRESULT Status = S_OK;
	bool Status=true;
	STREAM_REC_HDR_4B Hdr4B;
	unsigned long	HeaderLength = 0;
	
	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return false;

	ASSERT(pTag != NULL);
	ASSERT(pDataLength != NULL);

	*pDataLength = 0;

	Status = (*SourceHandlerFn)(pContext, &Hdr4B, sizeof(STREAM_REC_HDR_4B));

	if (Status != true) {
		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED ReadFromStream, Status  = %08X\n", Status );
		return (false);
	}

	HeaderLength = GET_STREAM_HEADER_LENGTH(&Hdr4B);

	CLEAR_STREAM_REC_FLAGS_BIT(&Hdr4B, STREAM_REC_FLAGS_LENGTH_BIT);

	ASSERT((Hdr4B.ucFlags & 0x0f) == 0);

	if (HeaderLength == sizeof(STREAM_REC_HDR_4B)) {
	
		*pDataLength = (unsigned long)Hdr4B.ucLength - sizeof(STREAM_REC_HDR_4B);	
	
	} else if (HeaderLength == sizeof(STREAM_REC_HDR_8B)) {
				
		Status = (*SourceHandlerFn)(pContext, pDataLength, sizeof(unsigned long));

		if (Status != true) {
			printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
			printf("FAILED ReadFromStream, Status  = %08X\n", Status );
			return (false);
		}

		//From the actual stream header length, deduct the size of the 
		//stream header size to get the actual data length.
		*pDataLength -= sizeof(STREAM_REC_HDR_8B);
	}
  
	*pTag = Hdr4B.usStreamRecType;

	//If the stream record is not an empty header, update the stream state.
	if (*pDataLength != 0) {

		StreamState = eStreamWaitingForData;
		ulPendingDataLength = *pDataLength;
	}

	return (Status);
}

bool StreamEngine::PopRecordData(void *pData, unsigned long ulDataLength)
{
	//HRESULT Status = S_OK;
	bool Status=true;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleParser) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return false;

	//ASSERT(StreamState == eStreamParserWaitingForData);
	ASSERT(pData != NULL);
	ASSERT(ulDataLength != 0);

	// If requested data length is more than the length of the stream record data.
	if (ulDataLength > ulPendingDataLength) {
		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Expected Length 0x%x, Actual Length 0x%x", ulPendingDataLength, ulDataLength);

		return (false);
	}

	//read data from the Stream
	Status = (*SourceHandlerFn)(pContext, pData, ulDataLength);

	if (Status != true) {

		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED ReadFromStream, Status  = %08X\n", Status );
		return (false);
	}
	
	ulPendingDataLength -= ulDataLength;

	if (ulPendingDataLength == 0) {
		StreamState = eStreamWaitingForHeader;
	}

	return (Status);
}

bool StreamEngine::PopRecordHeaderAndData(unsigned short *pTag, void *pBuffer, unsigned long TotalBufferLength, unsigned long *pActualDataLength)
{
	//HRESULT Status = S_OK;
	bool Status=true;
	unsigned short CurrentRecordTag;
	unsigned long CurrentRecordDataLength;
	unsigned long	min_len;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return false;

	//ASSERT(StreamState == eStreamParserWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(pTag != NULL);
	
	Status = PopRecordHeader(&CurrentRecordTag, &CurrentRecordDataLength);

	if (Status != true) {
		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED PopRecordHeader, Status  = %08X\n", Status );
		return (false);
	}

	min_len = (TotalBufferLength < CurrentRecordDataLength) ? TotalBufferLength : CurrentRecordDataLength;

	if (min_len != 0) {

		ASSERT(pBuffer != NULL);
		ASSERT(pActualDataLength != (unsigned long *)NULL);

		Status = PopRecordData(pBuffer, min_len);

		if (Status != true) {
			printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
			printf("FAILED GetRecordData, Status  = %08X\n", Status );
			return (false);
		}
	}

	*pTag = CurrentRecordTag;
	*pActualDataLength = CurrentRecordDataLength;

	return (Status);
}

bool StreamEngine::PeekRecordHeader(unsigned short *pTag, unsigned long *pDataLength)
{
	//HRESULT Status = S_OK;
	bool Status = true;
	STREAM_REC_HDR_4B *pHdr4B = NULL;
	unsigned long	HeaderLength = 0;

	if (CheckEOS() != true)
		return false;

	//Store Original StreamDataBuffer pointer
	char *pTmpDataBuffer = pStreamDataBuffer;
	unsigned long ulTmpPendingBufferLength = ulPendingBufferLength;


	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;

	ASSERT(pTag != NULL);
	ASSERT(pDataLength != NULL);

	*pDataLength = 0;

	ASSERT(ulTmpPendingBufferLength >= sizeof(STREAM_REC_HDR_4B));

	pHdr4B = (STREAM_REC_HDR_4B *)pTmpDataBuffer;

	pTmpDataBuffer += sizeof(STREAM_REC_HDR_4B);
	ulTmpPendingBufferLength -= sizeof(STREAM_REC_HDR_4B);

	HeaderLength = GET_STREAM_HEADER_LENGTH(pHdr4B);

	CLEAR_STREAM_REC_FLAGS_BIT(pHdr4B, STREAM_REC_FLAGS_LENGTH_BIT);

	ASSERT((pHdr4B->ucFlags & 0x0f) == 0);

	if (HeaderLength == sizeof(STREAM_REC_HDR_4B)) {
	
		*pDataLength = (unsigned long)pHdr4B->ucLength - sizeof(STREAM_REC_HDR_4B);	
	
	} else if (HeaderLength == sizeof(STREAM_REC_HDR_8B)) {

		ASSERT(ulTmpPendingBufferLength >= sizeof(unsigned long));
				
		(void)inm_memcpy_s(pDataLength, sizeof(unsigned long), pTmpDataBuffer, sizeof(unsigned long));
	
		//From the actual stream header length, deduct the size of the 
		//stream header size to get the actual data length.
		*pDataLength -= sizeof(STREAM_REC_HDR_8B);
	}
  
	*pTag = pHdr4B->usStreamRecType;

	return (Status);

}

bool StreamEngine::PeekRecordData(void *pData, unsigned long ulDataBufferLength, unsigned long ulDataLengthToPeek)
{
	//HRESULT Status = S_OK;
	bool Status=true;

	if (CheckEOS() != true)
		return false;

	//Store Original StreamDataBuffer pointer
	char *pTmpDataBuffer = pStreamDataBuffer;
	unsigned long ulTmpPendingBufferLength = ulPendingBufferLength;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleParser) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;

	//ASSERT(StreamState == eStreamParserWaitingForData);
	ASSERT(pData != NULL);
	ASSERT(ulDataLengthToPeek != 0);

	// If requested data length is more than the length of the stream record data.
	if (ulDataLengthToPeek != ulTmpPendingBufferLength) {
		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Expected Length 0x%x, Actual Length 0x%x", ulPendingDataLength, ulDataLengthToPeek);

		return (false);
	}

	//read data from the Stream
	(void)inm_memcpy_s(pData, ulDataBufferLength, pTmpDataBuffer, ulDataLengthToPeek);

	return (Status);
}

bool StreamEngine::PeekRecordHeaderAndData(unsigned short *pTag, void *pBuffer, unsigned long TotalBufferLength, unsigned long *pActualDataLength)
{
	//HRESULT Status = S_OK;
	bool Status=true;
	unsigned long CurrentRecordDataLength;
	unsigned long	min_len;
	STREAM_REC_HDR_4B *pHdr4B = NULL;
	unsigned long	HeaderLength = 0;

	if (CheckEOS() != true)
		return false;

	//Store Original StreamDataBuffer pointer
	char *pTmpDataBuffer = pStreamDataBuffer;
	unsigned long ulTmpPendingBufferLength = ulPendingBufferLength;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;

	//ASSERT(StreamState == eStreamParserWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(pTag != NULL);

	
	// --------------------
	//  Peek Record Header
	// --------------------

	CurrentRecordDataLength = 0;
	
	ASSERT(ulTmpPendingBufferLength >= sizeof(STREAM_REC_HDR_4B));
	ASSERT(pActualDataLength != (unsigned long *)NULL);

	pHdr4B = (STREAM_REC_HDR_4B *)pTmpDataBuffer;

	pTmpDataBuffer += sizeof(STREAM_REC_HDR_4B);
	ulTmpPendingBufferLength -= sizeof(STREAM_REC_HDR_4B);

	HeaderLength = GET_STREAM_HEADER_LENGTH(pHdr4B);

	CLEAR_STREAM_REC_FLAGS_BIT(pHdr4B, STREAM_REC_FLAGS_LENGTH_BIT);

	ASSERT((pHdr4B->ucFlags & 0x0f) == 0);

	if (HeaderLength == sizeof(STREAM_REC_HDR_4B)) {
	
		CurrentRecordDataLength = (unsigned long)pHdr4B->ucLength - sizeof(STREAM_REC_HDR_4B);	
	
	} else if (HeaderLength == sizeof(STREAM_REC_HDR_8B)) {

		ASSERT(ulTmpPendingBufferLength >= sizeof(unsigned long));
				
		(void)inm_memcpy_s(&CurrentRecordDataLength, sizeof(unsigned long), pTmpDataBuffer, sizeof(unsigned long));

		// Adjust tmpDataBuffer
		pTmpDataBuffer += sizeof(unsigned long);
		ulTmpPendingBufferLength -= sizeof(unsigned long);
	
		//From the actual stream header length, deduct the size of the 
		//stream header size to get the actual data length.
		CurrentRecordDataLength -= sizeof(STREAM_REC_HDR_8B);
	}
  
	*pTag = pHdr4B->usStreamRecType;
	*pActualDataLength = CurrentRecordDataLength;

	// -----------------
	//  Peek Record Data
	// -----------------

	min_len = (TotalBufferLength < CurrentRecordDataLength) ? TotalBufferLength : CurrentRecordDataLength;

	if (min_len > 0) {

		ASSERT(pBuffer != NULL);
		//read data from the Stream
		(void)inm_memcpy_s(pBuffer, TotalBufferLength, pTmpDataBuffer, min_len);
	}

	return (Status);
}

SVERROR StreamEngine::GetRecordHeader(unsigned short *pTag, unsigned long  *pDataLength, unsigned int& dataFormatFlags )
{
	//HRESULT Status = S_OK;
	SVERROR Status= SVS_OK;//true;
	STREAM_REC_HDR_4B Hdr4B;
	unsigned long	HeaderLength = 0;
	
	if (CheckEOS() != true)
		return SVE_FAIL;//false;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return SVE_FAIL;//false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return SVE_FAIL;//false;

	ASSERT(pTag != NULL);
	ASSERT(pDataLength != NULL);

	*pDataLength = 0;

	ASSERT(ulPendingBufferLength >= sizeof(STREAM_REC_HDR_4B));

	(void)inm_memcpy_s(&Hdr4B, sizeof(STREAM_REC_HDR_4B), pStreamDataBuffer, sizeof(STREAM_REC_HDR_4B));
	pStreamDataBuffer += sizeof(STREAM_REC_HDR_4B);
	ulPendingBufferLength -= sizeof(STREAM_REC_HDR_4B);

	HeaderLength = GET_STREAM_HEADER_LENGTH(&Hdr4B);

	CLEAR_STREAM_REC_FLAGS_BIT(&Hdr4B, STREAM_REC_FLAGS_LENGTH_BIT);

	ASSERT((Hdr4B.ucFlags & 0x0f) == 0);
 
        BaseConvertorPtr baseConvertor ;
        ConvertorFactory::getBaseConvertor( dataFormatFlags, baseConvertor ) ;

	if (HeaderLength == sizeof(STREAM_REC_HDR_4B)) {

		INM_SAFE_ARITHMETIC(*pDataLength = (unsigned long)Hdr4B.ucLength - InmSafeInt<size_t>::Type(sizeof(STREAM_REC_HDR_4B)), INMAGE_EX((unsigned long)Hdr4B.ucLength)(sizeof(STREAM_REC_HDR_4B)))
	
	} else if (HeaderLength == sizeof(STREAM_REC_HDR_8B)) {
				

        //printf("The HeaderLength is same as size of STREAM_REC_HDR_8B @LINE %d in FILE %s. Wrong assumption of always 4B\n", 
        //                           LINE_NO, FILE_NAME);
		ASSERT(ulPendingBufferLength >= sizeof(unsigned long));
		(void)inm_memcpy_s(pDataLength, sizeof(unsigned long), pStreamDataBuffer, sizeof(unsigned long));
		pStreamDataBuffer += sizeof(unsigned long);
		ulPendingBufferLength -= sizeof(unsigned long);

		//From the actual stream header length, deduct the size of the 
		//stream header size to get the actual data length.
        *pDataLength = baseConvertor->convertULONG( *pDataLength ) ;
        
		INM_SAFE_ARITHMETIC(*pDataLength -= InmSafeInt<size_t>::Type(sizeof(STREAM_REC_HDR_8B)), INMAGE_EX(*pDataLength)(sizeof(STREAM_REC_HDR_8B)))
	}

    //Hdr4B.usStreamRecType = VacpUtil::Swap(Hdr4B.usStreamRecType);
    Hdr4B.usStreamRecType = baseConvertor->convertUSHORT( Hdr4B.usStreamRecType ) ;

	*pTag = Hdr4B.usStreamRecType;

	//If the stream record is not an empty header, update the stream state.
	if (*pDataLength != 0) {
		StreamState = eStreamWaitingForData;
		ulPendingDataLength = *pDataLength;
	}
	if(ulPendingBufferLength > 0)
	{
		Status = SVS_OK;
	}
	else
	{
		Status = SVS_FALSE;
	}

	return (Status);
}


SVERROR StreamEngine::GetRecordData(void *pData, unsigned long ulDataBufferLength, unsigned long ulSourceDataLength)
{
	//HRESULT Status = S_OK;
	SVERROR Status= SVS_OK;//true;

	if (CheckEOS() != true)
		return SVE_FAIL;//false;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleParser) == false)
		return SVE_FAIL;//false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return SVE_FAIL;//false;
		
	//ASSERT(StreamState == eStreamParserWaitingForData);
	ASSERT(pData != NULL);
	ASSERT(ulSourceDataLength != 0);
	ASSERT(ulPendingBufferLength >= ulSourceDataLength);

	// If requested data length is more than the length of the stream record data.
	if (ulSourceDataLength > ulPendingDataLength) {
		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Expected Length 0x%x, Actual Length 0x%x", ulPendingDataLength, ulSourceDataLength);

		return (SVE_FAIL);
	}
	
	//read data from the Stream
	inm_memcpy_s(pData, ulDataBufferLength, pStreamDataBuffer, ulSourceDataLength);
	pStreamDataBuffer += ulSourceDataLength;
	ulPendingBufferLength -= ulSourceDataLength;

	ulPendingDataLength -= ulSourceDataLength;

	if (ulPendingDataLength == 0) {
		StreamState = eStreamWaitingForHeader;
	}
	if(ulPendingBufferLength > 0)
	{
		Status = SVS_OK;
	}
	else
	{
		Status = SVS_FALSE;
	}
	return (Status);
}

SVERROR StreamEngine::GetRecordHeaderAndData(unsigned short *pTag, void *pBuffer, unsigned long TotalBufferLength, unsigned long *pActualDataLength)
{
	//HRESULT Status = S_OK;
	SVERROR Status= SVS_OK;//true;
	unsigned short CurrentRecordTag;
	unsigned long CurrentRecordDataLength;
	unsigned long	min_len;

	if (CheckEOS() != true)
		return SVE_FAIL;//false;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return SVE_FAIL;//false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return SVE_FAIL;//false;

	//ASSERT(StreamState == eStreamParserWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(pTag != NULL);
	
    /**
    *
    * NEEDTODISCUSS:
    *              For what dataFormatFlags is zero and its effects
    *
    */

    unsigned int dataFormatFlags = 0 ;
	Status = GetRecordHeader(&CurrentRecordTag, &CurrentRecordDataLength, dataFormatFlags);

	if (Status != SVS_OK) {
		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED PopRecordHeader, Status  = %s\n", Status.toString());
		return (SVE_FAIL);
	}

	min_len = (TotalBufferLength < CurrentRecordDataLength) ? TotalBufferLength : CurrentRecordDataLength;

	if (min_len != 0) {

		ASSERT(pBuffer != NULL);
		ASSERT(pActualDataLength != (unsigned long *)NULL);

		Status = GetRecordData(pBuffer, TotalBufferLength, min_len);

		if ((Status != SVS_OK) || (Status != SVS_FALSE)) {
			printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
			printf("FAILED GetRecordData, Status  = %s\n", Status.toString() );
			return (SVE_FAIL);
		}
	}

	*pTag = CurrentRecordTag;
	*pActualDataLength = CurrentRecordDataLength;
	if(ulPendingBufferLength > 0)
	{
		Status = SVS_OK;
	}
	else
	{
		Status = SVS_FALSE;
	}

	return (Status);
}

SVERROR StreamEngine::SkipRecordHeader(void)
{
	//HRESULT Status = S_OK;
	//bool Status=true;
	SVERROR Status = SVS_OK;
	unsigned short CurrentRecordTag;
	unsigned long CurrentRecordDataLength;

	if (CheckEOS() != true)
		return false;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;


    /**
    *
    * NEEDTODISCUSS:
    *              For what dataFormatFlags is zero and its effects
    *
    */

    unsigned int dataFormatFlags = 0 ;
	Status = GetRecordHeader(&CurrentRecordTag, &CurrentRecordDataLength, dataFormatFlags);

	if ((Status != SVS_OK) || (Status != SVS_FALSE)) {
		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED GetRecordHeader(), Status  = %s\n", Status.toString() );
		return (SVE_FAIL);
	}

	return (Status);
}

//Skip Entire RecordData
SVERROR StreamEngine::SkipRecordData(void)
{
	//HRESULT Status = S_OK;
	SVERROR Status = SVS_OK;//true;

	if (CheckEOS() != true)
		return SVE_FAIL;

	if (IsInitialized(eStreamRoleParser) == false)
		return SVE_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return SVE_FAIL;

	if(GetStreamState() == eStreamWaitingForHeader)
	{
		if (ulPendingDataLength != 0)
			return SVE_FAIL;
		else  	// if there is no data record, return simply SUCCESS
			return SVS_FALSE;
	}

	pStreamDataBuffer += ulPendingDataLength;
	ulPendingBufferLength -= ulPendingDataLength;

	ulPendingDataLength = 0;
	StreamState = eStreamWaitingForHeader;
	return (Status);
}

SVERROR StreamEngine::SkipRecordData(unsigned long ulDataLength)
{
	//HRESULT Status = S_OK;
	SVERROR Status = SVS_OK;//true;

	if (CheckEOS() != true)
		return SVE_FAIL;

	// If the ulDataLegnth == 0, then skip the entire record.
	if (ulDataLength == 0)
		return SkipRecordData();

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return SVE_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return SVE_FAIL;

	ASSERT(ulPendingBufferLength >= ulDataLength);

	// If requested data length is more than the length of the stream record data.
	if (ulDataLength > ulPendingDataLength) {
		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Expected Length 0x%x, Actual Length 0x%x", ulPendingDataLength, ulDataLength);

		return (SVE_FAIL);
	}
	
	//read data from the Stream
	pStreamDataBuffer += ulDataLength;
	ulPendingBufferLength -= ulDataLength;

	ulPendingDataLength -= ulDataLength;

	if (ulPendingDataLength == 0) {
		StreamState = eStreamWaitingForHeader;
	}

	return (Status);
}



SVERROR StreamEngine::SkipRecordHeaderAndData(void)
{
	//HRESULT Status = S_OK;
	SVERROR Status= SVS_OK;//true;

	if (CheckEOS() != true)
		return false;

	Status = SkipRecordHeader();

	if ((Status != SVS_OK) || (Status != SVS_FALSE))
		return (SVE_FAIL);

	return (SkipRecordData());

}

SVERROR StreamEngine::SkipRecords(unsigned int NumRecords)
{
	//RESULT Status = S_OK;
	SVERROR Status = SVS_OK;//true;

	if (CheckEOS() != true)
		return SVE_FAIL;

	for (unsigned int index=0; index < NumRecords; index++)
	{
		Status = SkipRecordHeaderAndData();
		if ((Status != SVS_OK) || (Status != SVS_FALSE))
			return (SVE_FAIL);
	}

	return (Status);
}

bool StreamEngine::IsEndOfStream(bool *pEOS)
{
	if (pEOS == NULL)
		return false;

	if (IsInitialized() == false)
		return false;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return false;

	if (ulPendingBufferLength == 0)
		*pEOS = true;
	else
		*pEOS = false;

	return true;
}

bool StreamEngine::CheckEOS()
{
	//HRESULT Status = S_OK;
	bool Status=true;
    
	bool bEOS;

	Status = IsEndOfStream(&bEOS);

	if (Status != true)
		return Status;

	if (bEOS == true)
		return false;

	return Status;
}

//Verify if the expected length of bytes are valid or not
bool StreamEngine::VerifyStreamLength(void* pData, unsigned long expectedLength)
{
	
	bool rv = true;
	unsigned char* pTempData = (unsigned char*)pData;
	unsigned long ActualLen = 0;
	unsigned long PendingLen = expectedLength;
	unsigned long hdrlength =0, datalength = 0;

	while(PendingLen > 0)
	{
		datalength = GET_STREAM_LENGTH(pTempData);
		pTempData += datalength;
		ActualLen += datalength;
		PendingLen -= datalength;
	}

	if(ActualLen != expectedLength)
	{
		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Found Invalid Stream Length Received, Expected Stream Length = 0x%x, Actual Stream Length = 0x%x\n",expectedLength, ActualLen);
		rv = false;
	}
	return rv;
}

bool StreamEngine::ValidateStreamRecord(void *pData, unsigned long ulTotalLength)
{
	STREAM_REC_HDR_4B *Hdr4B;	
	bool Status=true;
	unsigned long	StreamLength = 0;
	unsigned char ucFlags;

	if (pData == NULL)
		return false;

	if (!VerifyStreamLength(pData,ulTotalLength ))
	{
		return (false);
	}

	// Lower nibble in ucFlags specifies the mandatory fields supported by the
	// the Stream.
	
	Hdr4B = (PSTREAM_REC_HDR_4B)pData;

	ucFlags = Hdr4B->ucFlags & 0x0f;

	if (ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) {
		//Clear the bit field
		ucFlags &= ~STREAM_REC_FLAGS_LENGTH_BIT;
	}

	// Check if there are any unsupported mandatory bit fields are set.
	if (ucFlags & 0x0f) {

		printf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		printf("FAILED Found Invalid Mandatory Bit field set in the Stream Header, ucFlags  = %08X\n", Hdr4B->ucFlags );
		return (false);
	}
	return (Status);
}

