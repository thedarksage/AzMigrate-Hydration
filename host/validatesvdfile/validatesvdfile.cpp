//
// validatesvdfile.cpp: validate SV files
//

//
// BUGBUG:
//  - Non-MS Windows compilations only:
//      When reading data with a volume offset greater than 4294967296 the volume offset
//      displayed is actually (offset % 4294967296).  The data is still intact, the number
//      is just too large for the ANSI C printf to display.
//     
//      so added hex print values for them to at least get full hex value
//

#ifdef SV_UNIX
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <sstream>
#include <map>
#include <iterator>
#include <algorithm>
#include "svdparse.h"
#include "portable.h"
#include "zlib.h"
#include "validatesvdfile.h"
#include "portablehelpers.h"
#include "inmsafecapis.h"

#ifdef SV_WINDOWS
#include <atlbase.h>
#endif

#include "svdconvertor.h"
#include "hashconvertor.h"
#include "convertorfactory.h"

#include "inm_md5.h"
#include "boost/shared_array.hpp"

/* logging callback start */

#define FORMAT_MSG_BUFF_SIZE    1024

#define VALIDATESVD_LOG_ALWAYS   7

#ifdef WIN32
#define VSNPRINTF(buf, size, maxCount, format, args) _vsnprintf_s(buf, size, maxCount, format, args)
#else
#define VSNPRINTF(buf, size, maxCount, format, args) inm_vsnprintf_s(buf, size, format, args)
#endif

static boost::function<void(unsigned int loglevel, const char *msg)> s_svdCheckLogCallback = 0;

/* logging callback end */

bool IsTso(const char *fileName);

void PrintStats(DIFF_STATS *pDiffStats, const bool &bShouldPrint);

int OnHashCompareInfo( 
                      void* in , 
                      const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                      bool bIsCompressedFile, 
                      HashConvertorPtr hashConvertor
                     );

int OnHeader(
             void *in, 
             const unsigned long size, 
             const unsigned long flags, 
             const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
             bool bIsCompressedFile
            );

int OnHeaderV2(
               void *in, 
               const unsigned long size, 
               const unsigned long flags, 
               const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
               bool bIsCompressedFile
              );

int OnDirtyBlocksData(
                      void *in, 
                      const unsigned long size, 
                      const unsigned long flags, 
                      const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                      DIFF_STATS *pDiffStats, 
                      bool bIsCompressedFile,
                      SVDConvertorPtr svdConvertor, 
                      Checksums_t* checksums
                     );

//Added by BSR for new file formats generated for the project: source io time stamps
int OnDirtyBlocksDataV2( 
                        void *in, 
                        const unsigned long size, 
                        const unsigned long flags, 
                        const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                        DIFF_STATS *pDiffStats,
                        bool bIsCompressedFile, 
                        SV_UINT *ptimeDelta, 
                        SV_UINT *psequenceDelta, 
                        SVDConvertorPtr svdConvertor, 
                        Checksums_t* checksums
                       ); 

int OnDirtyBlocks(
                  void *in, 
                  const unsigned long size, 
                  const unsigned long flags, 
                  const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                  bool bIsCompressedFile,
                  SVDConvertorPtr svdConvertor
                 );

int OnDRTDChanges(
                  void *in, 
                  const unsigned long size, 
                  const unsigned long flags, 
                  const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                  bool bIsCompressedFile,
                  SVDConvertorPtr svdConvertor 
                 );

int OnTimeStampInfo(
                    void *in, 
                    const unsigned long size, 
                    const unsigned long tag, 
                    const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                    bool bIsCompressedFile, 
                    SVDConvertorPtr svdConvertor,
                    SV_ULONGLONG *OutTSFCorTSLC,
                    SV_ULONGLONG *OutSeqFCorLC
                   );

//Added by BSR for new file formats generated for the project: source io time stamps
int OnTimeStampInfoV2( 
                      void *in, 
                      const unsigned long size, 
                      const unsigned long tag, 
                      const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                      bool bIsCompressedFile,
                      SV_ULONGLONG *OutTSFCorTSLC, 
                      SV_ULONGLONG *OutSeqFCorLC, 
                      SVDConvertorPtr svdConvertor 
                     ) ;

const char* getToken( const char* temp, SV_ULONGLONG& value ) ;

int OnUDT(
          void *in, 
          const unsigned long size, 
          const unsigned long flags, 
          const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
          bool bIsCompressedFile,
          SVDConvertorPtr svdConvertori
         );

int OnHashCompareData(
                      void *in, 
                      unsigned long size, 
                      unsigned long flags, 
                      const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                      bool bIsCompressedFile, 
                      SV_UINT dataFormatFlags
                     );

//Wrapper functions to handle compressed(.dat.gz) and uncompressed files(.dat)
bool    IsCompressedFile (const char *fname);
void * fopen_wrapper    (const char *fname, char *mode, bool bIsCompressedFile);
int    fclose_wrapper   (void *fpcontext, bool bIsCompressedFile);
size_t fread_wrapper    (void *buf, size_t size, size_t nitems, void *fpcontext, bool bIsCompressedFile);
size_t fwrite_wrapper   (const void *buf, size_t size, size_t nitems, void *fpcontext, bool bIsCompressedFile);
long   ftell_wrapper    (void *fpcontext, bool bIsCompressedFile);
int    fseek_wrapper    (void *fpcontext, long offset, int mode, bool bIsCompressedFile);
int    gzseek_end       (gzFile *gzfp, long offset, bool bIsCompressedFile);

//Function to validate time stamps and sequence numbers from diff file
int validateTimestampsAndSequenceNos( 
                                     bool bIsDirtyBlockV2, 
                                     bool bIsTso, 
                                     stTimeStampsAndSeqNos_t *ptsandseqnosfilename, 
                                     stTimeStampsAndSeqNos_t *ptsandseqnosfile,
	                                 const stCmdLineAndPrintOpts_t *pCLineAndPrOpts,
                                     SV_UINT timeDelta, 
                                     SV_UINT sequenceDelta
                                    );

void StandardPrintf(const bool &bShouldPrint, FILE *fp, char *format, ...);
E_FILE_TYPE ParseFileName(const char *fileName, stTimeStampsAndSeqNos_t *ptsandseqnos);

//Compressed file extension
#define GZIP_FILE_EXTN    ".gz"

