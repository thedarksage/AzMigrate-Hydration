/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : compress.cpp
 *
 * Description: 
 */
#include <assert.h>
#include <math.h>

#include "zlib.h"

#include "portableheaders.h"
#include "portablehelpers.h"

#include "entity.h"
#include "compress.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <iostream>
#include <fstream>

const long int Compress::DEFAULT_OUT_BUFFER_SIZE = 4*1024*1024; //4MB

/*
 * FUNCTION NAME : CreateCompressor
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
Compress* CompressionFactory::CreateCompressor(COMPRESSOR CompressLib)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	Compress* pCompress = NULL;
	switch(CompressLib)
	{
		case GZIP:
			pCompress = NULL;
		break;

		case ZLIB:
			pCompress = new ZCompress;
			pCompress = dynamic_cast<ZCompress*>(pCompress);
		break;
			
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return pCompress;
}

/*
 * FUNCTION NAME : ZCompress
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
ZCompress::ZCompress():	m_fCompressionRatio(0),
						m_CompressionLevel(BEST_COMPRESSION),
						m_liOutputBufferLen(DEFAULT_OUT_BUFFER_SIZE),
						m_liStatus(0),
						m_pOutputBuffer(NULL)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : ~ZCompress
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
ZCompress::~ZCompress()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	ReleaseBuffers();
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : Init
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
int ZCompress::Init()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return 0;
}

/*
 * FUNCTION NAME : SetOutputBufferSize
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
void ZCompress::SetOutputBufferSize(const long int liLen)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	if ( liLen > 0 )
		m_liOutputBufferLen = liLen;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : SetCompressionLevel
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
void ZCompress::SetCompressionLevel(COMPRESSION_LEVEL level)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_CompressionLevel = level;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

/*
 * FUNCTION NAME : InitZStream
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
void ZCompress::InitZStream()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_Stream.avail_in	= 0;
	m_Stream.next_in	= NULL;

	m_Stream.avail_out	= 0;
	m_Stream.next_out	= NULL;


	m_Stream.msg		= NULL;
	m_Stream.total_in	= 0;
	m_Stream.total_out	= 0;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	
}

/*
 * FUNCTION NAME : CompressBuffer2
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
long int ZCompress::CompressBuffer2(
		unsigned char* pszInBuffer,
		unsigned long ulInBufferLen,
		unsigned char** pszOutBuffer)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_liStatus = 0;
	m_liOutputBufferLen = compressBound(ulInBufferLen );
	// Zlib usage manual recommends the output buffer should be 0.1 times or 10% greater
	// than the input buffer + 12 bytes.
	INM_SAFE_ARITHMETIC(m_liOutputBufferLen += InmSafeInt<unsigned long>::Type(static_cast<unsigned long>( ceil((m_liOutputBufferLen* (90.0/100.0))) )) + 12, INMAGE_EX(m_liOutputBufferLen))
    *pszOutBuffer = new unsigned char[m_liOutputBufferLen];
	memset(*pszOutBuffer, 0,m_liOutputBufferLen);
	
	unsigned long int uliCompressedSize = m_liOutputBufferLen;

	/*
	 * Comress yields a better compression ratio than compress2 ??????!!
	 * Why ??
	 */
	m_liStatus = compress(*pszOutBuffer,&uliCompressedSize,pszInBuffer,ulInBufferLen);
	if ( Z_BUF_ERROR == m_liStatus )
	{

		INM_SAFE_ARITHMETIC(m_liOutputBufferLen	+=  InmSafeInt<unsigned long>::Type(static_cast<unsigned long>( (m_liOutputBufferLen * (50.0/100.0)) )), INMAGE_EX(m_liOutputBufferLen))
		*pszOutBuffer = (unsigned char*)
			realloc(pszOutBuffer,(m_liOutputBufferLen * sizeof(unsigned char)));
		memset(*pszOutBuffer, 0,m_liOutputBufferLen);
		uliCompressedSize = m_liOutputBufferLen;
		m_liStatus = compress2(
				*pszOutBuffer,
				&uliCompressedSize,
				pszInBuffer,
				ulInBufferLen,
				m_CompressionLevel);
	}
	
	if ( Z_OK == m_liStatus )
	{
		// Compute the compression ratio. 
		float fLen = static_cast<float>( uliCompressedSize );
		if ( fLen < ulInBufferLen )
		{
			m_fCompressionRatio = (ulInBufferLen - fLen)/ulInBufferLen;
			m_fCompressionRatio *= 100.0;
		}
		else
			m_fCompressionRatio = 0;
		
		m_liStatus = uliCompressedSize;
	}
	else
		m_liStatus = 0;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return m_liStatus;

}


