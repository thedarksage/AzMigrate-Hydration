#pragma once

#include "StreamRecords.h"

enum tagStreamState {
	eStreamUninitialized = 0,		//Stream is Un initialized
	eStreamWaitingForHeader = 1,	//Stream is waiting for the Stream Record Header
	eStreamWaitingForData =2		//Stream is waiting for the Stream Data
};

enum tagStreamRole {
	eStreamRoleUninitialized = 0,	
	eStreamRoleGenerator = 1,	
	eStreamRoleParser =2		
};

enum tagStreamDataSource {
	eStreamDataSourceUninitialized = 0,
	eStreamDataSourceHandlerFn = 1,	
	eStreamDataSourceBuffer =2		
};

class StreamEngine {

private:

	enum tagStreamState StreamState;
	enum tagStreamRole StreamRole;
	enum tagStreamDataSource StreamSource;

	VOID *pContext;
	HRESULT (*SourceHandlerFn)(VOID *pCxt, VOID *pData, ULONG ulLength);

	char *pStreamDataBuffer;
	ULONG ulStreamBufferLength;
	ULONG ulPendingBufferLength;

	ULONG ulPendingDataLength;

	HRESULT CheckEOS();

public:
	//Constructors
	StreamEngine();
	StreamEngine(enum tagStreamRole Role);

	StreamEngine(VOID *pCxt);
	StreamEngine(VOID *pCxt, enum tagStreamRole Role);

	StreamEngine(HRESULT (*pHandler)(VOID *, VOID *, ULONG));
	StreamEngine(HRESULT (*pHandler)(VOID *, VOID *, ULONG), enum tagStreamRole Role);

	StreamEngine(HRESULT (*pHandler)(VOID *, VOID *, ULONG), VOID *pCxt);
	StreamEngine(HRESULT (*pHandler)(VOID *, VOID *, ULONG), VOID *pCxt, enum tagStreamRole Role);

	void SetHandlerContext(VOID *pCxt);

	// Check EndOfStream. Returns E_FAIL at EndOfStream, Or returns S_OK.



	~StreamEngine();



	virtual enum tagStreamState GetStreamState();
	virtual void SetStreamState(tagStreamState st) { StreamState = st; }

	virtual enum tagStreamRole GetStreamRole();
	virtual HRESULT SetStreamRole(enum tagStreamRole Role);

	virtual enum tagStreamDataSource GetStreamDataSource();
	virtual HRESULT SetStreamDataSource(enum tagStreamDataSource DataSource);

	virtual HRESULT RegisterDataSourceHandler(HRESULT (* pHandler)(VOID *, VOID *, ULONG));
	virtual HRESULT RegisterDataSourceHandler(HRESULT (* pHandler)(VOID *, VOID *, ULONG), VOID *pCtx);
	virtual HRESULT PushRecordHeader(USHORT usTag, ULONG ulDataLength);
	virtual HRESULT PushRecordData(VOID *pData, ULONG ulDataLength);
    virtual HRESULT PushRecordHeaderAndData(USHORT usTag, VOID *pData, ULONG ulDataLength);
	virtual HRESULT PushValidStreamRecord(VOID *pData, ULONG ulTotalLength);

	virtual HRESULT RegisterDataSourceBuffer(char *pBuffer, ULONG ulTotalBufferLength);
	virtual HRESULT BuildStreamRecordHeader(USHORT usTag, ULONG ulDataLength);
	virtual HRESULT BuildStreamRecordDataForTagGuid(VOID *pData, ULONG ulDataLength);
	virtual HRESULT BuildStreamRecordData(VOID *pData, ULONG ulDataLength);
	virtual HRESULT BuildStreamRecordHeaderAndData(USHORT usTag, VOID *pData, ULONG ulDataLength);
	virtual HRESULT BuildValidStreamRecord(VOID *pData, ULONG ulTotalLength);
	virtual HRESULT GetStreamBufferOffset(ULONG *pOffset);
	virtual HRESULT GetDataBufferAllocationLength(ULONG ulDataLength, ULONG *pOutAllocLength);

	virtual HRESULT PopRecordHeader(USHORT *pOutTag, ULONG *pOutDataLength);
	virtual HRESULT PopRecordData(VOID *pData, ULONG ulDataLength);
	virtual HRESULT PopRecordHeaderAndData(USHORT *pOutTag, VOID *pInDataBuffer, ULONG InDataLength, ULONG *pOutStreamDataLength);
	
	virtual HRESULT PeekRecordHeader(USHORT *pOutTag, ULONG *pOutDataLength);
	virtual HRESULT PeekRecordData(VOID *pData, ULONG ulDataBufferLength, ULONG ulDataLengthToPeek);
	virtual HRESULT PeekRecordHeaderAndData(USHORT *pOutTag, VOID *pInDataBuffer, ULONG InDataLength, ULONG *pOutStreamDataLength);

	virtual HRESULT GetRecordHeader(USHORT *pOutTag, ULONG *pOutDataLength);
	virtual HRESULT GetRecordData(VOID *pData, ULONG ulDataBufferLength, ULONG ulSourceDataLength);
	virtual HRESULT GetRecordHeaderAndData(USHORT *pOutTag, VOID *pInDataBuffer, ULONG InDataLength, ULONG *pOutStreamDataLength);

	virtual HRESULT SkipRecordHeader();
	virtual HRESULT SkipRecordData();
	virtual HRESULT SkipRecordData(ULONG ulDataLength);
	virtual HRESULT SkipRecordHeaderAndData();
	virtual HRESULT SkipRecords(UINT NumRecords);

	virtual HRESULT IsEndOfStream(bool *pEOS);
	virtual HRESULT ValidateStreamRecord(void *pData, ULONG ulTotalLength);
	bool VerifyStreamLength(void* pData, ULONG expectedLength);

	virtual bool IsInitialized();
	virtual bool IsInitialized(enum tagStreamState State);
	virtual bool IsInitialized(enum tagStreamRole Role);
	virtual bool IsInitialized(enum tagStreamState State, enum tagStreamRole Role);
};