int validateTimestampsAndSequenceNos( 
                                     bool bIsDirtyBlockV2, 
                                     bool bIsTso,
                                     stTimeStampsAndSeqNos_t *ptsandseqnosfilename, 
                                     stTimeStampsAndSeqNos_t *ptsandseqnosfile,
	                                 const stCmdLineAndPrintOpts_t *pCLineAndPrOpts,
                                     SV_UINT timeDelta, 
                                     SV_UINT sequenceDelta
                                    )
{
    int retVal = 1 ;

    if (0 == ptsandseqnosfile->startTimeStamp)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "ERROR: timestamp of first change is zero\n");
        retVal = 0;
    }

    if (0 == ptsandseqnosfile->lastTimeStamp)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "ERROR: timestamp of last change is zero\n");
        retVal = 0;
    }

    if( ptsandseqnosfile->startTimeStamp > ptsandseqnosfile->lastTimeStamp )
	{
		StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "timestampOfFirstChange: " ULLSPEC "is greater than lastTimeStamp" ULLSPEC"\n", 
			ptsandseqnosfile->startTimeStamp,
		    ptsandseqnosfile->lastTimeStamp ) ;
		retVal = 0 ;
	}

    if (ptsandseqnosfilename->startTimeStamp && 
        (ptsandseqnosfile->startTimeStamp != ptsandseqnosfilename->startTimeStamp))
    {
		StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "timestampOfFirstChange: " ULLSPEC " does not match same in filename: " ULLSPEC"\n", 
			ptsandseqnosfile->startTimeStamp,
		    ptsandseqnosfilename->startTimeStamp);
		retVal = 0 ;
    }

    if (ptsandseqnosfile->lastTimeStamp != ptsandseqnosfilename->lastTimeStamp)
    {
		StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "timestampOfLastChange: " ULLSPEC " does not match same in filename: " ULLSPEC"\n", 
			ptsandseqnosfile->lastTimeStamp,
		    ptsandseqnosfilename->lastTimeStamp);
		retVal = 0 ;
    }

	if (true == bIsDirtyBlockV2)
	{
       	if( ptsandseqnosfile->lastSequenceNumber < ptsandseqnosfile->startSequenceNumber )
    	{
    		StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "lastSequenceNumber: " ULLSPEC "is less than startSequenceNumber" ULLSPEC "\n", 
    				ptsandseqnosfile->lastSequenceNumber,
    				ptsandseqnosfile->startSequenceNumber ) ; 
    		retVal = 0 ;
    	}

    	if((!bIsTso) && 
           (ptsandseqnosfile->lastSequenceNumber != ( ptsandseqnosfile->startSequenceNumber + sequenceDelta ) ) )
    	{
    		StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, 
					"lastSequenceNumber" ULLSPEC "is less than the sum of startSequenceNumber and last sequenceDelta" ULLSPEC "\n", 
					ptsandseqnosfile->lastSequenceNumber, 
					ptsandseqnosfile->startSequenceNumber + sequenceDelta ) ;
    		retVal = 0 ;
    	}

	
    	if ((!bIsTso) &&
           (ptsandseqnosfile->lastTimeStamp != ( ptsandseqnosfile->startTimeStamp + timeDelta ) ) )
    	{
    		StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, 
					"lastTimeStamp" ULLSPEC "is less than the sum of startTimeStamp and last timeDelta" ULLSPEC "\n", 
					ptsandseqnosfile->lastTimeStamp, ptsandseqnosfile->startTimeStamp + timeDelta ) ;
    		retVal = 0 ;
    	}

        if (ptsandseqnosfilename->startSequenceNumber && 
            (ptsandseqnosfile->startSequenceNumber != ptsandseqnosfilename->startSequenceNumber))
        {
    		StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "startSequenceNumber: " ULLSPEC " does not match same in filename: " ULLSPEC"\n", 
    			ptsandseqnosfile->startSequenceNumber,
    		    ptsandseqnosfilename->startSequenceNumber);
    		retVal = 0 ;
        }

        if (ptsandseqnosfile->lastSequenceNumber != ptsandseqnosfilename->lastSequenceNumber)
        {
    		StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "lastSequenceNumber: " ULLSPEC " does not match same in filename: " ULLSPEC"\n", 
    			ptsandseqnosfile->lastSequenceNumber,
    		    ptsandseqnosfilename->lastSequenceNumber);
    		retVal = 0 ;
        }

	} /* End of if (true == bIsDirtyBlockV2) */

    return retVal;
}

