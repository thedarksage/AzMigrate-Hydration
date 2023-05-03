#ifndef STREAMENGINE_H
#define STREAMENGINE_H

#include "svtypes.h"
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

	void *pContext;
	bool (*SourceHandlerFn)(void *pCxt, void *pData, unsigned long ulLength);

	char *pStreamDataBuffer;
	unsigned long ulStreamBufferLength;
	unsigned long ulPendingBufferLength;

	unsigned long ulPendingDataLength;

	bool CheckEOS();

public:
	//Constructors
	StreamEngine(void);
	StreamEngine(enum tagStreamRole Role);

	StreamEngine(void *pCxt);
	StreamEngine(void *pCxt, enum tagStreamRole Role);

	StreamEngine(bool (*pHandler)(void *, void *, unsigned long));
	StreamEngine(bool (*pHandler)(void *, void *, unsigned long), enum tagStreamRole Role);

	StreamEngine(bool (*pHandler)(void *, void *, unsigned long), void *pCxt);
	StreamEngine(bool (*pHandler)(void *, void *, unsigned long), void *pCxt, enum tagStreamRole Role);

	void SetHandlerContext(void *pCxt);

	// Check EndOfStream. Returns E_FAIL at EndOfStream, Or returns S_OK.



	~StreamEngine(void);



	virtual enum tagStreamState GetStreamState();
    virtual void SetStreamState(tagStreamState st) { StreamState = st; }

	virtual enum tagStreamRole GetStreamRole();
	virtual bool SetStreamRole(enum tagStreamRole Role);

	virtual enum tagStreamDataSource GetStreamDataSource();
	virtual bool SetStreamDataSource(enum tagStreamDataSource DataSource);

	virtual bool RegisterDataSourceHandler(bool (* pHandler)(void *, void *, unsigned long));
	virtual bool RegisterDataSourceHandler(bool (* pHandler)(void *, void *, unsigned long), void *pCtx);
	virtual bool PushRecordHeader(unsigned short usTag, unsigned long ulDataLength);
	virtual bool PushRecordData(void *pData, unsigned long ulDataLength);
    virtual bool PushRecordHeaderAndData(unsigned short usTag, void *pData, unsigned long ulDataLength);
	virtual bool PushValidStreamRecord(void *pData, unsigned long ulTotalLength);

	virtual bool RegisterDataSourceBuffer(char *pBuffer, unsigned long ulTotalBufferLength);
	virtual bool BuildStreamRecordHeader(unsigned short usTag, unsigned long ulDataLength);
	virtual bool BuildStreamRecordData(void *pData, unsigned long ulDataLength);
	virtual bool BuildStreamRecordHeaderAndData(unsigned short usTag, void *pData, unsigned long ulDataLength);
	virtual bool BuildValidStreamRecord(void *pData, unsigned long ulTotalLength);
	virtual bool GetStreamBufferOffset(unsigned long *pOffset);
	virtual bool GetDataBufferAllocationLength(unsigned long ulDataLength, unsigned long *pOutAllocLength);

	virtual bool PopRecordHeader(unsigned short *pOutTag, unsigned long *pOutDataLength);
	virtual bool PopRecordData(void *pData, unsigned long ulDataLength);
	virtual bool PopRecordHeaderAndData(unsigned short *pOutTag, void *pInDataBuffer, unsigned long InDataLength, unsigned long *pOutStreamDataLength);
	
	virtual bool PeekRecordHeader(unsigned short *pOutTag, unsigned long *pOutDataLength);
	virtual bool PeekRecordData(void *pData, unsigned long ulDataBufferLength, unsigned long ulDataLengthToPeek);
	virtual bool PeekRecordHeaderAndData(unsigned short *pOutTag, void *pInDataBuffer, unsigned long InDataLength, unsigned long *pOutStreamDataLength);

	virtual SVERROR GetRecordHeader(unsigned short *pOutTag, unsigned long *pOutDataLength, unsigned int & dataFormatFlags);
	virtual SVERROR GetRecordData(void *pData, unsigned long ulDataBufferLength, unsigned long ulSourceDataLength);
	virtual SVERROR GetRecordHeaderAndData(unsigned short *pOutTag, void *pInDataBuffer, unsigned long InDataLength, unsigned long *pOutStreamDataLength);

	virtual SVERROR SkipRecordHeader(void);
	virtual SVERROR SkipRecordData(void);
	virtual SVERROR SkipRecordData(unsigned long ulDataLength);
	virtual SVERROR SkipRecordHeaderAndData(void);
	virtual SVERROR SkipRecords(unsigned int NumRecords);

	virtual bool IsEndOfStream(bool *pEOS);
	virtual bool ValidateStreamRecord(void *pData, unsigned long ulTotalLength);
	virtual bool VerifyStreamLength(void* pData, unsigned long expectedLength);

	virtual bool IsInitialized();
	virtual bool IsInitialized(enum tagStreamState State);
	virtual bool IsInitialized(enum tagStreamRole Role);
	virtual bool IsInitialized(enum tagStreamState State, enum tagStreamRole Role);
};

#endif // STREAMENGINE_H
