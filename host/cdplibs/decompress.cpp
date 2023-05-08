//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : decompress.cpp
//
// Description: 
//

#include "decompress.h"
#include "file.h"
#include "cdputil.h"
#include "inmsafeint.h"
#include "inmageex.h"

#include <math.h>
using namespace std;

const int DEFAULT_BUFFER_SIZE = 1048576;

bool GzipUncompress::UnCompress(const char * pszInBuffer, const unsigned long & ulInBufferLen, char ** ppOutBuffer, unsigned long& ulOutBufferLen)
{
	if(!ppOutBuffer)
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
			<< " Invalid parameter (output buffer)\n";

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		return false;
	}

	*ppOutBuffer = NULL;
	ulOutBufferLen = 0;

	if(!pszInBuffer)
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
			<< " Invalid parameter (input buffer)\n";

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		return false;
	}

	unsigned long int liOutputBufferLen = DEFAULT_BUFFER_SIZE;
	long int liStatus = Z_OK;
	char* pOutputBuffer = NULL;
	z_stream zStream;
	zStream.avail_in	= 0;
	zStream.next_in	= Z_NULL;
	zStream.avail_out	= 0;
	zStream.next_out	= Z_NULL;
	zStream.msg		= Z_NULL;
	zStream.total_in	= 0;
	zStream.total_out	= 0;
	zStream.zalloc = Z_NULL;
	zStream.zfree = Z_NULL;
	zStream.opaque = Z_NULL;


	liStatus = inflateInit2(&zStream,15+16);

	if( Z_OK  != liStatus)
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "inflateInit2 returned status " << liStatus << "\n";
		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		return false;
	}

	// Next input byte is available at start of input buffer
	zStream.next_in  = (Bytef*)pszInBuffer;

	// Number of available bytes of input is size of input buffer
	zStream.avail_in = ulInBufferLen;


	// Allocate space for output buffer and initialize.
	pOutputBuffer = (char *) malloc(liOutputBufferLen);

	if(!pOutputBuffer)
		return false;

	if(!pOutputBuffer)
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "malloc failed for size\n"
			<< liOutputBufferLen  << "\n" ;

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		return false;
	}

	memset(pOutputBuffer, 0, liOutputBufferLen);


	// The uncompressed output data will be available from the start of the out buffer
	zStream.next_out = (Bytef*) pOutputBuffer;
	// Indicate size of output buffer to the zlib structure.
	zStream.avail_out = liOutputBufferLen;

	while(0 != zStream.avail_in)
	{

		liStatus = inflate(&zStream, Z_FINISH);

		// inflate returns Z_BUF_ERROR when the output buffer is 
		// full and requires more memory
		if( Z_BUF_ERROR == liStatus ) 
		{
			double dCF = log((double)liOutputBufferLen);
            unsigned long q;
            INM_SAFE_ARITHMETIC(q = InmSafeInt<unsigned long>::Type(liOutputBufferLen)/100, INMAGE_EX(liOutputBufferLen))
			dCF *= (double)(q);
			INM_SAFE_ARITHMETIC(liOutputBufferLen += InmSafeInt<unsigned long>::Type((unsigned long)dCF), INMAGE_EX(liOutputBufferLen)((unsigned long)dCF))

			// zStream.avail_out is remaining free space 
			// expect it to be zero as inflate returned Z_BUF_ERROR
			if(0 != zStream.avail_out)
			{
				stringstream l_stderr;
				l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "cannot perform in memory uncompress as memory required is greater than 256mb\n";

				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
				free(pOutputBuffer);
				pOutputBuffer = NULL;
				return false;
			}

			if(liOutputBufferLen > (256*1024*1024))
			{
				stringstream l_stderr;
				l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "cannot perform in memory uncompress as memory required is greater than 256mb\n";

				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
				free(pOutputBuffer);
				pOutputBuffer = NULL;
				return false;
			}

			char * tmpBuffer = (char*) realloc(pOutputBuffer, liOutputBufferLen);

			if(!tmpBuffer)
			{
				stringstream l_stderr;
				l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "realloc failed for size\n"
					<< liOutputBufferLen  << "\n" ;

				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

				free(pOutputBuffer);
				pOutputBuffer = NULL;
				return false;
			} else
			{
				pOutputBuffer = tmpBuffer;
			}

			zStream.next_out = (Bytef*)(pOutputBuffer + zStream.total_out);
			zStream.avail_out = liOutputBufferLen - zStream.total_out;
		}
		else if(Z_STREAM_END == liStatus )
		{
			if(0 != zStream.avail_in) 
			{
				// commented the below code as inflateReset also resets
				// zStream.total_out parameter used above to find out the
				// position in output buffer


				// for now, we do not support multiple streams 
				// in in memory uncompress
				// and fall back to on disk uncompress

				//int rv = inflateReset(&zStream);
				//if(Z_OK != rv )
				//{
				//	stringstream l_stderr;
				//	l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				//		<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				//		<< "inflateReset returned status " << rv << "\n";

				//	DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
				//	free(pOutputBuffer);
				//	pOutputBuffer = NULL;
				//	return false;
				//}

				stringstream l_stderr;
				l_stderr   << "INFORMATION (can be ignored) :  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
					<< "Found multiple compress streams in single file." << "\n"
					<< "Not supported by in-memory uncompress\n"
					<< "uncompress will be done using on disk routines\n";

				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
				free(pOutputBuffer);
				pOutputBuffer = NULL;
				return false;
			}
		}
		else if( Z_OK != liStatus )
		{
			stringstream l_stderr;
			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "inflate returned status " << liStatus << "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
			free(pOutputBuffer);
			pOutputBuffer = NULL;
			return false;
		}
	} // while ( 0 != zStream.avail_in ) 

	if (  Z_STREAM_END == liStatus)
	{
		liStatus = inflateEnd(&zStream);
	} else
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "inflate returned status " << liStatus << " instead of " << Z_STREAM_END << "\n" ;

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		free(pOutputBuffer);
		pOutputBuffer = NULL;
		return false;
	}


	if ( Z_OK != liStatus )
	{ 
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "inflate returned status " << liStatus << "\n" ;

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		free(pOutputBuffer);
		pOutputBuffer = NULL;
		return false;
	} else
	{
		*ppOutBuffer = pOutputBuffer;
		ulOutBufferLen = zStream.total_out;
	}

	return true;
}