// ValidateSVStream (void *in)
//  Scans an SV file stream to ensure proper formatting
//  Parameter:
//      void *in     file stream pointer to be verified
//  Return Value:
//      1               On succesful completion
//      0               On detecting a problem with the file stream
//
int ValidateSVStream(
    void *in,
    const char *fileName,
    const stCmdLineAndPrintOpts_t *pclinepropts,
    stTimeStampsAndSeqNos_t *pTsAndSeqnosRead,
    Checksums_t* checksums
    )
{
    bool        eof = false;
    size_t         readin = 0;
    int         chunkerr = 1;
    int         retVal = 0;
    char        *tagname = NULL;
    SVD_PREFIX  prefix = { 0 };
    SV_ULONGLONG timeStampOfFirstChange = 0;
    SV_ULONGLONG timeStampOfLastChange = 0;
    SV_ULONGLONG sequenceNumberOfFirstChange = 0;
    SV_ULONGLONG sequenceNumberOfLastChange = 0;
    SV_UINT timeDelta = 0;
    SV_UINT sequenceDelta = 0;
    DIFF_STATS diffstat;
    stCmdLineAndPrintOpts_t cLineAndPrOpts;
    const stCmdLineAndPrintOpts_t *pCLineAndPrOpts = pclinepropts ? (pclinepropts) : (&cLineAndPrOpts);
    bool bIsCompressedFile = IsCompressedFile(fileName);

    //diffstat.m_fileName = fileName;
    diffstat.m_ullWrUnitLen = pCLineAndPrOpts->m_ullWrUnitLen;
    diffstat.m_ullOverlapLen = pCLineAndPrOpts->m_ullOverlapLen;
    

    bool bIsDirtyBlockV2 = false;
    SVDConvertorPtr svdConvertor;
    SV_UINT dataFormatFlags;
    readin = fread_wrapper((char*)&dataFormatFlags, 1, sizeof(SV_UINT), in, bIsCompressedFile);
    if (readin == sizeof(SV_UINT))
    {
        ConvertorFactory::getSVDConvertor(dataFormatFlags, svdConvertor);
        if (ConvertorFactory::isOldSVDFile(dataFormatFlags) == true)
        {
            fseek_wrapper(in, 0, SEEK_SET, bIsCompressedFile);
        }
    }
    // Scan through the file one chunk at a time
    while (!eof)
    {
        // Read the chunk prefix
        /* NOTE: The record size and record number fields have intentionally    *
        *   been swapped.  This way, we can distinguish between a full prefix   *
        *   being read (result = sizeof (prefix)), an end-of-file marker in the *
        *   middle of the prefix (0 < result < sizeof (prefix), the only error  *
        *   condition), and a proper end-of-file at the end of the last chunk   *
        *   (result = 0)                                                        */
        readin = fread_wrapper(&prefix, 1, sizeof(prefix), in, bIsCompressedFile);

        if (0 == readin)
        {
            retVal = 1;
            eof = true;
        }
        else if (sizeof(prefix) != readin)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "Couldn't read prefix from file %s\n", fileName);
            retVal = 0;
            eof = true;
        }
        else
        {
            // Now that we have the prefix, determine the tag and branch accordingly
            svdConvertor->convertPrefix(prefix);

            switch (prefix.tag)
            {
                // File header
            case SVD_TAG_HEADER1:
                chunkerr = OnHeader(in, prefix.count, prefix.Flags, pCLineAndPrOpts, bIsCompressedFile);
                tagname = "SVD1";
                break;

                /* TODO: Please uncomment this when support for SVD2 is added
                case SVD_TAG_HEADER_V2:
                chunkerr = OnHeaderV2 (in, prefix.count, prefix.Flags, pCLineAndPrOpts, bIsCompressedFile);
                tagname = "SVD2";
                break;
                */

                // Dirty block with data
            case SVD_TAG_DIRTY_BLOCK_DATA_V2:
                bIsDirtyBlockV2 = true;
                chunkerr = OnDirtyBlocksDataV2(
                    in,
                    prefix.count,
                    prefix.Flags,
                    pCLineAndPrOpts,
                    &diffstat,
                    bIsCompressedFile,
                    &timeDelta,
                    &sequenceDelta,
                    svdConvertor,
                    checksums
                    );
                tagname = "DDV2";
                break;

            case SVD_TAG_DIRTY_BLOCK_DATA:
                chunkerr = OnDirtyBlocksData(
                    in,
                    prefix.count,
                    prefix.Flags,
                    pCLineAndPrOpts,
                    &diffstat,
                    bIsCompressedFile,
                    svdConvertor,
                    checksums
                    );
                tagname = "DRTD";
                break;

                // Dirty block
            case SVD_TAG_DIRTY_BLOCKS:
                chunkerr = OnDirtyBlocks(
                    in,
                    prefix.count,
                    prefix.Flags,
                    pCLineAndPrOpts,
                    bIsCompressedFile,
                    svdConvertor
                    );
                tagname = "DIRT";
                break;

                //Time Stamp First Change
            case SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2:
                bIsDirtyBlockV2 = true;
                chunkerr = OnTimeStampInfoV2(
                    in,
                    prefix.count,
                    prefix.tag,
                    pCLineAndPrOpts,
                    bIsCompressedFile,
                    &timeStampOfFirstChange,
                    &sequenceNumberOfFirstChange,
                    svdConvertor
                    );
                tagname = "TFV2";
                break;

            case SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE:
                chunkerr = OnTimeStampInfo(
                    in,
                    prefix.count,
                    prefix.tag,
                    pCLineAndPrOpts,
                    bIsCompressedFile,
                    svdConvertor,
                    &timeStampOfFirstChange,
                    &sequenceNumberOfFirstChange
                    );
                tagname = "TOFC";
                break;

                //Time Stamp Last Change
            case SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2:
                bIsDirtyBlockV2 = true;
                chunkerr = OnTimeStampInfoV2(
                    in,
                    prefix.count,
                    prefix.tag,
                    pCLineAndPrOpts,
                    bIsCompressedFile,
                    &timeStampOfLastChange,
                    &sequenceNumberOfLastChange,
                    svdConvertor
                    );
                tagname = "TLV2";
                break;

            case SVD_TAG_TIME_STAMP_OF_LAST_CHANGE:
                chunkerr = OnTimeStampInfo(
                    in,
                    prefix.count,
                    prefix.tag,
                    pCLineAndPrOpts,
                    bIsCompressedFile,
                    svdConvertor,
                    &timeStampOfLastChange,
                    &sequenceNumberOfLastChange
                    );
                tagname = "TOLC";
                break;

                //DRTD Changes
            case SVD_TAG_LENGTH_OF_DRTD_CHANGES:
                chunkerr = OnDRTDChanges(
                    in,
                    prefix.count,
                    prefix.Flags,
                    pCLineAndPrOpts,
                    bIsCompressedFile,
                    svdConvertor
                    );
                tagname = "LODC";
                break;

                //User Defined Tag
            case SVD_TAG_USER:
                chunkerr = OnUDT(
                    in,
                    prefix.count,
                    prefix.Flags, pCLineAndPrOpts,
                    bIsCompressedFile,
                    svdConvertor
                    );
                tagname = "USER";
                break;

                // HCDs (hash compare data for fast sync)
            case SVD_TAG_SYNC_HASH_COMPARE_DATA:
                chunkerr = OnHashCompareData(
                    in,
                    prefix.count,
                    prefix.Flags,
                    pCLineAndPrOpts,
                    bIsCompressedFile,
                    dataFormatFlags
                    );
                tagname = "HCDS";
                break;

                // Unkown chunk.  Error condition
            default:
                StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr,
                    "FAILED: Encountered an unknown tag 0x%lX at offset %ld\n", prefix.tag, ftell_wrapper(in, bIsCompressedFile));
                retVal = 0;
                eof = true;
            }

            // If the individual chunk reader failed, so should we
            if (chunkerr == 0)
            {
                StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr,
                    "FAILED: Encountered error in chunk \"%s\" at offset %ld\n", tagname, ftell_wrapper(in, bIsCompressedFile));
                retVal = 0;
                eof = true;
            }
        }
    }

    // Close the file
    fclose_wrapper(in, bIsCompressedFile);

    stTimeStampsAndSeqNos_t tsandseqnosfilename;
    E_FILE_TYPE efiletyp = ParseFileName(fileName, &tsandseqnosfilename);
    bool bIsTso = IsTso(fileName);

    if (E_BAD == efiletyp)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Invalid File Name %s\n", fileName);
        retVal = 0;
    }

    if (retVal && (E_DIFF == efiletyp))
    {
        stTimeStampsAndSeqNos_t tsandseqnosfile;
        tsandseqnosfile.startTimeStamp = timeStampOfFirstChange;
        tsandseqnosfile.startSequenceNumber = sequenceNumberOfFirstChange;
        tsandseqnosfile.lastTimeStamp = timeStampOfLastChange;
        tsandseqnosfile.lastSequenceNumber = sequenceNumberOfLastChange;
        retVal = validateTimestampsAndSequenceNos(
            bIsDirtyBlockV2,
            bIsTso,
            &tsandseqnosfilename,
            &tsandseqnosfile,
            pCLineAndPrOpts,
            timeDelta,
            sequenceDelta
            );
        if (retVal)
        {
            if (pTsAndSeqnosRead)
            {
                *pTsAndSeqnosRead = tsandseqnosfile;
            }
        }
    }

    if (retVal && pCLineAndPrOpts->m_bstatEnabled)
    {
        PrintStats(&diffstat, pCLineAndPrOpts->m_bShouldPrint);
    }

    // Exit
    return (retVal);
}

//
// ValidateSVFile (char *fileName)
//  Scans an SV data file to ensure proper formatting
//  Parameter:
//      char *fileName  Path and name of teh file to be verified
//  Return Value:
//      1               On succesful completion
//      0               On detecting a problem with the file
//
int ValidateSVFile (
                    const char *fileName, 
                    const stCmdLineAndPrintOpts_t *pclinepropts,
                    stTimeStampsAndSeqNos_t *pTsAndSeqnosRead,
                    Checksums_t* checksums
                   )
{
    assert (NULL != fileName);
    int retVal = 0;

    stCmdLineAndPrintOpts_t cLineAndPrOpts;
    const stCmdLineAndPrintOpts_t *pCLineAndPrOpts = pclinepropts ? (pclinepropts) : (&cLineAndPrOpts);
    bool bIsCompressedFile = IsCompressedFile(fileName);
    void *in = fopen_wrapper (fileName, "rb", bIsCompressedFile);
    
    if (NULL == in)
    {
        StandardPrintf (pCLineAndPrOpts->m_bShouldPrint, stderr, "No such file %s, dwErr = %d\n", fileName , errno);
        return (retVal);
    }
    
    // Exit
    return ValidateSVStream(in, fileName, pclinepropts, pTsAndSeqnosRead, checksums);
}

//
// ValidateSVBuffer (char *buffer)
//  Scans an SV data buffer to ensure proper formatting
//  Parameter:
//      char *buffer  data to be verified
//  Return Value:
//      1               On succesful completion
//      0               On detecting a problem with the buffer
//
int ValidateSVBuffer(
    char *buffer,
    const size_t bufferlen,
    const char *fileName,
    const stCmdLineAndPrintOpts_t *pclinepropts,
    stTimeStampsAndSeqNos_t *pTsAndSeqnosRead,
    Checksums_t* checksums
    )
{
    int retVal = 0;
    FILE *fp = NULL;
    void *in = NULL;

    stCmdLineAndPrintOpts_t cLineAndPrOpts;
    const stCmdLineAndPrintOpts_t *pCLineAndPrOpts = pclinepropts ? (pclinepropts) : (&cLineAndPrOpts);

#ifdef SV_UNIX
    fp = fmemopen(buffer, bufferlen, "rb");
#else
    StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "ValidateSVBuffer not implemented.\n");
    return (retVal);
#endif

    if (NULL == fp)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "No such file %s, dwErr = %d\n", fileName, errno);
        return (retVal);
    }

    in = (void *)fp;

    return ValidateSVStream(in, fileName, pclinepropts, pTsAndSeqnosRead, checksums);;
}

