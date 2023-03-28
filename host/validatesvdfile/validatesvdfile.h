//
// validatesvdfile.h: validate SV files
//
#ifndef _VALIDATESVDFILE_H_
#define _VALIDATESVDFILE_H_

#include <list>
#include <string>
#include <map>

#include <boost/function.hpp>
#include <boost/bind.hpp>

typedef enum eValidateSvdLibUsedBy
{
    E_NOPRINT,
    E_PRINT

} E_VALIDATESVDLIB_USEDBY;

typedef enum eFileType
{
    E_DIFF,
    E_NOTDIFF,
    E_BAD

} E_FILE_TYPE;

#define STARTTIME_TOKEN "_S"
#define ENDTIME_TOKEN "_E"
#define PREVENDTIME_TOKEN "_P"
#define TSO_TOKEN "tso_"

struct stTimeStampsAndSeqNos
{
    SV_ULONGLONG startTimeStamp;
    SV_ULONGLONG startSequenceNumber;
    SV_ULONGLONG lastTimeStamp;
    SV_ULONGLONG lastSequenceNumber;
    stTimeStampsAndSeqNos()
    {
        startTimeStamp = startSequenceNumber = lastTimeStamp = lastSequenceNumber = 0;
    }
    void zeroAll(void)
    {
        startTimeStamp = startSequenceNumber = lastTimeStamp = lastSequenceNumber = 0;
    }
};
typedef struct stTimeStampsAndSeqNos stTimeStampsAndSeqNos_t;

/* command line and print options */
typedef struct stCmdLineAndPrintOpts
{
    bool m_bvboseEnabled;           /* verbose flag [-v] */
    bool m_bstatEnabled;            /* statistics flag */
    bool m_bShouldPrint;            /* print flag */
    SV_ULONGLONG m_ullWrUnitLen;    /* write length unit for frequency of IOPs */
    SV_ULONGLONG m_ullOverlapLen;   /* overlap block length */

    stCmdLineAndPrintOpts()
    {
        m_bvboseEnabled = m_bstatEnabled = m_bShouldPrint = false;
        m_ullWrUnitLen = m_ullOverlapLen = 0;
    }

} stCmdLineAndPrintOpts_t;

#define WRITELENGTH_UNIT 4096
#define OVERLAP_LENGTH WRITELENGTH_UNIT

#define BLK_FREQ_SEP " : "

typedef struct stDiffStats
{
    std::string m_fileName;
    SV_ULONGLONG m_ullNumIOs;
    std::map<SV_ULONGLONG, SV_ULONGLONG> m_WriteLengthFreq;
    std::map<SV_ULONGLONG, SV_ULONGLONG> m_OverLapCount;
    SV_ULONGLONG m_ullWrUnitLen;
    SV_ULONGLONG m_ullOverlapLen;

    stDiffStats()
    {
        m_ullNumIOs = 0;
        m_ullWrUnitLen = WRITELENGTH_UNIT;
        m_ullOverlapLen = OVERLAP_LENGTH;
    }

    void FillWriteLengthFreq( SV_ULONGLONG writeLength )
    {
        SV_ULONGLONG quotient = writeLength / m_ullWrUnitLen;
        SV_ULONGLONG firstval = (quotient * m_ullWrUnitLen);
        m_WriteLengthFreq[firstval]++;
    }

    void FillOverLapCount(SV_ULONGLONG offset, SV_ULONGLONG length)
    {
        SV_ULONGLONG offsetLenAdded = offset + length;
        SV_ULONGLONG quotient = offsetLenAdded / m_ullOverlapLen;
        SV_ULONGLONG firstval = (quotient * m_ullOverlapLen);
        m_OverLapCount[firstval]++;
    }

} DIFF_STATS;

typedef std::list<std::string> Checksums_t ;
E_FILE_TYPE ParseFileName(const char *fileName,
                          stTimeStampsAndSeqNos_t *ptsandseqnos);
int ValidateSVFile(
    const char *fileName, 
    const stCmdLineAndPrintOpts_t *pclinepropts,
    stTimeStampsAndSeqNos_t *pTsAndSeqnosRead,
    Checksums_t* checksums
    );

int ValidateSVBuffer(
    char *buffer,
    const size_t bufferlen,
    const char *fileName,
    const stCmdLineAndPrintOpts_t *pclinepropts,
    stTimeStampsAndSeqNos_t *pTsAndSeqnosRead,
    Checksums_t* checksums
    );

int ValidateSVStream(
    void *in,
    const char *fileName,
    const stCmdLineAndPrintOpts_t *pclinepropts,
    stTimeStampsAndSeqNos_t *pTsAndSeqnosRead,
    Checksums_t* checksums
    );

void SetValidateSvdLogCallback(boost::function<void(unsigned int loglevel, const char *msg)> callback);

#endif /* endif of _VALIDATESVDFILE_H_ */

