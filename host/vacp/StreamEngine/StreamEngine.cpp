// StreamEngine.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "StreamEngine.h"
#include "devicefilter.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

extern void inm_printf(const char * format, ...);

StreamEngine::StreamEngine()
{
	StreamState = eStreamUninitialized;
	StreamRole = eStreamRoleUninitialized;
	StreamSource = eStreamDataSourceUninitialized;

	pContext = NULL;
	SourceHandlerFn = (HRESULT (*)(VOID *, VOID *, ULONG))NULL;

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

StreamEngine::StreamEngine(VOID *pCnt)
{
	StreamEngine();
	SetHandlerContext(pCnt);
}

StreamEngine::StreamEngine(VOID *pCnt, enum tagStreamRole Role)
{
	StreamEngine();
	SetHandlerContext(pCnt);
	SetStreamRole(Role);
}

StreamEngine::StreamEngine(HRESULT (*pHandler)(VOID *, VOID *, ULONG))
{
	StreamEngine();
	RegisterDataSourceHandler(pHandler);
}

StreamEngine::StreamEngine(HRESULT (*pHandler)(VOID *, VOID *, ULONG), enum tagStreamRole Role)
{
	StreamEngine();
	SetStreamRole(Role);
	RegisterDataSourceHandler(pHandler);
}

StreamEngine::StreamEngine(HRESULT (* pHandler)(VOID *, VOID *, ULONG), VOID *pCnt)
{
	StreamEngine();
	RegisterDataSourceHandler(pHandler, pCnt);
}

StreamEngine::StreamEngine(HRESULT (* pHandler)(VOID *, VOID *, ULONG), VOID *pCnt, enum tagStreamRole Role)
{
	StreamEngine();
	SetStreamRole(Role);
	RegisterDataSourceHandler(pHandler, pCnt);
}

StreamEngine::~StreamEngine()
{
	
}

void StreamEngine::SetHandlerContext(VOID *pCtx)
{
	pContext = pCtx;
}

HRESULT StreamEngine::RegisterDataSourceHandler(HRESULT (* pHandler)(VOID *, VOID *, ULONG))
{
	if (pHandler != NULL)
	{
		SourceHandlerFn = pHandler;
		StreamState = eStreamWaitingForHeader;
		StreamSource = eStreamDataSourceHandlerFn;
		return S_OK;
	}
	return E_FAIL;
}

HRESULT StreamEngine::RegisterDataSourceHandler(HRESULT (* pHandler)(VOID *, VOID *, ULONG), void *pCxt)
{
	if (pHandler != NULL)
	{
		pContext = pCxt;
		SourceHandlerFn = pHandler;
		StreamState = eStreamWaitingForHeader;
		StreamSource = eStreamDataSourceHandlerFn;
		return S_OK;
	}
	return E_FAIL;
}

HRESULT StreamEngine::PushRecordHeader(USHORT usTag, ULONG ulDataLength)
{
	STREAM_REC_HDR_4B Hdr4B;
	STREAM_REC_HDR_8B Hdr8B;
	HRESULT Status = S_OK;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return E_FAIL;
	
	ASSERT(ulPendingDataLength == 0);
	
	if (ulDataLength <= ((ULONG)0xFF - sizeof(STREAM_REC_HDR_4B))) {

		FILL_STREAM_HEADER_4B(&Hdr4B, usTag, (UCHAR)(sizeof(STREAM_REC_HDR_4B) + ulDataLength));
		Status = (*SourceHandlerFn)(pContext, &Hdr4B, sizeof(STREAM_REC_HDR_4B));

	}else {

		ASSERT(ulDataLength <= (ULONG(-1) - sizeof(STREAM_REC_HDR_8B)));
		FILL_STREAM_HEADER_8B(&Hdr8B, usTag, sizeof(STREAM_REC_HDR_8B) + ulDataLength);
		Status = (*SourceHandlerFn)(pContext, &Hdr8B, sizeof(STREAM_REC_HDR_8B));
	}

	if (Status != S_OK) {

		DebugPrintf("@ LINE %d in FILE %s \n", __LINE__, __FILE__);
		DebugPrintf("FAILED SourceHandlerFn, Status  = %08X\n", Status );

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

HRESULT StreamEngine::PushRecordData(VOID *pData, ULONG ulDataLength)
{
	HRESULT Status = S_OK;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return E_FAIL;

	//ASSERT(StreamState == eStreamWaitingForData);

	if (ulPendingDataLength != ulDataLength) {

		DebugPrintf("FAILED: Received Invalid Data length, Expected Data Length =0x%x, Actual Data Length\n",ulPendingDataLength, ulDataLength);

		return (E_FAIL);
	}

	Status = (*SourceHandlerFn)(pContext, pData, ulDataLength);

	if (Status != S_OK) {

		DebugPrintf("@ LINE %d in FILE %s \n", __LINE__, __FILE__);
		DebugPrintf("FAILED SourceHandlerFn, Status  = %08X\n", Status );

	} else {
		StreamState = eStreamWaitingForHeader;
		ulPendingDataLength = 0;
	}

	return (Status);
}

HRESULT StreamEngine::PushRecordHeaderAndData(USHORT usTag, VOID *pData, ULONG ulDataLength)
{
	HRESULT Status = S_OK;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return E_FAIL;

	Status = PushRecordHeader(usTag, ulDataLength);
	
	ASSERT(Status == S_OK);

	if (ulDataLength != 0) {
		Status = PushRecordData(pData, ulDataLength);
	}

	return (Status);
}

HRESULT StreamEngine::PushValidStreamRecord(VOID *pData, ULONG ulTotalLength)
{
	STREAM_REC_HDR_4B *Hdr4B;
	HRESULT Status = S_OK;
	ULONG	StreamLength = 0;
	UCHAR ucFlags;
	
	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return E_FAIL;

	//ASSERT(StreamState == eStreamWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(ulTotalLength >= sizeof(STREAM_REC_HDR_4B));
	ASSERT(pData != NULL);
	
	StreamLength = GET_STREAM_LENGTH(pData);

	if (StreamLength != ulTotalLength) {

		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Found Invalid Stream Length Received, Expected Stream Length = 0x%x, Actual Stream Length = 0x%x\n",StreamLength, ulTotalLength);
		return (E_FAIL);

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

		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Found Invalid Mandatory Bit field set in the Stream Header, ucFlags  = %08X\n", Hdr4B->ucFlags );
		return (E_FAIL);
	}

	//Write Data to the Stream
	Status = (*SourceHandlerFn)(pContext, pData, ulTotalLength);

	if (Status != S_OK) {

		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED SourceHandlerFn, Status  = %08X\n", Status );
	}

	return (Status);
}

HRESULT StreamEngine::RegisterDataSourceBuffer(char *pBuffer, ULONG ulTotalBufferLength)
{
	if (pBuffer != NULL && ulTotalBufferLength >0)
	{
		pStreamDataBuffer = pBuffer;
		ulStreamBufferLength = ulTotalBufferLength;
		ulPendingBufferLength = ulTotalBufferLength;
		StreamState = eStreamWaitingForHeader;
		StreamSource = eStreamDataSourceBuffer;
		ulPendingDataLength = 0;
		return S_OK;
	}

	return E_FAIL;
}

HRESULT StreamEngine::BuildStreamRecordHeader(USHORT usTag, ULONG ulDataLength)
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;
	
	ASSERT(ulPendingBufferLength >= ulDataLength);
	ASSERT(ulPendingDataLength == 0);
	
	if (ulDataLength <= ((ULONG)0xFF - sizeof(STREAM_REC_HDR_4B))) {

		FILL_STREAM_HEADER_4B(&pStreamDataBuffer[0], usTag, (UCHAR)(sizeof(STREAM_REC_HDR_4B) + ulDataLength));
		pStreamDataBuffer += sizeof(STREAM_REC_HDR_4B);
		ulPendingBufferLength -= sizeof(STREAM_REC_HDR_4B);

	}else {

		ASSERT(ulDataLength <= (ULONG(-1) - sizeof(STREAM_REC_HDR_8B)));
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

HRESULT StreamEngine::BuildStreamRecordData(VOID *pData, ULONG ulDataLength)
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	//ASSERT(StreamState == eStreamWaitingForData);

	ASSERT(ulPendingBufferLength >= ulDataLength);


	
	if (ulPendingDataLength != ulDataLength) {

		DebugPrintf("FAILED: Received Invalid Data length, Expected Data Length =0x%x, Actual Data Length\n",ulPendingDataLength, ulDataLength);
		return (E_FAIL);
	}
	
	(void)inm_memcpy_s(&pStreamDataBuffer[0], ulPendingBufferLength, pData, ulDataLength);
	pStreamDataBuffer += ulDataLength;
	ulPendingBufferLength -= ulDataLength;
	
	StreamState = eStreamWaitingForHeader;
	ulPendingDataLength = 0;

	return (Status);
}

HRESULT StreamEngine::BuildStreamRecordDataForTagGuid(VOID *pData, ULONG ulDataLength)
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	(void)inm_memcpy_s(&pStreamDataBuffer[0], ulPendingBufferLength, pData, ulDataLength);
	pStreamDataBuffer += ulDataLength;
	ulPendingBufferLength -= ulDataLength;
	
	StreamState = eStreamWaitingForHeader;
	ulPendingDataLength -= ulDataLength;

	return (Status);
}


HRESULT StreamEngine::BuildStreamRecordHeaderAndData(USHORT usTag, VOID *pData, ULONG ulDataLength)
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	Status = PushRecordHeader(usTag, ulDataLength);
	
	ASSERT(Status == S_OK);

	if (ulDataLength != 0) {
		Status = PushRecordData(pData, ulDataLength);
	}

	return (Status);
}

HRESULT StreamEngine::BuildValidStreamRecord(VOID *pData, ULONG ulTotalLength)
{
	STREAM_REC_HDR_4B *Hdr4B;
	HRESULT Status = S_OK;
	ULONG	StreamLength = 0;
	UCHAR ucFlags;

	if (CheckEOS() != S_OK)
		return E_FAIL;
	
	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	ASSERT(ulPendingBufferLength >= ulTotalLength);
	//ASSERT(StreamState == eStreamWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(ulTotalLength >= sizeof(STREAM_REC_HDR_4B));
	ASSERT(pData != NULL);
	
	StreamLength = GET_STREAM_LENGTH(pData);

	if (StreamLength != ulTotalLength) {

		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Found Invalid Stream Length Received, Expected Stream Length = 0x%x, Actual Stream Length = 0x%x\n",StreamLength, ulTotalLength);
		return (E_FAIL);
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

		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Found Invalid Mandatory Bit field set in the Stream Header, ucFlags  = %08X\n", Hdr4B->ucFlags );
		return (E_FAIL);
	}

	//Write Data to the Stream Buffer
	(void)inm_memcpy_s(&pStreamDataBuffer[0], ulPendingBufferLength, pData, ulTotalLength);
	pStreamDataBuffer += ulTotalLength;
	ulPendingBufferLength -= ulTotalLength;

	return (Status);
}

HRESULT StreamEngine::SetStreamRole(enum tagStreamRole Role)
{
	StreamRole = Role;
	return S_OK;
}

enum tagStreamRole StreamEngine::GetStreamRole()
{
	return StreamRole;
}

enum tagStreamState StreamEngine::GetStreamState()
{
	return StreamState;
}

HRESULT StreamEngine::GetStreamBufferOffset(ULONG *pOutLength)
{
	if (pOutLength == NULL)
		return E_FAIL;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleGenerator) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	*pOutLength = (ulStreamBufferLength - ulPendingBufferLength);

	return S_OK;
}