const char* getToken( const char* temp, SV_ULONGLONG& value )
{
	const char* temp2 = strchr( temp, '_' ) ;
	char temp3[128] ={0} ;
	if( temp2 != NULL )
	{
		inm_strncpy_s( temp3, ARRAYSIZE(temp3), temp, temp2 - temp ) ;
#ifdef SV_WINDOWS 
		value = ( SV_ULONGLONG ) _strtoui64( temp3, NULL, 10 ) ;
#else
		value = ( SV_ULONGLONG ) strtoull( temp3, NULL, 10 ) ;
#endif 
	}
    return temp2 ; 	
}

//
// OnHeader (void *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      void *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnHeader(
             void *in, 
             const unsigned long size, 
             const unsigned long flags, 
             const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
             bool bIsCompressedFile
            )
{
    assert (in);
    assert (size > 0);
    
    int retVal = 0;
    size_t readin = 0;
    SVD_HEADER1 *head = new SVD_HEADER1 [size];
    if (NULL == head)
    {
        return (0);
    }
    
    // Read the header chunk
    readin = fread_wrapper (head, sizeof (SVD_HEADER1), size, in, bIsCompressedFile);
    
    // If there is no discrepancy between what we read and what we expected to read, we succeded
    if (size == (unsigned) readin)
    {
        retVal = 1;
        if (!pCLineAndPrOpts->m_bstatEnabled)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "SVD1 Header\n");
        }
    }
    
    delete [] head;
    return retVal;
}


//
// OnHeaderV2 (void *in, unsigned int size)
//  Scans a block with a SVD_HEADER2 tag
//  Parameters:
//      void *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnHeaderV2(
               void *in, 
               const unsigned long size, 
               const unsigned long flags, 
               const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
               bool bIsCompressedFile
              )
{
    assert (in);
    assert (size > 0);
    
    int retVal = 0;
    size_t readin = 0;
    SVD_HEADER_V2 *head = new SVD_HEADER_V2 [size];
    if (NULL == head)
    {
        return (0);
    }
    
    // Read the header chunk
    readin = fread_wrapper (head, sizeof (SVD_HEADER_V2), size, in, bIsCompressedFile);
    
    // If there is no discrepancy between what we read and what we expected to read, we succeded
    if (size == (unsigned) readin)
    {
        retVal = 1;
        if (!pCLineAndPrOpts->m_bstatEnabled)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "SVD2 Header\n");
        }
    }
    
    delete [] head;
    return retVal;
}

char* convertCSToString( unsigned char* computedHash )
{
    char *md5localcksum = new char[INM_MD5TEXTSIGLEN + 1] ;
	// Get the string representation of the checksum
    for (int i = 0; i < INM_MD5TEXTSIGLEN/2; i++) {
		inm_sprintf_s((md5localcksum + i*2), ((INM_MD5TEXTSIGLEN + 1) - (i*2)), "%02X", computedHash[i]);
    }
    return md5localcksum ;
}

char* calculateCS( unsigned char* volumeData, SV_ULONGLONG dataLength )
{
    unsigned char computedHash[16];
    INM_MD5_CTX ctx;
    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, volumeData, dataLength ) ;
    INM_MD5Final(computedHash, &ctx);
    
    return convertCSToString( computedHash ) ;    
}

int calculateCSAndStore( 
                        void* in, 
                        bool bIsCompressedFile, 
                        SV_LONGLONG byteOffset, 
                        SV_LONGLONG dataLength, 
                        bool bShouldPrint,
						Checksums_t* checksums 
                       )
{
    int retVal = 0 ;

    if( checksums != NULL )
    {
        unsigned char *volumeData = NULL;
        #ifdef SV_WINDOWS
        volumeData = new (std::nothrow) unsigned char[dataLength];
        #else
        volumeData = (unsigned char *)valloc(dataLength);
        #endif

        if (volumeData)
        {
            unsigned long long bytesRead = fread_wrapper( volumeData, 1, dataLength, in, bIsCompressedFile ) ;
            if( bytesRead != dataLength )
            {
                StandardPrintf(bShouldPrint, stderr, "FAILED: Unable to read the volume data from the file. Tobe Read:" ULLSPEC" Read:" ULLSPEC"\n",
                                bytesRead, dataLength ) ;
            }
            else
            {
                std::ostringstream msg ;
                char* Checksum = calculateCS( volumeData, dataLength ) ;
                msg << byteOffset << "," << dataLength << "," << Checksum << std::endl ;
                delete [] Checksum ;
                checksums->push_back( msg.str() ) ;
                retVal = 1 ;
            }
            #ifdef SV_WINDOWS
            delete [] volumeData;
            #else
            free(volumeData);
            #endif
        }
        else
        {
            StandardPrintf(bShouldPrint, stderr, "FAILED: Unable to allocate " ULLSPEC " bytes to read diff data from file\n",
                           dataLength);
        }
    }
    else
    {
        retVal = 1;
    }
    return retVal ;
}
//
// OnDirtyBlocksData (void *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      void *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnDirtyBlocksData(
                      void *in, 
                      const unsigned long size, 
                      const unsigned long flags, 
                      const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                      DIFF_STATS *pDiffStats, 
                      bool bIsCompressedFile,
                      SVDConvertorPtr svdConvertor, 
                      Checksums_t* checksums
                     )
{
    assert (in);
    assert (size > 0);
    
    unsigned long   i = 0;
    unsigned long   offset = 0;
    int             retVal = 1;
    SVD_DIRTY_BLOCK header = {0};
    unsigned long   eof_offset = 0;
    
    // For silent mode - commented out the following
    if (pCLineAndPrOpts->m_bvboseEnabled) {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "DRTD\tRecords: %ld\tFile Offset: %ld\n", size, ftell_wrapper (in, bIsCompressedFile));
    }

    //Store the current offset
    offset = ftell_wrapper (in, bIsCompressedFile);

    //Compute the file length
    (void)fseek_wrapper(in, 0, SEEK_END, bIsCompressedFile);
    eof_offset = ftell_wrapper(in, bIsCompressedFile);

    //Restore to the original offset
    (void)fseek_wrapper(in, offset, SEEK_SET, bIsCompressedFile);
    
    // Cycle through all records, reading each one
    for (i = 0; i < size; i++)
    {
        // Read the record header
        if (fread_wrapper (&header, sizeof (SVD_DIRTY_BLOCK), 1, in, bIsCompressedFile) == 0)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Record #%lu of %lu does not exist\n", i + 1, size);
            retVal = 0;
            break;
        }
        
        offset = ftell_wrapper(in, bIsCompressedFile);
        svdConvertor->convertDBHeader( header ) ;
        retVal = calculateCSAndStore( in, bIsCompressedFile, header.ByteOffset, header.Length, pCLineAndPrOpts->m_bShouldPrint, checksums ) ;
        if( retVal == 0 )
            break ;
        
// A workaround for our one known bug.  If we're compiling on Windows we can use MS's printf so we will

// For silent mode- commented out the following
        if (pCLineAndPrOpts->m_bvboseEnabled) 
        {
#ifndef SV_WINDOWS
        // at least get the full hex values printed
             StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "[%lu] file offset: %lu, data length: %llu(0x%x%x), vol. offset: %llu(0x%x%x)\n", 
                i, offset, header.Length, (unsigned int)(header.Length >> 32), (unsigned int)header.Length,
                header.ByteOffset, (unsigned int)(header.ByteOffset >> 32), (unsigned int)header.ByteOffset);