/*
 * FUNCTION NAME : UncompressBuffer2
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
long int ZCompress::UncompressBuffer2(
		unsigned char* pInBuffer,
		unsigned long uliInBufferLen,
		unsigned char** pOutBuffer)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_liStatus = 0;

	if (uliInBufferLen > DEFAULT_OUT_BUFFER_SIZE )
		INM_SAFE_ARITHMETIC(m_liOutputBufferLen = InmSafeInt<unsigned long>::Type(uliInBufferLen) * 10, INMAGE_EX(uliInBufferLen))
	else if ( (uliInBufferLen * 10) > DEFAULT_OUT_BUFFER_SIZE )
		INM_SAFE_ARITHMETIC(m_liOutputBufferLen = InmSafeInt<unsigned long>::Type(uliInBufferLen) * 10, INMAGE_EX(uliInBufferLen))
	else
		m_liOutputBufferLen = DEFAULT_OUT_BUFFER_SIZE;


	*pOutBuffer = new unsigned char[m_liOutputBufferLen];
	memset(*pOutBuffer, 0,m_liOutputBufferLen);

	unsigned long int uliUncompressedSize = 0;
	m_liStatus = uncompress(
			*pOutBuffer,
			&uliUncompressedSize,
			pInBuffer,
			uliInBufferLen);
	if (  Z_BUF_ERROR == m_liStatus)
	{
		// Incrememnt buffer len by 50%
		INM_SAFE_ARITHMETIC(m_liOutputBufferLen += InmSafeInt<unsigned long>::Type(static_cast<unsigned long> ( (m_liOutputBufferLen * (50.0/100.0)) )), INMAGE_EX(m_liOutputBufferLen))
		*pOutBuffer = (unsigned char*) 
			realloc (*pOutBuffer,m_liOutputBufferLen * sizeof(unsigned char));
		memset(*pOutBuffer, 0,m_liOutputBufferLen);
		m_liStatus = uncompress(*pOutBuffer,&uliUncompressedSize,pInBuffer,uliInBufferLen);

	}

	if ( Z_OK == m_liStatus )
	{
		// Compute the compression ratio. 
		float  fLen = static_cast<float>( uliUncompressedSize );
		if ( fLen > uliInBufferLen )
		{
			m_fCompressionRatio = (fLen - uliInBufferLen)/fLen;
			m_fCompressionRatio *= 100.0;
		}
		else
			m_fCompressionRatio = 0;
		m_liStatus = uliUncompressedSize;
	}
	else 
		m_liStatus = 0;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return m_liStatus;
}


/*
 * FUNCTION NAME : 
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
const unsigned char * const ZCompress::UncompressBuffer(
		unsigned char* pszInBuffer,
		const unsigned long ulInBufferLen,
		unsigned long* ulOutBufferLen)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	m_liStatus = Z_OK;

	// Sanity checks
	if ( ( 0 == ulInBufferLen ) || ( NULL == pszInBuffer ))
		return NULL;

	InitZStream();

	// set inflate state
	// Let zlib use default memory management routines
	m_Stream.zalloc = Z_NULL;
	m_Stream.zfree = Z_NULL;
	m_Stream.opaque = Z_NULL;

	// Number of available bytes in the input
	m_Stream.avail_in = 0;
	// Location of next available input byte
	m_Stream.next_in = Z_NULL;
	
	m_liStatus = inflateInit(&m_Stream);

	// Next input byte is available at start
	// of inputput buffer
	m_Stream.next_in  = pszInBuffer;
	// Number of available bytes of input is
	// size of input buffer
	m_Stream.avail_in = ulInBufferLen;

	if(NULL != m_pOutputBuffer)
	{
		ReleaseBuffers();
	}

	// Allocate space for output buffer and initialize.
	m_liOutputBufferLen = DEFAULT_OUT_BUFFER_SIZE;
	m_pOutputBuffer = (unsigned char *) malloc(m_liOutputBufferLen);
	memset(m_pOutputBuffer, 0, m_liOutputBufferLen);


	// The uncompressed output data will be available from the start of the out buffer
	m_Stream.next_out = m_pOutputBuffer;
	// Indicate size of output buffer to the zlib structure.
	m_Stream.avail_out = m_liOutputBufferLen;


	// Uncompress or "inflate in zlib's world"
	m_liStatus = inflate(&m_Stream,Z_FINISH);
	if(0 == m_Stream.avail_out)
	{
		do
		{
			//Allocate the percent of log.
			double dCF = log((double)m_liOutputBufferLen);
            unsigned long q;
            INM_SAFE_ARITHMETIC(q = InmSafeInt<unsigned long>::Type(m_liOutputBufferLen)/100, INMAGE_EX(m_liOutputBufferLen))
			dCF *= (double)(q);
			INM_SAFE_ARITHMETIC(m_liOutputBufferLen += InmSafeInt<unsigned long>::Type((unsigned long)dCF), INMAGE_EX(m_liOutputBufferLen)((unsigned long)dCF))
			
			//128mb limit.
			if(m_liOutputBufferLen > (128*1024*1024))
			{
				DebugPrintf(SV_LOG_FATAL, "Unable to allocate memory."
					   " memory allocation crossed 128 MB threshold. Aborting.");
				return NULL;
			}

			m_pOutputBuffer = (unsigned char*) realloc(m_pOutputBuffer, m_liOutputBufferLen);

			if(NULL == m_pOutputBuffer)
			{
				return NULL;
			}

			m_Stream.next_out = m_pOutputBuffer + m_Stream.total_out;
			m_Stream.avail_out = m_liOutputBufferLen - m_Stream.total_out;
			m_liStatus = inflate(&m_Stream, Z_FINISH);
		}while( (0 != m_Stream.avail_in) && (0 == m_Stream.avail_out) );
	}
	// All well; end the uncomress
	if (  Z_STREAM_END == m_liStatus)
		m_liStatus = inflateEnd(&m_Stream);

	assert(m_liStatus != Z_STREAM_ERROR); 

	if ( Z_OK == m_liStatus )
	{ 
		// Set the output to size of the uncompressed output buffer
		*ulOutBufferLen = m_Stream.total_out;

		// Compute the compression ratio. 
		if ( m_Stream.total_out > ulInBufferLen )
		{
			float  fLen = static_cast<float>( m_Stream.total_out );
			m_fCompressionRatio = (fLen - ulInBufferLen)/fLen;
			m_fCompressionRatio *= 100.0;
		}
		else	
		{
			m_fCompressionRatio = 0;
		}
		
	}
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return m_pOutputBuffer;	
}


/*
 * FUNCTION NAME : CompressBuffer
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
const unsigned char * const ZCompress::CompressBuffer(
	unsigned char* pszInBuffer,
	const unsigned long ulInBufferLen,
	unsigned long* ulOutBufferLen)
{

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	m_liStatus = Z_OK;
	// Sanity check
	if ( ( ulInBufferLen <= 0) || (NULL == pszInBuffer))
	{
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
		return NULL;
	}

	InitZStream();
	// set deflate
	m_Stream.zalloc = Z_NULL;
	m_Stream.zfree = Z_NULL;
	m_Stream.opaque = Z_NULL;

	m_Stream.data_type = Z_BINARY;

	// Use best compression
	m_CompressionLevel = BEST_COMPRESSION;
	
	// compress init
    m_liStatus = deflateInit(&m_Stream, m_CompressionLevel);


	//  Return if initialization failed.
	if ( Z_OK != m_liStatus )
	{
		return NULL;
	}

	m_liOutputBufferLen = DEFAULT_OUT_BUFFER_SIZE;
	
	// Initialize the z stream structure with output buffer size
	m_Stream.avail_out = m_liOutputBufferLen;

	if(NULL != m_pOutputBuffer)
	{
		ReleaseBuffers();
	}
	// Allocate memory for output buffer and initialize.
	m_pOutputBuffer = (unsigned char *) malloc( m_liOutputBufferLen);
	memset(m_pOutputBuffer, 0, m_liOutputBufferLen);


	// Initialize z stream structure with input buffer size
	// and location of input buffer
	m_Stream.avail_in = ulInBufferLen;
	m_Stream.next_in = pszInBuffer;
				
	// Initialize z stream structure with location of output buffer
	m_Stream.next_out = m_pOutputBuffer;
	
	// Compress and finish compression in one step
	do
	{
		m_liStatus = deflate(&m_Stream, Z_FINISH );
		if(0 == m_Stream.avail_out)
		{
			do
			{
				//Allocate the percent of log.
				double dCF = log((double)m_liOutputBufferLen);
                unsigned long q;
                INM_SAFE_ARITHMETIC(q = InmSafeInt<unsigned long>::Type(m_liOutputBufferLen)/100, INMAGE_EX(m_liOutputBufferLen))
				dCF *= (double)(q);
				INM_SAFE_ARITHMETIC(m_liOutputBufferLen += InmSafeInt<unsigned long>::Type((unsigned long)dCF), INMAGE_EX(m_liOutputBufferLen)((unsigned long)dCF))
				
				//128mb limit.
				if(m_liOutputBufferLen > (128*1024*1024))
				{
					DebugPrintf(SV_LOG_FATAL, "Unable to allocate memory."
						" memory allocation crossed 128 MB threshold. Aborting.");
					return NULL;
				}
				m_pOutputBuffer = (unsigned char*) realloc(m_pOutputBuffer, m_liOutputBufferLen);

				if(NULL == m_pOutputBuffer)
				{
					return NULL;
				}

				m_Stream.next_out = m_pOutputBuffer + m_Stream.total_out;
				m_Stream.avail_out = m_liOutputBufferLen - m_Stream.total_out;
				m_liStatus = deflate(&m_Stream, Z_FINISH);
			}while( (m_Stream.avail_in != 0) && (m_Stream.avail_out == 0) );
			if(m_liStatus < 0)
			{
				break;
			}
		}
	}while(m_Stream.avail_in > 0);

	// Make sure there are no errors during compression.
	switch(m_liStatus)
	{
		case Z_STREAM_ERROR:
			assert(m_liStatus != Z_STREAM_ERROR);  
		break;
		case Z_DATA_ERROR:
			assert(m_liStatus != Z_DATA_ERROR);  
		break;
		case Z_MEM_ERROR:
			assert(m_liStatus != Z_MEM_ERROR);  
		break;
		case Z_BUF_ERROR:
			assert(m_liStatus != Z_BUF_ERROR);  
		break;
		case Z_VERSION_ERROR:
			assert(m_liStatus != Z_VERSION_ERROR);  
		break;
	}

	// All well and compression successful
	if ( Z_STREAM_END == m_liStatus)
	{
		// Complete compression and ready to return
		// size of compressed buffer.
		m_liStatus	= deflateEnd(&m_Stream);
		*ulOutBufferLen = m_Stream.total_out;

		
		// Calculate compression ratio.
		if ( m_Stream.total_out < ulInBufferLen )
		{
			float  fLen = static_cast<float>( m_Stream.total_out );
			m_fCompressionRatio = (ulInBufferLen - fLen )/ulInBufferLen;
        	m_fCompressionRatio *= 100.0;
		}
		else
		{
			m_fCompressionRatio = 0;
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return m_pOutputBuffer;
}

/*
 * FUNCTION NAME : MapCompressionLevel
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
int ZCompress::MapCompressionLevel(COMPRESSION_LEVEL level)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	
	int iLevel = 0;
	switch (level)
	{
		case NO_COMPRESSION:
			iLevel =  Z_NO_COMPRESSION;
		break;
		case BEST_SPEED:
			iLevel = Z_BEST_SPEED;
		break;
		case BEST_COMPRESSION:
			iLevel =  Z_BEST_COMPRESSION;
		break;
		case DEFAULT_COMPRESSION:
			iLevel =  Z_DEFAULT_COMPRESSION;
		break;
		default:
			iLevel = 0;
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return iLevel;
}

/*
 * FUNCTION NAME : CompressStreamInit
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
long int ZCompress::CompressStreamInit(COMPRESSION_LEVEL Level)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	m_bInitialized	= false;
	m_CompressionLevel = Level;

	InitZStream();
	m_liOutputBufferLen = 0 ; //DEFAULT_OUT_BUFFER_SIZE;

    m_bProfile && (m_TimeForCompression=m_TimeBeforeCompress=m_TimeAfterCompress=0);

	m_liStatus = Z_OK;
	// set deflate
	// Use default memory routines.
	m_Stream.zalloc = Z_NULL;
	m_Stream.zfree = Z_NULL;
	m_Stream.opaque = Z_NULL;

	m_Stream.data_type = Z_BINARY;
	
	// Allocate memory for output buffer and inialize with zeros
	if ( NULL != m_pOutputBuffer)
	{
		ReleaseBuffers();
	}
	// Initialize compression
	m_liStatus = deflateInit2(
		&m_Stream,
		MapCompressionLevel(m_CompressionLevel),
		Z_DEFLATED,
		15+16,
		8,
		Z_DEFAULT_STRATEGY);


	if ( m_liStatus == Z_OK )
		m_bInitialized = true;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return m_liStatus;
}


void ZCompress::CleanUp(void)
{
    if (IsInitialized())
    {
        DebugPrintf(SV_LOG_ERROR, "cleanup of ZCompress because of error\n");
        deflateEnd(&m_Stream);
    }
}


/*
 * FUNCTION NAME : CompressStream
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
long int ZCompress::CompressStream(
	unsigned char* pszInBuffer /* Input Buffer */,
	const unsigned long ulInBufferLength/* Input Buffer Length */)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	m_liStatus = Z_OK;

	if( !IsInitialized() || (NULL == pszInBuffer) || (0 == ulInBufferLength) ) //Sanity Check
	{
        CleanUp();
		return Z_ERRNO;
	}

	bool bLogUnCompressedDataSuccess = LogUncompressedDataIfChkEnabled(pszInBuffer, ulInBufferLength);
	if (false == bLogUnCompressedDataSuccess)
	{
        CleanUp();
		return Z_ERRNO;
	}

    m_bProfile && (m_TimeBeforeCompress=ACE_OS::gettimeofday());

	m_Stream.avail_in = ulInBufferLength;
	m_Stream.next_in = pszInBuffer;

	INM_SAFE_ARITHMETIC(m_liOutputBufferLen += InmSafeInt<unsigned long>::Type(ulInBufferLength), INMAGE_EX(m_liOutputBufferLen)(ulInBufferLength))
	do
	{
        unsigned long q;
        INM_SAFE_ARITHMETIC(q = InmSafeInt<unsigned long>::Type(ulInBufferLength)/2, INMAGE_EX(ulInBufferLength))
		INM_SAFE_ARITHMETIC(m_liOutputBufferLen += InmSafeInt<unsigned long>::Type(q), INMAGE_EX(m_liOutputBufferLen)(q))
		if( m_pOutputBuffer == NULL )
		{   
			m_pOutputBuffer = (unsigned char *) malloc( m_liOutputBufferLen);         
            if (m_pOutputBuffer)
			    memset(m_pOutputBuffer, 0, m_liOutputBufferLen);
		}
		else
		{
			unsigned char* tempBuffer ;
			tempBuffer = (unsigned char *) realloc( m_pOutputBuffer, m_liOutputBufferLen);         
			if( tempBuffer == NULL )
			{
				DebugPrintf(SV_LOG_ERROR, "realloc failed @ LINE %d FILE %s: \n", LINE_NO, FILE_NAME );
                CleanUp();
				return Z_BUF_ERROR ;
			}
			
			m_pOutputBuffer = tempBuffer ;
		}
		if(NULL == m_pOutputBuffer)
		{
            CleanUp();
			DebugPrintf(SV_LOG_ERROR, "malloc failed @ LINE %d FILE %s: \n", LINE_NO, FILE_NAME );
			return Z_BUF_ERROR;
		}

		m_Stream.next_out = m_pOutputBuffer + m_Stream.total_out;
		m_Stream.avail_out = m_liOutputBufferLen - m_Stream.total_out;
		m_liStatus = deflate(&m_Stream, Z_SYNC_FLUSH );				

		if( m_liStatus == Z_BUF_ERROR && m_Stream.avail_in == 0 )
		{
			m_liStatus = Z_OK ;
		}
		if( m_liStatus != Z_OK )
			break ;

	}while( m_Stream.avail_out == 0 );
	
    m_bProfile && (m_TimeAfterCompress=ACE_OS::gettimeofday()) && (m_TimeForCompression+=(m_TimeAfterCompress-m_TimeBeforeCompress));

	// Ensure no errors during compression
	if( m_liStatus != Z_OK ) 
	{
        CleanUp();
		DebugPrintf( SV_LOG_ERROR, "Compress Stream Failed: %s\n", m_Stream.msg ) ;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return m_liStatus;
}