bool GzipUncompress::UnCompress(const char* pszInBuffer,const unsigned long ulInBufferLen, const std::string & path)
{
	unsigned long int liOutputBufferLen = DEFAULT_BUFFER_SIZE;
	z_stream zStream;
	long int liStatus = Z_OK;
	char* pOutputBuffer = NULL;
	int  bytesWritten = 0;

	if(!pszInBuffer)
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
			<< " Invalid parameter (input buffer)\n";

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		return false;
	}

	// open file for writing
	File outputFile(path);
	outputFile.Open(BasicIo::BioWriteAlways |  BasicIo::BioShareRead | BasicIo::BioBinary);
	if ( !outputFile.Good() )
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "Error opening file\n"
			<< "File Name:" << outputFile.GetName()  << "\n"
			<< "Error Code: " << outputFile.LastError() << "\n"
			<< "Error Message: " << Error::Msg(outputFile.LastError()) << "\n"
			<< USER_DEFAULT_ACTION_MSG << "\n";

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		return false;
	}

	zStream.avail_in	= 0;
	zStream.next_in	= Z_NULL;
	zStream.avail_out	= 0;
	zStream.next_out	= Z_NULL;
	zStream.msg		= Z_NULL;
	zStream.total_in	= 0;
	zStream.total_out	= 0;
	zStream.zalloc = Z_NULL;
	zStream.zfree = Z_NULL;
	zStream.opaque = Z_NULL;



	liStatus = inflateInit2(&zStream,15+16);

	if( Z_OK  != liStatus)
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "inflateInit2 returned status " << liStatus << "\n";
		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		outputFile.Close();
		return false;
	}


	zStream.next_in  = (Bytef*)pszInBuffer;
	zStream.avail_in = ulInBufferLen;

	pOutputBuffer = (char *) malloc(liOutputBufferLen);

	if(!pOutputBuffer)
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "malloc failed for size\n"
			<< liOutputBufferLen  << "\n" ;

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

		outputFile.Close();
		return false;
	}

	memset(pOutputBuffer, 0, liOutputBufferLen);

	zStream.next_out = (Bytef*) pOutputBuffer;
	zStream.avail_out = liOutputBufferLen;


	while(0 != zStream.avail_in)
	{
		// The uncompressed output data will be available from the start of the out buffer
		zStream.next_out = (Bytef*)pOutputBuffer;
		// Indicate size of output buffer to the zlib structure.
		zStream.avail_out = liOutputBufferLen;
		// Uncompress or "inflate in zlib's world"
		liStatus = inflate(&zStream,Z_FINISH);


		if( Z_STREAM_END == liStatus || Z_OK == liStatus || Z_BUF_ERROR == liStatus)
		{
			unsigned long bytestowrite = liOutputBufferLen - zStream.avail_out;

			if(bytestowrite)
			{

				bytesWritten = outputFile.Write(pOutputBuffer, bytestowrite);
				if ( !outputFile.Good() || (bytesWritten != bytestowrite) )
				{
					stringstream l_stderr;
					l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
						<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
						<< "Error during write operation\n"
						<< "File Name:" << outputFile.GetName()  << "\n"
						<< " Expected Write Bytes: " << bytestowrite
						<< " Actual Write Bytes: " << bytesWritten << "\n"
						<< "Error Code: " << outputFile.LastError() << "\n"
						<< "Error Message: " << Error::Msg(outputFile.LastError()) << "\n";

					DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
					free(pOutputBuffer);
					pOutputBuffer = NULL;
					outputFile.Close();
					return false;			
				}
			}

		} else
		{
			stringstream l_stderr;
			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "inflate returned status " << liStatus << "\n"
				<< "uncompressed file name: " << outputFile.GetName()  << "\n" ;

			DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
			free(pOutputBuffer);
			pOutputBuffer = NULL;
			outputFile.Close();
			return false;
		}

		if(Z_STREAM_END == liStatus ) 
		{
			if(0 != zStream.avail_in)
			{
				int rv = inflateReset(&zStream);
				if(Z_OK != rv )
				{
					stringstream l_stderr;
					l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
						<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
						<< "inflateReset returned status " << rv << "\n";

					DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
					free(pOutputBuffer);
					pOutputBuffer = NULL;
					outputFile.Close();
					return false;
				}
			}
		}

	} // while ( 0 != zStream.avail_in )

	if (  Z_STREAM_END == liStatus)
	{
		liStatus = inflateEnd(&zStream);
	} else
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "inflate returned status " << liStatus << " instead of " << Z_STREAM_END << "\n" 
			<< "uncompressed file name: " << outputFile.GetName()  << "\n" ;

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		free(pOutputBuffer);
		pOutputBuffer = NULL;
		outputFile.Close();
		return false;
	}


	if ( Z_OK != liStatus )
	{ 
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "inflate returned status " << liStatus << "\n" 
			<< "uncompressed file name: " << outputFile.GetName()  << "\n" ;

		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		free(pOutputBuffer);
		pOutputBuffer = NULL;
		outputFile.Close();
		return false;
	}

	free(pOutputBuffer);
	pOutputBuffer = NULL;
	outputFile.Close();

	return true;	
}