#else
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "[%lu] file offset: %lu(0x%x), data length: %I64u(0x%I64x), vol. offset: %I64u(0x%I64x)\n", 
            i, offset, offset, header.Length, header.Length, header.ByteOffset, header.ByteOffset);
#endif // SV_WINDOWS
        }
        
        // If, for some reason we're biting off more than we can chew we stop
        if (header.Length > ULONG_MAX)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Block header too long (%lu)\n", (unsigned long)header.Length);
            retVal = 0;
            break;
        }

        //Check if we are crossing an end-of-file mark.
        if ( (offset + (unsigned long) header.Length) > eof_offset) 
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr,
					"FAILED: Encountered an end-of-file at offset %lu, While trying to seek %lu bytes from offset %lu \n", 
					eof_offset, (unsigned long)header.Length, offset);
            retVal = 0;
            break;

        }

        // Try to skip past the actual data.
        if( checksums == NULL && fseek_wrapper (in, (long)header.Length, SEEK_CUR, bIsCompressedFile) != 0)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr,
					"FAILED: Could not seek to end of chunk (%lu bytes from offset %lu)\n", (unsigned long)header.Length, offset);
            retVal = 0;
            break;
        }
        
        if (pCLineAndPrOpts->m_bstatEnabled)
        {
            pDiffStats->m_ullNumIOs++;
            pDiffStats->FillWriteLengthFreq(header.Length);
            pDiffStats->FillOverLapCount(header.ByteOffset, header.Length);
        }
    }
    
    if (!pCLineAndPrOpts->m_bstatEnabled)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "End DRTD\n");
    }
    return retVal;
}

int OnDirtyBlocksDataV2( 
                        void *in, 
                        const unsigned long size, 
                        const unsigned long flags, 
                        const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                        DIFF_STATS *pDiffStats,
                        bool bIsCompressedFile, 
                        SV_UINT *ptimeDelta, 
                        SV_UINT *psequenceDelta, 
                        SVDConvertorPtr svdConvertor, 
                        Checksums_t* checksums
                       )
{
	assert( in ) ;
    assert( size > 0 ) ;

	unsigned long i = 0 ;
	unsigned long offset = 0 ;
	int retVal = 1 ;

	SVD_DIRTY_BLOCK_V2 header = { 0 } ;
	unsigned long eof_offset = 0 ;
	if (pCLineAndPrOpts->m_bvboseEnabled) 
	{
		StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "DDV2\tRecords: %ld\tFile Offset: %ld\n", size, ftell_wrapper (in, bIsCompressedFile));
	}

	offset = ftell_wrapper( in , bIsCompressedFile) ;
	(void) fseek_wrapper( in, 0, SEEK_END, bIsCompressedFile ) ;
	eof_offset = ftell_wrapper( in , bIsCompressedFile) ;

	( void ) fseek_wrapper( in, offset, SEEK_SET , bIsCompressedFile) ;

	for( i = 0; i < size; i++ ) 
	{
		if( fread_wrapper( &header, sizeof( SVD_DIRTY_BLOCK_V2 ), 1, in , bIsCompressedFile) == 0 )
		{
			StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Record #%lu of %lu does not exist\n", i + 1, size);
			retVal = 0;
			break;
		}
		offset = ftell_wrapper( in , bIsCompressedFile) ;
        svdConvertor->convertDBHeaderV2( header ) ;
		retVal = calculateCSAndStore( in, bIsCompressedFile,  header.ByteOffset, header.Length, pCLineAndPrOpts->m_bShouldPrint, checksums ) ;
        if( retVal == 0 )
            break ;
            
		if( pCLineAndPrOpts->m_bvboseEnabled )
		{
#ifndef SV_WINDOWS
        	StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "[%lu] file offset: %lu," 
					"data length: %u(0x%x)," 
					"vol. offset: %llu(0x%x%x),"
					"sequence number delta: %u(0x%x),"
					"time delta: %u(0x%x) \n", 
               		i, offset, 
					header.Length, header.Length,
					header.ByteOffset, (unsigned int)(header.ByteOffset >> 32), (unsigned int)header.ByteOffset,
					header.uiSequenceNumberDelta, header.uiSequenceNumberDelta,
					header.uiTimeDelta, header.uiTimeDelta );
#else
			StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "[%lu] file offset: %lu(0x%x)," 
					"data length: %u(0x%x)," 
					"vol. offset: %I64u(0x%I64x),"
					"sequence number delta: %u(0x%x),"
					"time delta: %u(0x%x) \n", 
					i, offset, offset, 
					header.Length, header.Length, 
					header.ByteOffset, header.ByteOffset,
					header.uiSequenceNumberDelta, header.uiSequenceNumberDelta,
					header.uiTimeDelta, header.uiTimeDelta ) ;
#endif
		}

		if (header.Length > UINT_MAX)
        {
        	StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, " FAILED: Block header too long (%u)\n", header.Length );
			retVal = 0;
			break;
		}
        if( i != 0 && *ptimeDelta >= header.uiTimeDelta ) 
		{
			StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "timeDelta: %u is less or equal to previous time delta: %u\n", 
					*ptimeDelta, header.uiTimeDelta ) ;
			retVal = 0 ;
			break ;
		}
		if( i != 0 && *psequenceDelta >= header.uiSequenceNumberDelta )
		{
			StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, " sequnceDelta: %u is less or equal to previous sequence delta: %u\n",
					*psequenceDelta, header.uiSequenceNumberDelta ) ;
			retVal = 0 ;
			break ;
		}

        *ptimeDelta = header.uiTimeDelta ;
		*psequenceDelta = header.uiSequenceNumberDelta ;

		if( ( offset + header.Length ) > eof_offset )
		{
			StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Encountered an end-of-file at offset %lu, While trying to seek %u bytes from offset %lu \n", 
					eof_offset, header.Length, offset);
			 retVal = 0;
			 break;
		}

		if( checksums == NULL && fseek_wrapper( in, header.Length, SEEK_CUR, bIsCompressedFile ) != 0 ) 
		{
			StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Could not seek to end of chunk (%u bytes from offset %lu)\n", 
					header.Length, offset);
			retVal = 0;
			break;
		}
       
        if (pCLineAndPrOpts->m_bstatEnabled)
        {
            pDiffStats->m_ullNumIOs++;     
            pDiffStats->FillWriteLengthFreq(header.Length);
            pDiffStats->FillOverLapCount(header.ByteOffset, header.Length);
        }
	}

    if (!pCLineAndPrOpts->m_bstatEnabled)
    {
	    StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "End of DDV2\n ") ;
    }
	return retVal ;
}
 
//
// OnDirtyBlocks (void *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      void *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnDirtyBlocks(
                  void *in, 
                  const unsigned long size, 
                  const unsigned long flags, 
                  const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                  bool bIsCompressedFile,
                  SVDConvertorPtr svdConvertor
                 )
{
    assert (in);
    assert (size > 0);
    
    unsigned long   i = 0;
    unsigned long   offset = 0;
    int             retVal = 1;
    SVD_DIRTY_BLOCK header;
    
    //For silent mode - commented out the following  
    if (pCLineAndPrOpts->m_bvboseEnabled) {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "DIRT\tRecords: %ld\tFile Offset: %ld\n", size, ftell_wrapper (in, bIsCompressedFile));
    }

    // Cycle through all records, reading each one
    for (i = 0; i < size; i++)
    {
        offset = ftell_wrapper (in, bIsCompressedFile);
        
        // Read the record header
        if (fread_wrapper (&header, sizeof (SVD_DIRTY_BLOCK), 1, in, bIsCompressedFile) == 0)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Record #%lu of %lu does not exist\n", i + 1, size);
            retVal = 0;
            break;
        }

        svdConvertor->convertDBHeader( header ) ;
        