/*
 * FUNCTION NAME : CompressStreamFinish
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
const unsigned char * const ZCompress::CompressStreamFinish(
	unsigned long *ulOutBufferLen,
    unsigned long *ulInBufferLength)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_liStatus = 0;
	if ( !IsInitialized() )
	{
		*ulOutBufferLen = 0 ;
		return NULL;
	}

	m_Stream.avail_in	= 0;
	m_Stream.next_in	= NULL;
	
    m_bProfile && (m_TimeBeforeCompress=ACE_OS::gettimeofday());
	do
	{
		m_Stream.next_out = m_pOutputBuffer + m_Stream.total_out;
		m_Stream.avail_out = m_liOutputBufferLen - m_Stream.total_out;
		m_liStatus = deflate(&m_Stream, Z_FINISH);
		if ((m_liStatus != Z_OK) && (m_liStatus != Z_STREAM_END))
		{
			DebugPrintf(SV_LOG_ERROR, "deflate failed @ LINE %d FILE %s: Stream Error: %s\n", 
					LINE_NO, FILE_NAME, m_Stream.msg );
			break;
		}
		//this is asking for more buffer 
		if( m_liStatus == Z_OK )
		{
            unsigned long q;
            INM_SAFE_ARITHMETIC(q = InmSafeInt<unsigned long>::Type(m_liOutputBufferLen) / 2, INMAGE_EX(m_liOutputBufferLen))
			INM_SAFE_ARITHMETIC(m_liOutputBufferLen += InmSafeInt<unsigned long>::Type(q), INMAGE_EX(m_liOutputBufferLen)(q))
			unsigned char* tempBuffer = ( unsigned char* ) realloc( m_pOutputBuffer, m_liOutputBufferLen ) ;
			if( tempBuffer == NULL )
			{
				m_liStatus = Z_BUF_ERROR ;
				DebugPrintf(SV_LOG_ERROR, "realloc failed @ LINE %d FILE %s: \n", LINE_NO, FILE_NAME );
				break;
			}
			else
				m_pOutputBuffer = tempBuffer ;
		}

	}while(m_liStatus != Z_STREAM_END );//(m_Stream.avail_in != 0) && (m_Stream.avail_out == 0) );

	bool bCompress = true ;
	if (m_liStatus == Z_STREAM_END)
	{
		m_liStatus	= deflateEnd(&m_Stream);
		if( m_liStatus != Z_OK )
		{
			DebugPrintf( SV_LOG_ERROR, "defalateEnd has failed: %s\n", m_Stream.msg ) ;
			bCompress = false ;
		}
	}
	else
    {
		bCompress = false ;
        CleanUp();
    }

    m_bProfile && (m_TimeAfterCompress=ACE_OS::gettimeofday()) && (m_TimeForCompression+=(m_TimeAfterCompress-m_TimeBeforeCompress));
	m_liOutputBufferLen = DEFAULT_OUT_BUFFER_SIZE;
	m_bInitialized = false;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	if( bCompress == true )
	{
		*ulOutBufferLen = m_Stream.total_out;
        ulInBufferLength && (*ulInBufferLength=m_Stream.total_in);
		return m_pOutputBuffer;
	}
	else
	{
		*ulOutBufferLen = 0 ;
		return NULL;
	}
}


ACE_Time_Value ZCompress::GetTimeForCompression(void)
{
    return m_TimeForCompression;
}

/*
 * FUNCTION NAME : UncompressStreamInit
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
long int ZCompress::UncompressStreamInit()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_bInitialized	= false;
	
	InitZStream();
	m_liStatus = 0;
	// set inflate state
	m_Stream.zalloc = Z_NULL;
	m_Stream.zfree = Z_NULL;
	m_Stream.opaque = Z_NULL;
	m_Stream.data_type = Z_BINARY;
	
	//m_liStatus = inflateInit(&m_Stream);
	m_liStatus = inflateInit2(&m_Stream, 15+16);
	m_liOutputBufferLen = DEFAULT_OUT_BUFFER_SIZE;

	if(NULL != m_pOutputBuffer)
	{
		ReleaseBuffers();
	}

	m_pOutputBuffer = (unsigned char *) malloc(m_liOutputBufferLen);
	memset(m_pOutputBuffer, 0, m_liOutputBufferLen);
	m_Stream.next_out = m_pOutputBuffer;

	if ( m_liStatus == Z_OK )
		m_bInitialized = true;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return m_liStatus;
}


/*
 * FUNCTION NAME : UncompressStream
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
long int ZCompress::UncompressStream(
	unsigned char* pszInBuffer/* Input Buffer */,
	const unsigned long ulInBufferLength)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	if ( !IsInitialized() )
		return 0;

	if ( pszInBuffer == NULL )
		return 0;

	m_Stream.next_in  = pszInBuffer;
	m_Stream.avail_in = ulInBufferLength;
	m_Stream.avail_out = m_liOutputBufferLen - m_Stream.total_out;
	
	do
	{
		m_liStatus = inflate(&m_Stream,Z_NO_FLUSH);
		if(0 == m_Stream.avail_out)
		{
			do
			{
				//Allocate the percent of log.
				double dCF = log((double)m_liOutputBufferLen);
                unsigned long q;
                INM_SAFE_ARITHMETIC(q = InmSafeInt<unsigned long>::Type(m_liOutputBufferLen)/100, INMAGE_EX(m_liOutputBufferLen))
				dCF *= (double)(q);
				INM_SAFE_ARITHMETIC(m_liOutputBufferLen += InmSafeInt<unsigned long>::Type((unsigned long)dCF), INMAGE_EX(m_liOutputBufferLen)((unsigned long)dCF))

				//128mb limit.
				if(m_liOutputBufferLen > (128*1024*1024))
				{
					DebugPrintf(SV_LOG_FATAL,
						"Unable to allocate memory. memory allocation"
						" crossed 128 MB threshold. Aborting.");
					return Z_BUF_ERROR;
				}

				m_pOutputBuffer = (unsigned char*) realloc(m_pOutputBuffer, m_liOutputBufferLen);

				if(NULL == m_pOutputBuffer)
				{
					return Z_BUF_ERROR;
				}

				m_Stream.next_out = m_pOutputBuffer + m_Stream.total_out;
				m_Stream.avail_out = m_liOutputBufferLen - m_Stream.total_out;
				m_liStatus = inflate(&m_Stream, Z_NO_FLUSH);
			}while( (m_Stream.avail_in != 0) && (m_Stream.avail_out == 0) );
		}
		if(m_liStatus < 0)
		{
			break;
		}
	} while(m_Stream.avail_in > 0);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
 	return m_liStatus;
}