HRESULT StreamEngine::GetDataBufferAllocationLength(ULONG ulDataLength, ULONG *pOutAllocLength)
{
	if (pOutAllocLength == NULL)
		return E_FAIL;

	ULONG pTotalLength = 0;

	if (ulDataLength <= ((ULONG)0xFF - sizeof(STREAM_REC_HDR_4B))) {
		INM_SAFE_ARITHMETIC(pTotalLength = ulDataLength + InmSafeInt<size_t>::Type(sizeof(STREAM_REC_HDR_4B)), INMAGE_EX(ulDataLength)(sizeof(STREAM_REC_HDR_4B)))
	} else {
		ASSERT(ulDataLength <= (ULONG(-1) - sizeof(STREAM_REC_HDR_8B)));
		INM_SAFE_ARITHMETIC(pTotalLength = ulDataLength + InmSafeInt<size_t>::Type(sizeof(STREAM_REC_HDR_8B)), INMAGE_EX(ulDataLength)(sizeof(STREAM_REC_HDR_8B)))
	}

	*pOutAllocLength = pTotalLength;

	return S_OK;
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

HRESULT StreamEngine::SetStreamDataSource(enum tagStreamDataSource DataSource)
{
	StreamSource = DataSource;
	return S_OK;
}


//Parser specific functions

HRESULT StreamEngine::PopRecordHeader(USHORT *pTag, ULONG *pDataLength)
{
	HRESULT Status = S_OK;
	STREAM_REC_HDR_4B Hdr4B;
	ULONG	HeaderLength = 0;
	
	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return E_FAIL;

	ASSERT(pTag != NULL);
	ASSERT(pDataLength != NULL);

	*pDataLength = 0;

	Status = (*SourceHandlerFn)(pContext, &Hdr4B, sizeof(STREAM_REC_HDR_4B));

	if (Status != S_OK) {
		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED ReadFromStream, Status  = %08X\n", Status );
		return (E_FAIL);
	}

	HeaderLength = GET_STREAM_HEADER_LENGTH(&Hdr4B);

	CLEAR_STREAM_REC_FLAGS_BIT(&Hdr4B, STREAM_REC_FLAGS_LENGTH_BIT);

	ASSERT((Hdr4B.ucFlags & 0x0f) == 0);

	if (HeaderLength == sizeof(STREAM_REC_HDR_4B)) {
	
		*pDataLength = (ULONG)Hdr4B.ucLength - sizeof(STREAM_REC_HDR_4B);	
	
	} else if (HeaderLength == sizeof(STREAM_REC_HDR_8B)) {
				
		Status = (*SourceHandlerFn)(pContext, pDataLength, sizeof(ULONG));

		if (Status != S_OK) {
			DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
			DebugPrintf("FAILED ReadFromStream, Status  = %08X\n", Status );
			return (E_FAIL);
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

HRESULT StreamEngine::PopRecordData(VOID *pData, ULONG ulDataLength)
{
	HRESULT Status = S_OK;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return E_FAIL;

	//ASSERT(StreamState == eStreamParserWaitingForData);
	ASSERT(pData != NULL);
	ASSERT(ulDataLength != 0);

	// If requested data length is more than the length of the stream record data.
	if (ulDataLength > ulPendingDataLength) {
		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Expected Length 0x%x, Actual Length 0x%x", ulPendingDataLength, ulDataLength);

		return (E_FAIL);
	}

	//read data from the Stream
	Status = (*SourceHandlerFn)(pContext, pData, ulDataLength);

	if (Status != S_OK) {

		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED ReadFromStream, Status  = %08X\n", Status );
		return (E_FAIL);
	}
	
	ulPendingDataLength -= ulDataLength;

	if (ulPendingDataLength == 0) {
		StreamState = eStreamWaitingForHeader;
	}

	return (Status);
}

HRESULT StreamEngine::PopRecordHeaderAndData(USHORT *pTag, VOID *pBuffer, ULONG TotalBufferLength, ULONG *pActualDataLength)
{
	HRESULT Status = S_OK;
	USHORT CurrentRecordTag;
	ULONG CurrentRecordDataLength;
	ULONG	min_len;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceHandlerFn)
		return E_FAIL;

	//ASSERT(StreamState == eStreamParserWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(pTag != NULL);
	
	Status = PopRecordHeader(&CurrentRecordTag, &CurrentRecordDataLength);

	if (Status != S_OK) {
		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED PopRecordHeader, Status  = %08X\n", Status );
		return (E_FAIL);
	}

	min_len = (TotalBufferLength < CurrentRecordDataLength) ? TotalBufferLength : CurrentRecordDataLength;

	if (min_len != 0) {

		ASSERT(pBuffer != NULL);
		ASSERT(pActualDataLength != (ULONG *)NULL);

		Status = PopRecordData(pBuffer, min_len);

		if (Status != S_OK) {
			DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
			DebugPrintf("FAILED GetRecordData, Status  = %08X\n", Status );
			return (E_FAIL);
		}
	}

	*pTag = CurrentRecordTag;
	*pActualDataLength = CurrentRecordDataLength;

	return (Status);
}

HRESULT StreamEngine::PeekRecordHeader(USHORT *pTag, ULONG *pDataLength)
{
	HRESULT Status = S_OK;
	STREAM_REC_HDR_4B *pHdr4B = NULL;
	ULONG	HeaderLength = 0;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	//Store Original StreamDataBuffer pointer
	char *pTmpDataBuffer = pStreamDataBuffer;
	ULONG ulTmpPendingBufferLength = ulPendingBufferLength;


	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

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
	
		*pDataLength = (ULONG)pHdr4B->ucLength - sizeof(STREAM_REC_HDR_4B);	
	
	} else if (HeaderLength == sizeof(STREAM_REC_HDR_8B)) {

		ASSERT(ulTmpPendingBufferLength >= sizeof(ULONG));
				
		(void)inm_memcpy_s(pDataLength, sizeof(ULONG), pTmpDataBuffer, sizeof(ULONG));
	
		//From the actual stream header length, deduct the size of the 
		//stream header size to get the actual data length.
		*pDataLength -= sizeof(STREAM_REC_HDR_8B);
	}
  
	*pTag = pHdr4B->usStreamRecType;

	return (Status);

}

HRESULT StreamEngine::PeekRecordData(VOID *pData, ULONG ulDataBufferLength, ULONG ulDataLengthToPeek)
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	//Store Original StreamDataBuffer pointer
	char *pTmpDataBuffer = pStreamDataBuffer;
	ULONG ulTmpPendingBufferLength = ulPendingBufferLength;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	//ASSERT(StreamState == eStreamParserWaitingForData);
	ASSERT(pData != NULL);
	ASSERT(ulDataLengthToPeek != 0);

	// If requested data length is more than the length of the stream record data.
	if (ulDataLengthToPeek != ulTmpPendingBufferLength) {
		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Expected Length 0x%x, Actual Length 0x%x", ulPendingDataLength, ulDataLengthToPeek);

		return (E_FAIL);
	}

	//read data from the Stream	
	(void)inm_memcpy_s(pData, ulDataBufferLength, pTmpDataBuffer, ulDataLengthToPeek);

	return (Status);
}

HRESULT StreamEngine::PeekRecordHeaderAndData(USHORT *pTag, VOID *pBuffer, ULONG TotalBufferLength, ULONG *pActualDataLength)
{
	HRESULT Status = S_OK;
	ULONG CurrentRecordDataLength;
	ULONG	min_len;
	STREAM_REC_HDR_4B *pHdr4B = NULL;
	ULONG	HeaderLength = 0;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	//Store Original StreamDataBuffer pointer
	char *pTmpDataBuffer = pStreamDataBuffer;
	ULONG ulTmpPendingBufferLength = ulPendingBufferLength;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	//ASSERT(StreamState == eStreamParserWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(pTag != NULL);

	
	// --------------------
	//  Peek Record Header
	// --------------------

	CurrentRecordDataLength = 0;
	
	ASSERT(ulTmpPendingBufferLength >= sizeof(STREAM_REC_HDR_4B));
	ASSERT(pActualDataLength != (ULONG *)NULL);

	pHdr4B = (STREAM_REC_HDR_4B *)pTmpDataBuffer;

	pTmpDataBuffer += sizeof(STREAM_REC_HDR_4B);
	ulTmpPendingBufferLength -= sizeof(STREAM_REC_HDR_4B);

	HeaderLength = GET_STREAM_HEADER_LENGTH(pHdr4B);

	CLEAR_STREAM_REC_FLAGS_BIT(pHdr4B, STREAM_REC_FLAGS_LENGTH_BIT);

	ASSERT((pHdr4B->ucFlags & 0x0f) == 0);

	if (HeaderLength == sizeof(STREAM_REC_HDR_4B)) {
	
		CurrentRecordDataLength = (ULONG)pHdr4B->ucLength - sizeof(STREAM_REC_HDR_4B);	
	
	} else if (HeaderLength == sizeof(STREAM_REC_HDR_8B)) {

		ASSERT(ulTmpPendingBufferLength >= sizeof(ULONG));
				
		(void)inm_memcpy_s(&CurrentRecordDataLength, sizeof(ULONG), pTmpDataBuffer, sizeof(ULONG));

		// Adjust tmpDataBuffer
		pTmpDataBuffer += sizeof(ULONG);
		ulTmpPendingBufferLength -= sizeof(ULONG);
	
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

HRESULT StreamEngine::GetRecordHeader(USHORT *pTag, ULONG *pDataLength)
{
	HRESULT Status = S_OK;
	STREAM_REC_HDR_4B Hdr4B;
	ULONG	HeaderLength = 0;
	
	if (CheckEOS() != S_OK)
		return E_FAIL;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

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

	if (HeaderLength == sizeof(STREAM_REC_HDR_4B)) {
	
		INM_SAFE_ARITHMETIC(*pDataLength = (ULONG)Hdr4B.ucLength - InmSafeInt<size_t>::Type(sizeof(STREAM_REC_HDR_4B)),	INMAGE_EX(Hdr4B.ucLength)(sizeof(STREAM_REC_HDR_4B)))
	
	} else if (HeaderLength == sizeof(STREAM_REC_HDR_8B)) {
				
		ASSERT(ulPendingBufferLength >= sizeof(ULONG));
		(void)inm_memcpy_s(pDataLength, sizeof(ULONG), pStreamDataBuffer, sizeof(ULONG));
		pStreamDataBuffer += sizeof(ULONG);
		ulPendingBufferLength -= sizeof(ULONG);

		//From the actual stream header length, deduct the size of the 
		//stream header size to get the actual data length.
		INM_SAFE_ARITHMETIC(*pDataLength -= InmSafeInt<size_t>::Type(sizeof(STREAM_REC_HDR_8B)), INMAGE_EX(*pDataLength)(sizeof(STREAM_REC_HDR_8B)))
	}

	*pTag = Hdr4B.usStreamRecType;

	//If the stream record is not an empty header, update the stream state.
	if (*pDataLength != 0) {
		StreamState = eStreamWaitingForData;
		ulPendingDataLength = *pDataLength;
	}

	return (Status);
}

HRESULT StreamEngine::GetRecordData(VOID *pData, ULONG ulDataBufferLength, ULONG ulSourceDataLength)
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	if (IsInitialized(eStreamWaitingForData, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	//ASSERT(StreamState == eStreamParserWaitingForData);
	ASSERT(pData != NULL);
	ASSERT(ulSourceDataLength != 0);
	ASSERT(ulPendingBufferLength >= ulSourceDataLength);

	// If requested data length is more than the length of the stream record data.
	if (ulSourceDataLength > ulPendingDataLength) {
		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Expected Length 0x%x, Actual Length 0x%x", ulPendingDataLength, ulSourceDataLength);

		return (E_FAIL);
	}
	
	//read data from the Stream
	inm_memcpy_s(pData, ulDataBufferLength, pStreamDataBuffer, ulSourceDataLength);
	pStreamDataBuffer += ulSourceDataLength;
	ulPendingBufferLength -= ulSourceDataLength;

	ulPendingDataLength -= ulSourceDataLength;

	if (ulPendingDataLength == 0) {
		StreamState = eStreamWaitingForHeader;
	}

	return (Status);
}

HRESULT StreamEngine::GetRecordHeaderAndData(USHORT *pTag, VOID *pBuffer, ULONG TotalBufferLength, ULONG *pActualDataLength)
{
	HRESULT Status = S_OK;
	USHORT CurrentRecordTag;
	ULONG CurrentRecordDataLength;
	ULONG	min_len;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	//ASSERT(StreamState == eStreamParserWaitingForHeader);
	ASSERT(ulPendingDataLength == 0);
	ASSERT(pTag != NULL);
	
	Status = GetRecordHeader(&CurrentRecordTag, &CurrentRecordDataLength);

	if (Status != S_OK) {
		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED PopRecordHeader, Status  = %08X\n", Status );
		return (E_FAIL);
	}

	min_len = (TotalBufferLength < CurrentRecordDataLength) ? TotalBufferLength : CurrentRecordDataLength;

	if (min_len != 0) {

		ASSERT(pBuffer != NULL);
		ASSERT(pActualDataLength != (ULONG *)NULL);

		Status = GetRecordData(pBuffer, TotalBufferLength, min_len);

		if (Status != S_OK) {
			DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
			DebugPrintf("FAILED GetRecordData, Status  = %08X\n", Status );
			return (E_FAIL);
		}
	}

	*pTag = CurrentRecordTag;
	*pActualDataLength = CurrentRecordDataLength;

	return (Status);
}

HRESULT StreamEngine::SkipRecordHeader()
{
	HRESULT Status = S_OK;
	USHORT CurrentRecordTag;
	ULONG CurrentRecordDataLength;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	Status = GetRecordHeader(&CurrentRecordTag, &CurrentRecordDataLength);

	if (Status != S_OK) {
		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED GetRecordHeader(), Status  = %08X\n", Status );
		return (E_FAIL);
	}

	return (Status);
}

//Skip Entire RecordData
HRESULT StreamEngine::SkipRecordData()
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	if (IsInitialized(eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	if(GetStreamState() == eStreamWaitingForHeader)
	{
		if (ulPendingDataLength != 0)
			return E_FAIL;
		else  	// if there is no data record, return simply SUCCESS
			return S_OK;
	}

	pStreamDataBuffer += ulPendingDataLength;
	ulPendingBufferLength -= ulPendingDataLength;

	ulPendingDataLength = 0;
	StreamState = eStreamWaitingForHeader;
	return (Status);
}

HRESULT StreamEngine::SkipRecordData(ULONG ulDataLength)
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	// If the ulDataLegnth == 0, then skip the entire record.
	if (ulDataLength == 0)
		return SkipRecordData();

	if (IsInitialized(eStreamWaitingForHeader, eStreamRoleParser) == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	ASSERT(ulPendingBufferLength >= ulDataLength);

	// If requested data length is more than the length of the stream record data.
	if (ulDataLength > ulPendingDataLength) {
		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Expected Length 0x%x, Actual Length 0x%x", ulPendingDataLength, ulDataLength);

		return (E_FAIL);
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



HRESULT StreamEngine::SkipRecordHeaderAndData()
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	Status = SkipRecordHeader();

	if (Status != S_OK)
		return (Status);

	return (SkipRecordData());

}

HRESULT StreamEngine::SkipRecords(UINT NumRecords)
{
	HRESULT Status = S_OK;

	if (CheckEOS() != S_OK)
		return E_FAIL;

	for (unsigned int index=0; index < NumRecords; index++)
	{
		Status = SkipRecordHeaderAndData();
		if (Status != S_OK)
			return (Status);
	}

	return (Status);
}

HRESULT StreamEngine::IsEndOfStream(bool *pEOS)
{
	if (pEOS == NULL)
		return E_FAIL;

	if (IsInitialized() == false)
		return E_FAIL;

	if (GetStreamDataSource() != eStreamDataSourceBuffer)
		return E_FAIL;

	if (ulPendingBufferLength == 0)
		*pEOS = true;
	else
		*pEOS = false;

	return S_OK;
}

HRESULT StreamEngine::CheckEOS()
{
	HRESULT Status = S_OK;
    
	bool bEOS;

	Status = IsEndOfStream(&bEOS);

	if (Status != S_OK)
		return Status;

	if (bEOS == true)
		return E_FAIL;

	return Status;
}
//Verify if the expected length of bytes are valid or not
bool StreamEngine::VerifyStreamLength(void* pData, ULONG expectedLength)
{
	bool rv = true;
	unsigned char* pTempData = (unsigned char*)pData;
	ULONG ActualLen = 0;
	ULONG PendingLen = expectedLength;
	ULONG hdrlength =0, datalength = 0;

	while(PendingLen > 0)
	{
		datalength = GET_STREAM_LENGTH(pTempData);
		pTempData += datalength;
		ActualLen += datalength;
		PendingLen -= datalength;
	}

	if(ActualLen != expectedLength)
	{
		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Found Invalid Stream Length Received, Expected Stream Length = 0x%x, Actual Stream Length = 0x%x\n",expectedLength, ActualLen);
		rv = false;
	}
	return rv;
}

HRESULT StreamEngine::ValidateStreamRecord(void *pData, ULONG ulTotalLength)
{

	STREAM_REC_HDR_4B *Hdr4B;
	HRESULT Status = S_OK;
	ULONG	StreamLength = 0;
	UCHAR ucFlags;

	//if (pData == NULL || ulTotalLength == 0)
	if (pData == NULL)//zero size records are valid
		return E_FAIL;

	//StreamLength = GET_STREAM_LENGTH(pData);
	if (!VerifyStreamLength(pData,ulTotalLength ))
	{
		return (E_FAIL);
	}

	// Lower nibble in ucFlags specifies the mandatory fields supported by the
	// the Stream.
	
	Hdr4B = (PSTREAM_REC_HDR_4B)pData;

	ucFlags = Hdr4B->ucFlags & 0x0f;

	if (ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) {
		//Clear the bit field
		ucFlags &= ~STREAM_REC_FLAGS_LENGTH_BIT;
	}

	//Check if the following block is required or not.
	// Check if there are any unsupported mandatory bit fields are set.
	if (ucFlags & 0x0f) {

		DebugPrintf("@ LINE %d in FILE %s \n",__LINE__, __FILE__);
		DebugPrintf("FAILED Found Invalid Mandatory Bit field set in the Stream Header, ucFlags  = %08X\n", Hdr4B->ucFlags );
		return (E_FAIL);
	}
	return (Status);
}