// A workaround for our one known bug.  If we're compiling on Windows we can use MS's printf so we will
//For silent mode - comment it out the following
        if (pCLineAndPrOpts->m_bvboseEnabled) {
#ifndef SV_WINDOWS
            // at least get the full hex values printed
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "[%lu] file offset: %lu, data length: %llu(0x%x%x), vol. offset: %llu(0x%x%x)\n", 
                i, offset, header.Length, (unsigned int)(header.Length >> 32), (unsigned int)header.Length,
                header.ByteOffset, (unsigned int)(header.ByteOffset >> 32), (unsigned int)header.ByteOffset);
#else
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "[%lu] file offset: %lu(0x%x), data length: %I64u(0x%I64x), vol. offset: %I64u(0x%I64x)\n", 
                i, offset, offset, header.Length, header.Length, header.ByteOffset, header.ByteOffset);
#endif // SV_WINDOWS
        }
        
        // If, for some reason we're biting off more than we can chew we stop
        if (header.Length > ULONG_MAX)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Block header too long (%lu)\n", (unsigned long)header.Length); 
            retVal = 0;
            break;
        }
    }
    
    if (!pCLineAndPrOpts->m_bstatEnabled)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "End DIRT\n");
    }
    return retVal;
}

// OnTimeStmapInfo (void *in, unsigned int size)
//  Scans a block with a SVD_TIME_STAMP tag
//  Parameters:
//      void *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags Tag that tells whether it is a TOFC or TOLC
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnTimeStampInfo(
                    void *in, 
                    const unsigned long size, 
                    const unsigned long tag, 
                    const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                    bool bIsCompressedFile, 
                    SVDConvertorPtr svdConvertor,
                    SV_ULONGLONG *OutTSFCorTSLC,
                    SV_ULONGLONG *OutSeqFCorLC
                   )
{
    assert (in);
    assert (size > 0);
    unsigned long int i;
    int retVal = 1;
    SVD_TIME_STAMP timestamp;

    if(tag == SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE)
	{
        if (!pCLineAndPrOpts->m_bstatEnabled)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "TOFC\n");
        }
	}
    else
	{
        if (!pCLineAndPrOpts->m_bstatEnabled)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "TOLC\n");
        }
	}

    for(i = 0 ; i < size ; i++ )
    {
        if (fread_wrapper (&timestamp, sizeof (SVD_TIME_STAMP), 1, in, bIsCompressedFile) == 0)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Record to read record %lu while reading timstamp Info\n", i+1);
            retVal = 0;
            break;
        }

        svdConvertor->convertTimeStamp( timestamp ) ;
        if(pCLineAndPrOpts->m_bvboseEnabled)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "Rec Type %d Flag %d length %d Timestamp" ULLSPEC " Seqno %lu\n", 
					timestamp.Header.usStreamRecType, timestamp.Header.ucFlags, timestamp.Header.ucLength, timestamp.TimeInHundNanoSecondsFromJan1601, 
					timestamp.ulSequenceNumber);
        }

        *OutTSFCorTSLC = timestamp.TimeInHundNanoSecondsFromJan1601 ;
        *OutSeqFCorLC = timestamp.ulSequenceNumber;
    }
    
    return retVal;
}

int OnTimeStampInfoV2( 
                      void *in, 
                      const unsigned long size, 
                      const unsigned long tag, 
                      const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                      bool bIsCompressedFile,
                      SV_ULONGLONG *OutTSFCorTSLC, 
                      SV_ULONGLONG *OutSeqFCorLC, 
                      SVDConvertorPtr svdConvertor 
                     ) 
{
	assert( in ) ;
	assert( size > 0 ) ;

	unsigned long int i ;
	int retVal = 1 ;
	SVD_TIME_STAMP_V2 timestamp ;

	if( tag == SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2 ) 
	{
        if (!pCLineAndPrOpts->m_bstatEnabled)
        {
		    StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "TFV2\n" ) ;
        }
	}
	else
	{
        if (!pCLineAndPrOpts->m_bstatEnabled)
        {
		    StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "TLV2\n" ) ;
        }
	}

	for( i = 0; i < size; i++ )
	{
		if( fread_wrapper( &timestamp, sizeof( SVD_TIME_STAMP_V2 ), 1, in , bIsCompressedFile) == 0 )
		{
			StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Record to read record %lu while reading timestamp information\n", i + 1 ) ;
			retVal = 0 ;
			break ;
		}

         svdConvertor->convertTimeStampV2( timestamp ) ;

		if( pCLineAndPrOpts->m_bvboseEnabled )
		{
			StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "RecType: %d, Flag: %d, Length: %d, TimeStamp:" ULLSPEC", Seq. No:" ULLSPEC"\n",
					timestamp.Header.usStreamRecType,
					timestamp.Header.ucFlags,
					timestamp.Header.ucLength,
					timestamp.TimeInHundNanoSecondsFromJan1601,
					timestamp.SequenceNumber ) ;
					
		}
		*OutTSFCorTSLC = timestamp.TimeInHundNanoSecondsFromJan1601 ;
		*OutSeqFCorLC = timestamp.SequenceNumber ;		
	}
	return retVal ;
}

// OnDRTDChanges (void *in, unsigned int size)
//  Scans a block with a SVD_TAG_LENGTH_OF_DRTD_CHANGES tag
//  Parameters:
//      void *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnDRTDChanges(
                  void *in, 
                  const unsigned long size, 
                  const unsigned long flags, 
                  const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                  bool bIsCompressedFile,
                  SVDConvertorPtr svdConvertor 
                 )
{
    assert (in);
    assert (size > 0);
    unsigned long int i;
    unsigned long offset, eof_offset;
    SV_ULONGLONG Changes;
    SVD_PREFIX  prefix = {0};    

    for(i = 0 ; i < size ; i++ )
    {
        if (fread_wrapper (&Changes, sizeof (SV_ULONGLONG), 1, in, bIsCompressedFile) == 0)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Record to read record %lu while reading timstamp Info", i+1);
            return 0;
        }

        //Store the current offset
        offset = ftell_wrapper(in, bIsCompressedFile);

        (void)fseek_wrapper(in, 0, SEEK_END, bIsCompressedFile);
        eof_offset = ftell_wrapper(in, bIsCompressedFile);

        Changes = svdConvertor->getBaseConvertor()->convertULONGLONG( Changes ) ;
        if((offset + (unsigned long)Changes) > eof_offset)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Encountered size change that is greater than the end offset of the file cur_offset %lu, End offset %lu, Size of ChangesInfo %llu\n", offset, eof_offset, Changes);
            return 0;
        }

        //Restore the offset.
        fseek_wrapper(in, offset, SEEK_SET, bIsCompressedFile);

        //Seek to the end of LODC chunk
        if (fseek_wrapper (in, (unsigned long)Changes, SEEK_CUR, bIsCompressedFile) != 0)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr,
					"FAILED: Could not seek to end of LODC chunk (%lu bytes from offset %lu)\n", (unsigned long)Changes, offset);
            return 0;
        }
        
        //After LODC, we expect a SVD_PREFIX with SVD_TAG_TIME_STAMP_OF_LAST_CHANGE tag. Validate it.
        if (fread_wrapper (&prefix, sizeof (SVD_PREFIX), 1, in, bIsCompressedFile) == 0)
        {
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: Record to read svd prefix\n");
            return 0;
        }

       svdConvertor->convertPrefix( prefix ) ;

        if( prefix.tag != SVD_TAG_TIME_STAMP_OF_LAST_CHANGE && 
			   prefix.tag != SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2  )
        {    
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: After DRTD changes, expecting TOLC/TLV2 but got tag %lu\n", prefix.tag);
            return 0;
        }

        //Restore offset
        fseek_wrapper(in, offset, SEEK_SET, bIsCompressedFile);
        
        if(pCLineAndPrOpts->m_bvboseEnabled)
		{
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "size of change %llu", Changes);
		}
    }

    if (!pCLineAndPrOpts->m_bstatEnabled)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "End LODC\n");
    }
    return 1;
}

