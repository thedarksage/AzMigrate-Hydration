/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : compress.h
 *
 * Description: 
 */

#ifndef		COMPRESS__H
#define		COMPRESS__H

#include "zlib.h"
#include <ace/Time_Value.h>
#include "entity.h"

enum COMPRESSION_LEVEL
{
	NO_COMPRESSION		=0,
	BEST_SPEED			=1,
	BEST_COMPRESSION	=2,
	DEFAULT_COMPRESSION	=3

};

 
enum COMPRESSOR
{
	GZIP=0,
	ZLIB=1
};

struct Compress: public Entity
{
public:
	virtual long int CompressFile(
						void* /*In File*/,
						void* /*Out File*/) = 0;
	virtual long int UncompressFile(void*,void*)		= 0;

	virtual long int CompressStreamInit(
						COMPRESSION_LEVEL Level=BEST_COMPRESSION) = 0;
	virtual long int CompressStream(
						unsigned char* pszInBuffer /* Input Buffer */,
						const unsigned long liInBufferLength
						/* Input Buffer Length */) = 0;
	virtual const unsigned char * const CompressStreamFinish(
						unsigned long * /*Output buffer length*/,
                        unsigned long * /*Input buffer length*/) = 0;

	virtual long int UncompressStreamInit() = 0;
	virtual long int UncompressStream(
						unsigned char* /* Input Buffer */,
						const unsigned long /*InBufferLength*/) = 0;
	virtual const unsigned char * const UncompressStreamFinish(
						unsigned long * /*Output Buffer length.*/) = 0;

	virtual const unsigned char * const CompressBuffer(
						unsigned char*,
						const unsigned long,
						unsigned long* /*Output buffer length.*/) = 0;
	virtual const unsigned char * const UncompressBuffer(
						unsigned char*,
						const unsigned long,
						unsigned long*)	= 0;
	virtual const float GetCompressionRatio() const = 0;

	virtual void SetOutputBufferSize(const long int) = 0;
	virtual void SetCompressionLevel(COMPRESSION_LEVEL level) = 0;

	virtual bool LogUncompressedDataIfChkEnabled(
						unsigned char* pszInBuffer /* Input Buffer */,
						const unsigned long liInBufferLength
						/* Input Buffer Length */) = 0;
    virtual ACE_Time_Value GetTimeForCompression(void) = 0;
	Compress() : m_UnCompressedFileName(""),
				 m_bCompressionChecksEnabled(false),
                 m_bProfile(false) {}
public:
	static const  long int DEFAULT_OUT_BUFFER_SIZE;

public:
	std::string m_UnCompressedFileName;
	bool m_bCompressionChecksEnabled;
    bool m_bProfile;
};

class CompressionFactory
{
public:
	static Compress*CreateCompressor(COMPRESSOR);
};

class ZCompress: public Compress //Implement the compression/uncompression interface
{
private:
	void InitZStream();
	int MapCompressionLevel(COMPRESSION_LEVEL level);
	// Hide the methods we don't want to expose
	int Init();

	long int CompressBuffer2(
				unsigned char*,
				unsigned long,
				unsigned char**);
	long int UncompressBuffer2(
				unsigned char*,
				unsigned long,
				unsigned char**);
    void CleanUp(void);
private:
	COMPRESSION_LEVEL m_CompressionLevel;

	float m_fCompressionRatio;
	
	unsigned long int m_liOutputBufferLen;

	z_stream m_Stream;
	
	long int m_liStatus;

	unsigned char* m_pOutputBuffer;

    ACE_Time_Value m_TimeForCompression;

    ACE_Time_Value m_TimeBeforeCompress;

    ACE_Time_Value m_TimeAfterCompress;

public:
	ZCompress();
	~ZCompress();

	virtual void SetCompressionLevel(
						COMPRESSION_LEVEL level);
	
	virtual long int CompressFile(void*,void*);
	virtual long int UncompressFile(void*,void*);

	// Compression level
	virtual long int CompressStreamInit(
						COMPRESSION_LEVEL Level=BEST_COMPRESSION);
	virtual long int CompressStream(
						unsigned char* pszInBuffer /* Input Buffer */,
						const unsigned long liInBufferLength/* Input Buffer Length */);
	virtual const unsigned char * const CompressStreamFinish(
						unsigned long * /*Output buffer length*/,
                        unsigned long * /*Input buffer length*/);

	virtual long int UncompressStreamInit();
	virtual long int UncompressStream(
						unsigned char* /* Input Buffer */,
						const unsigned long /*InBufferLength*/);
	virtual const unsigned char * const UncompressStreamFinish(
						unsigned long * /*Output Buffer length.*/);

	virtual const unsigned char * const CompressBuffer(
						unsigned char*,
						const unsigned long,
						unsigned long* /*Output buffer length.*/);
	virtual const unsigned char * const UncompressBuffer(
						unsigned char*,
						const unsigned long,
						unsigned long*);

	virtual void SetOutputBufferSize(const long int);
	virtual const float GetCompressionRatio() const;
	virtual void ReleaseBuffers();


	virtual bool LogUncompressedDataIfChkEnabled(
						unsigned char* pszInBuffer /* Input Buffer */,
						const unsigned long liInBufferLength
						/* Input Buffer Length */);

    virtual ACE_Time_Value GetTimeForCompression(void);
};

#endif