bool GzipUncompress::UnCompress(const std::string & inpath, const std::string & outpath)
{
#if defined(_WIN32)
	gzFile  iFile = gzopen_w(getLongPathName(inpath.c_str()).c_str(), "rb6 ");
#else
	gzFile  iFile = gzopen(inpath.c_str(), "rb6 ");
#endif

	if (NULL == iFile)
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "Error opening file\n"
			<< "File Name:" << inpath  << "\n"
			<< "Error Code: " << errno << "\n";
		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		return false;
	}

	File outFile(outpath);
	outFile.Open(BasicIo::BioWriteAlways |  BasicIo::BioShareRead | BasicIo::BioBinary);
	if ( !outFile.Good() )
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "Error opening file\n"
			<< "File Name:" << outFile.GetName()  << "\n"
			<< "Error Code: " << outFile.LastError() << "\n"
			<< "Error Message: " << Error::Msg(outFile.LastError()) << "\n";
		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

		gzclose(iFile);
		return false;
	}

	std::vector<char> inBuffer(DEFAULT_BUFFER_SIZE);

	long bytesToProcess = 0;
	long bytesProcessed = 0;

	while (true)
	{
		bytesToProcess = gzread(iFile, &inBuffer[0], inBuffer.size());
		if (-1 == bytesToProcess) 
		{

			stringstream l_stderr;
			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error during gzread\n"
				<< "File Name:" << inpath  << "\n"
				<< "Error Code: " << errno << "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

			gzclose(iFile);
			outFile.Close();
			return false;
		}

		if (0 == bytesToProcess) 
		{
			break; // eof
		}

		bytesProcessed = outFile.Write(&inBuffer[0], bytesToProcess);
		if ( !outFile.Good() || (bytesProcessed != bytesToProcess) )
		{
			stringstream l_stderr;
			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error during write operation\n"
				<< "File Name:" << outFile.GetName()  << "\n"
				<< " Expected Write Bytes: " << bytesToProcess
				<< " Actual Write Bytes: " << bytesProcessed << "\n"
				<< "Error Code: " << outFile.LastError() << "\n"
				<< "Error Message: " << Error::Msg(outFile.LastError()) << "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
			gzclose(iFile);
			outFile.Close();
			return false;			
		}
	}

	
	gzclose(iFile);
	outFile.Close();

	return true;
}

bool GzipUncompress::Verify(const std::string & inpath)
{

#if defined(SV_WINDOWS)
	gzFile  iFile = gzopen_w(getLongPathName(inpath.c_str()).c_str(), "rb6 ");
#else
	gzFile  iFile = gzopen(inpath.c_str(), "rb6 ");
#endif

	if (NULL == iFile)
	{
		stringstream l_stderr;
		l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
			<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
			<< "Error opening file\n"
			<< "File Name:" << inpath  << "\n"
			<< "Error Code: " << errno << "\n";
		DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
		return false;
	}

	std::vector<char> inBuffer(DEFAULT_BUFFER_SIZE);

	long bytesToProcess = 0;

	while (true)
	{
		bytesToProcess = gzread(iFile, &inBuffer[0], inBuffer.size());
		if (-1 == bytesToProcess) 
		{

			stringstream l_stderr;
			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error during gzread\n"
				<< "File Name:" << inpath  << "\n"
				<< "Error Code: " << errno << "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

			gzclose(iFile);
			return false;
		}

		if (0 == bytesToProcess) 
		{
			break; // eof
		}
	}

	gzclose(iFile);
	return true;
}