//  OnUDT
//  Scans a block with a SVD_TAG_USER tag
//  Parameters:
//      void *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//        bool                verbose flag
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int OnUDT(
          void *in, 
          const unsigned long size, 
          const unsigned long flags, 
          const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
          bool bIsCompressedFile,
          SVDConvertorPtr svdConvertori
         )
{
    unsigned long offset = 0;

    if (!pCLineAndPrOpts->m_bstatEnabled)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "On UDT\n");
    }
    if (pCLineAndPrOpts->m_bvboseEnabled)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "\tLength of UDT: %lu\n", flags);
    }
    
    offset = ftell_wrapper(in, bIsCompressedFile);
    offset += flags;
    fseek_wrapper(in, offset, SEEK_SET, bIsCompressedFile);

    if (!pCLineAndPrOpts->m_bstatEnabled)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "End UDT\n");
    }
    return 1;
}

//
// Processes a HashCompareInfo block (the payload for HashCompareData)
// Returns 0 on failure, nonzero on success.
//
int OnHashCompareInfo( 
                      void* in , 
                      const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                      bool bIsCompressedFile, 
                      HashConvertorPtr hashConvertor
                     )
{
    assert( NULL != in );
    HashCompareInfo info = {0};

    // Read HashCompareInfo header
    if (fread_wrapper (&info, sizeof (HashCompareInfo), 1, in, bIsCompressedFile) == 0)
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "FAILED: HashCompareInfo at offset %lu couldn't be read\n", ftell_wrapper (in, bIsCompressedFile));
        return 0;
    }

    hashConvertor->convertInfo( info ) ;

    StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "HashCompareData: chunk_size %lu compare_size %lu hash_algorithm ",
        info.m_ChunkSize, info.m_CompareSize );
    if( info.m_Algo == 1 ) 
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "MD5\n" );
    }
    else 
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "Unknown (%d)\n", info.m_Algo );
    }

    // Assume a variable number of HashCompareNodes
    // Terminated by a node with zero length
    bool zeroTerminated = false;
    int nodeCount = 0;
    StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "HashNodes: %s", pCLineAndPrOpts->m_bvboseEnabled ? "\n" : "" );
    while( true ) 
    {
        HashCompareNode node; 
        if (0 == fread_wrapper (&node, sizeof (HashCompareNode), 1, in, bIsCompressedFile))
        {
            break;
        }

         hashConvertor->convertNode( node ) ;
        ++ nodeCount;

        if (pCLineAndPrOpts->m_bvboseEnabled) 
        {
#ifndef SV_WINDOWS
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "\t[%d] offset %llu(0x%x%x) ", 
                nodeCount, node.m_Offset, (unsigned int)(node.m_Offset >> 32), 
                (unsigned int)node.m_Offset);
#else
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "\t[%d] offset %I64u(0x%I64x) ", 
                nodeCount, node.m_Offset, node.m_Offset);
#endif
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "length %lu hash 0x", node.m_Length );
            for( size_t i = 0; i < sizeof(node.m_Hash); ++i )
            {
                StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "%02x", static_cast<int>( node.m_Hash[i] ) ); 
            }
            StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "\n" );
        }
        if( 0 == node.m_Length ) 
        {
            zeroTerminated = true;
            break;
        }
    }
    if (!pCLineAndPrOpts->m_bvboseEnabled) 
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stdout, "%d nodes\n", nodeCount);
    }

    if( !zeroTerminated ) 
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, 
				"Unexpected end of HashCompareNodes. Expected terminator with zero length at offset %lu\n", ftell_wrapper(in, bIsCompressedFile));
        return 0;
    }

    return 1;
}

//
// Processes a 'Hash Compare Data' block, sent by a fast sync source 
// Returns 0 on failure, nonzero on success
//
int OnHashCompareData(
                      void *in, 
                      unsigned long size, 
                      unsigned long flags, 
                      const stCmdLineAndPrintOpts_t *pCLineAndPrOpts, 
                      bool bIsCompressedFile, 
                      SV_UINT dataFormatFlags
                     )
{
    assert( NULL != in );

    if( 0 == size ) 
    {
        StandardPrintf(pCLineAndPrOpts->m_bShouldPrint, stderr, "HashCompareData: has zero size! expected one or more.\n" );
        return 0;
    }

    HashConvertorPtr hashConvertor ;
    ConvertorFactory::getHashConvertor( dataFormatFlags, hashConvertor ) ;
    for( size_t i = 0; i < size; ++i ) 
    {
        if( 0 == OnHashCompareInfo( in , pCLineAndPrOpts, bIsCompressedFile, hashConvertor) ) 
        {
            return 0;
        }    
    }

    return 1;
}

 void *fopen_wrapper(const char *fname, char *mode, bool bIsCompressedFile)
{
    FILE *fp = NULL;
    gzFile *gzfp = NULL;
    void *retval = NULL;

    if (fname == NULL) return retval;

    if (bIsCompressedFile)
    {
        gzfp = (gzFile *)malloc(sizeof(gzFile));
        if(gzfp == NULL) return NULL;
        (void)memset(gzfp,0,sizeof(gzFile));
        *gzfp = gzopen(fname, mode);
        retval = gzfp;
    }
    else
    {
// PR#10815: Long Path support
#if defined (SV_WINDOWS) && defined (SV_USES_LONGPATHS)

		// Using getLongPathName() from common module here causes linker error
		// while linking svdcheck project. 

		std::string fileName = fname;

		//replace all '/' with '\\'
		std::replace(fileName.begin(), fileName.end(), '/', '\\');

		//if path is not a drive letter and it contains neither "\\\\?\\" nor "\\\\?\\"
		//add "\\\\?\\"
		if( (fileName.length() > 2) &&
			(fileName.substr(0, 4) != "\\\\?\\" && fileName.substr(0, 4) != "\\\\.\\") )
		{
			fileName = "\\\\?\\" + fileName;
		}

		//remove repeting '\\' if any
		std::string::size_type index = -1;
		while( (index = fileName.find('\\', ++index)) != fileName.npos )
			if(index >= 4 && fileName[index-1] == '\\')
				fileName.erase(fileName.begin() + index--);

		fp = _wfopen (CA2W(fileName.c_str()), CA2W(mode));        
#else
        fp = fopen (fname, mode);        
#endif
        retval = fp;
    }

    return retval;
}

 int fclose_wrapper(void *fpcontext, bool bIsCompressedFile)
{
    FILE *fp = NULL;
    gzFile *gzfp = NULL;
    int retval = 1;

    if(fpcontext == NULL) return retval;

    if (bIsCompressedFile)
    {
        gzfp = (gzFile *)fpcontext;
        retval = gzclose(*gzfp);
        free(gzfp);
    }
    else
    {
        fp = (FILE *)fpcontext;
        retval = fclose(fp);
    }
    
    return retval;
}

 size_t fread_wrapper(void *buf, size_t size, size_t nitems, void *fpcontext, bool bIsCompressedFile)
{
    FILE *fp = NULL;
    gzFile *gzfp = NULL;
    size_t retval = 0;

    if(fpcontext == NULL) return retval;

    if (bIsCompressedFile)
    {
        gzfp = (gzFile *)fpcontext;
		retval = gzread (*gzfp, buf, (unsigned int)(size*nitems));
        //calculate nitems that were successfully read
        retval = retval/size;
    }
    else
    {
        fp = (FILE *)fpcontext;
        retval = fread (buf, size, nitems, fp);
    }

    return retval;
}

 size_t fwrite_wrapper(const void *buf, size_t size, size_t nitems, void *fpcontext, bool bIsCompressedFile)
{
 //Not supported
 return 0;
}

 long ftell_wrapper(void *fpcontext, bool bIsCompressedFile)
{
    FILE *fp = NULL;
    gzFile *gzfp = NULL;
    long retval = 0;

    if(fpcontext == NULL) return retval;

    if (bIsCompressedFile)
    {
        gzfp = (gzFile *)fpcontext;
        retval = gztell(*gzfp);
    }
    else
    {
        fp = (FILE *)fpcontext;
        retval = ftell(fp);
    }

    return retval;
}

 int fseek_wrapper(void *fpcontext, long offset, int mode, bool bIsCompressedFile)
{
    FILE *fp = NULL;
    gzFile *gzfp = NULL;
    long retval = 0;

    if(fpcontext == NULL) return retval;

    if (bIsCompressedFile)
    {
        gzfp = (gzFile *)fpcontext;

        //gzseek doesn't support SEEK_END
        //gzseek returns the resulting offset after seek.
        //-1 is returned on failure.
        if (mode != SEEK_END)
            retval = gzseek(*gzfp, offset, mode);
        else
            retval = gzseek_end(gzfp, offset, bIsCompressedFile);

    }
    else
    {
        fp = (FILE *)fpcontext;
        retval = fseek(fp, offset, mode);
    }

    // return 0 on success, -1 on failure
    return (retval < 0 ? -1 : 0);
}