/*
 * FUNCTION NAME : UncompressStreamFinish
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
const unsigned char * const ZCompress::UncompressStreamFinish(
	unsigned long *ulOutBufferLen)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_liStatus = Z_OK;

	if ( IsInitialized() )
	{
		m_Stream.next_in  = NULL;
		m_Stream.avail_in = 0;
        m_Stream.avail_out = m_liOutputBufferLen - m_Stream.total_out;
		m_liStatus = inflate(&m_Stream,Z_FINISH);
		if(0 == m_Stream.avail_out)
		{
			do
			{
				//Allocate the percent of log.
				double dCF = log((double)m_liOutputBufferLen);
                unsigned long q;
                INM_SAFE_ARITHMETIC(q = InmSafeInt<unsigned long>::Type(m_liOutputBufferLen)/100, INMAGE_EX(m_liOutputBufferLen))
				dCF *= (double)(q);
				INM_SAFE_ARITHMETIC(m_liOutputBufferLen += InmSafeInt<unsigned long>::Type((unsigned long)dCF), INMAGE_EX(m_liOutputBufferLen)((unsigned long)dCF))
				m_pOutputBuffer = (unsigned char*) realloc(m_pOutputBuffer, m_liOutputBufferLen);

				if(NULL == m_pOutputBuffer)
				{
					return NULL;
				}

				m_Stream.next_out = m_pOutputBuffer + m_Stream.total_out;
				m_Stream.avail_out = m_liOutputBufferLen - m_Stream.total_out;
				m_liStatus = inflate(&m_Stream, Z_FINISH);
			}while(m_Stream.avail_out == 0);
		}
	    m_liStatus = inflateEnd(&m_Stream);
		assert(m_liStatus != Z_STREAM_ERROR);
	}

	*ulOutBufferLen = m_Stream.total_out;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return m_pOutputBuffer;
}

/*
 * FUNCTION NAME : CompressFile
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
long int ZCompress::CompressFile(void* pInFile,void* pOutFile)
{
	// Not implemented as yet.!!
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return 0;
}


/*
 * FUNCTION NAME : UnCompressFile
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
long int ZCompress::UncompressFile(void* pInFile,void* pOutFile)
{
	// Not implemented as yet.!!
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return 0;
}

/*
 * FUNCTION NAME : GetCompressionRatio
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
const float ZCompress::GetCompressionRatio() const
{
	// Not implemented as yet.!!
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return m_fCompressionRatio;
}

/*
 * FUNCTION NAME : ReleaseBuffers
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
void ZCompress::ReleaseBuffers()
{
	if(m_pOutputBuffer)
	{
		free(m_pOutputBuffer);
		m_pOutputBuffer = NULL;
	}
}


bool ZCompress::LogUncompressedDataIfChkEnabled(
					unsigned char* pszInBuffer /* Input Buffer */,
					const unsigned long ulInBufferLength
					/* Input Buffer Length */)
{
	bool bRetVal = true;


    if (m_bCompressionChecksEnabled)
	{
		//the  m_UnCompressedFileName will be empty for tags and time stamps
		if ( !m_UnCompressedFileName.empty() )
		{
			//Added ios :: ate for tellp file comparion checks 
            std::ios ::openmode mode = std::ios :: out | std::ios :: binary | std::ios :: app | std::ios :: ate;
			// PR#10815: Long Path support
			std::ofstream localofstream(getLongPathName(m_UnCompressedFileName.c_str()).c_str(),mode);

            if(!localofstream.is_open())
            {
				DebugPrintf(SV_LOG_ERROR, "In Function %s @ LINE %d in FILE %s, failed to open localofstream\n", FUNCTION_NAME, LINE_NO, FILE_NAME );
				bRetVal = false;
            }
			if (true == bRetVal)
			{
				std::streamsize firstPointer = localofstream.tellp() ;

				/* DebugPrintf(SV_LOG_ERROR, 
						"In function %s, @ LINE %d in FILE %s, pszInBuffer = %p, ulInBufferLength = %lu, firstPointer = %d before write."
						"This is not an error.\n", 
						FUNCTION_NAME, LINE_NO, FILE_NAME, pszInBuffer, ulInBufferLength, firstPointer);*/
				localofstream.write( (const char *)pszInBuffer,(std::streamsize) ulInBufferLength );
				if(!localofstream)
				{
					DebugPrintf(SV_LOG_ERROR, "localofstream.write failed @ LINE %d in FILE %s \n", LINE_NO, FILE_NAME );
					localofstream.close();
					bRetVal = false;
				}
				if (true == bRetVal)
				{
					std::streamsize secondPointer = localofstream.tellp() ;

					if( (secondPointer - firstPointer) != ( std::streamsize ) ulInBufferLength )
					{
						DebugPrintf( SV_LOG_ERROR, "In function %s @ LINE %d in FILE %s, (secondPointer = %d - firstPointer = %d) != "
								     "ulInBufferLength = %lu. The state of localofstream is %d\n", FUNCTION_NAME, LINE_NO, FILE_NAME, 
									 secondPointer, firstPointer, ulInBufferLength, localofstream.good()) ;
					}
					localofstream.close();

				    /* DebugPrintf(SV_LOG_ERROR, 
						"In function %s, @ LINE %d in FILE %s, pszInBuffer = %p, ulInBufferLength = %lu, firstPointer = %d after write."
						"secondPointer = %d, secondPointer - firstPointer = %d. This is not an error.\n", FUNCTION_NAME, LINE_NO, FILE_NAME, 
						pszInBuffer, ulInBufferLength, firstPointer, secondPointer, (secondPointer - firstPointer));*/
				}
			}
		} /* End of if (!m_UnCompressedFileName.empty()) */
	}

	return bRetVal;

}