// Imitates SEEK_END functionality
// Read 8192 byte chunk data and discard it, till 
// we reach end of file.
// Then seek to specified offset
 int gzseek_end(gzFile *gzfp, long offset, bool bIsCompressedFile)
{
   long current_offset;
   size_t nr;
   char dummy[8192];
   
   current_offset = ftell_wrapper(gzfp, bIsCompressedFile);
   
   //seek to end-of-file
   while((nr = fread_wrapper (&dummy, 1, sizeof (dummy), gzfp, bIsCompressedFile)))
       current_offset += (long)nr;

   //if offset is non-zero, seek to offset from end-of-file
   if (offset != 0)
      current_offset = gzseek(*gzfp, offset, SEEK_CUR);
   
   return (current_offset);
}

// Returns 1 if the file name ends with ".gz",
// Otherwise 0 is reurned
 bool IsCompressedFile(const char *fname)
{
    const char *compressed_file_extension = GZIP_FILE_EXTN;

    if ((stricmp(fname + strlen(fname) - strlen(compressed_file_extension), compressed_file_extension)) == 0)
        return true;
    else
        return false;    
}

 void SetValidateSvdLogCallback(boost::function<void(unsigned int loglevel, const char *msg)> callback)
 {
     s_svdCheckLogCallback = callback;
 }

 std::string GetFormatedMsg(const char* format,
     va_list args)
 {
     ACE_ASSERT(NULL != format);
     std::stringstream msg;

     try
     {
         std::vector<char> frmtMsgBuff(FORMAT_MSG_BUFF_SIZE, 0);
         VSNPRINTF(&frmtMsgBuff[0], frmtMsgBuff.size(), frmtMsgBuff.size() - 1, format, args);

         msg << &frmtMsgBuff[0];
     }
     catch (ContextualException ce)
     {
         msg << "Exception in formating message. Error: "
             << ce.what();
     }
     catch (...)
     {
         msg << "Unknown exception in formating message.";
     }

     return msg.str();
 }

//if S2 is calling, this does not print any thing

void StandardPrintf(const bool &bShouldPrint, FILE *fp, char *format, ...)
{
    if (bShouldPrint)
    {
        va_list ap;
        va_start(ap, format);

        if (s_svdCheckLogCallback)
        {
            std::string msg = GetFormatedMsg(format, ap);
            s_svdCheckLogCallback(VALIDATESVD_LOG_ALWAYS, msg.c_str());
        }
        else
        {
            /* not printing anything on stderr since no one redirects 2> */
            fp = stdout;
            vfprintf(fp, format, ap);
        }

        va_end(ap);
    }
}

E_FILE_TYPE ParseFileName(const char *fileName, stTimeStampsAndSeqNos_t *ptsandseqnos)
{ 
    E_FILE_TYPE efiletyp = E_BAD;
    #ifdef SV_WINDOWS 
    char slash = '\\' ;
    #else
    char slash = '/' ;
    #endif
    const char* file = strrchr( fileName, slash ) ;
    if( file == NULL )
    {
        file = fileName ;
    }
    else
    {
        file++ ;
    }
    
    const char *starttimetok = strstr(file, STARTTIME_TOKEN);
    if (starttimetok)
    {
        starttimetok += strlen(STARTTIME_TOKEN);
        starttimetok = getToken(starttimetok, ptsandseqnos->startTimeStamp);
        if (starttimetok)
        {
            starttimetok++;
            starttimetok = getToken(starttimetok, ptsandseqnos->startSequenceNumber); 
        }
    }

    const char *endtimetok = strstr(file, ENDTIME_TOKEN);
    if (endtimetok)
    {
        endtimetok += strlen(ENDTIME_TOKEN);
        endtimetok = getToken(endtimetok, ptsandseqnos->lastTimeStamp);
        if (endtimetok)
        {
            endtimetok++;
            endtimetok = getToken(endtimetok, ptsandseqnos->lastSequenceNumber); 
            if (endtimetok)
            {
                efiletyp = E_DIFF;
            }
        }
    }
    else
    {
        efiletyp = E_NOTDIFF;
    }
   
    return efiletyp;
}


bool IsTso(const char *fileName)
{
    return strstr(fileName, TSO_TOKEN);
}


void PrintStats(DIFF_STATS *pDiffStats, const bool &bShouldPrint)
{
    SV_ULONGLONG numOverLaps = 0;
    StandardPrintf(bShouldPrint, stdout, "Statistics:\n");
    StandardPrintf(bShouldPrint, stdout, "Number of IO%s = " ULLSPEC "\n", (pDiffStats->m_ullNumIOs > 1)?"s":"", pDiffStats->m_ullNumIOs);
    StandardPrintf(bShouldPrint, stdout, "IO Length : Frequency:\n");
    std::map<SV_ULONGLONG, SV_ULONGLONG>::const_iterator iter = pDiffStats->m_WriteLengthFreq.begin();
    while (iter != pDiffStats->m_WriteLengthFreq.end())
    {
        StandardPrintf(bShouldPrint, stdout, ULLSPEC BLK_FREQ_SEP ULLSPEC "\n", iter->first, iter->second);
        iter++;
    }
    //StandardPrintf(bShouldPrint, stdout, "Change occurrenc regions:\n");
    std::map<SV_ULONGLONG, SV_ULONGLONG>::const_iterator overlapiter = pDiffStats->m_OverLapCount.begin();
    while (overlapiter != pDiffStats->m_OverLapCount.end())
    {
        //StandardPrintf(bShouldPrint, stdout, ULLSPEC "-->" ULLSPEC "\n", overlapiter->first, overlapiter->second);
        SV_ULONGLONG overlapcnt = (overlapiter->second - 1); 
        numOverLaps += overlapcnt;
        overlapiter++;
    }
    StandardPrintf(bShouldPrint, stdout, "Number of " ULLSPEC " Overlaps = " ULLSPEC "\n", pDiffStats->m_ullOverlapLen, numOverLaps);
}

