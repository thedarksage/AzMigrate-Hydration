//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : inmemorydifffile.cpp
//
// Description: 
//

#include "inmemorydifffile.h"
#include "diffsortcriterion.h"
#include "cdputil.h"
#include "error.h"

#include <boost/shared_array.hpp>
#include <boost/lexical_cast.hpp>
#include <ace/ACE.h>
#include "convertorfactory.h"
#include "configwrapper.h"
#include "inmalertdefs.h"

using namespace std;

InMemoryDiffFile::InMemoryDiffFile(const string & path, const char * source, SV_ULONG sourceLen)
:m_path(path),m_source(source),m_sourceLen(sourceLen)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    m_start          = 0;
    m_end            = sourceLen;
    m_curpos         = 0;
    m_baseline       = SV_WIN_OS;
    m_v2             = false;
	m_max_io_size  = 16777216; // 16 MB

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

InMemoryDiffFile::~InMemoryDiffFile()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void InMemoryDiffFile::set_max_io_size(SV_UINT& size)
{
	m_max_io_size = size;
}

void InMemoryDiffFile::ANNOUNCE_FILE()
{
    stringstream l_stdout;
    l_stdout << "Processing file:"
        << m_path
        << "\n";

    DebugPrintf(SV_LOG_DEBUG, "%s", l_stdout.str().c_str());
}

void InMemoryDiffFile::ANNOUNCE_TAG(const std::string & tagname)
{
    stringstream l_stdout;
    l_stdout << "Got:" << tagname << "\n" ;
    DebugPrintf(SV_LOG_DEBUG, "%s", l_stdout.str().c_str());
}

bool InMemoryDiffFile::ParseDiff(DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;
    bool done = false;

    int         readin = 0;
    std::string tagname;
    SVD_PREFIX  prefix = {0};

    Seek(m_start, FromBeg);

    SV_OFFSET_TYPE offset = Tell();
    bool newformat = false;

    /*
    1. init shared diff
    2. update the start offset
    3. parse and update the diff members
    4. stop after parsing tslc
    4. update end offset
    */

    do
    {

        if(!diffptr)
        {
            rv = false;
            break;
        }

        diffptr -> initialize();
		diffptr -> StartOffset(offset);
		diffptr -> EndOffset(offset);
		diffptr -> Baseline(m_baseline);

        SV_UINT dataFormatFlags ;
        readin = Read( ( char*) &dataFormatFlags, sizeof( SV_UINT ) ) ;
        if( readin == sizeof( SV_UINT ) )
        {
            if( ConvertorFactory::isOldSVDFile( dataFormatFlags ) == true )
            {
                Seek(m_start, FromBeg);
            }
            if( m_svdConvertor.get() == NULL )
            {
                ConvertorFactory::getSVDConvertor( dataFormatFlags, m_svdConvertor ) ;
            }
        }
        else
        {
            rv = false ;
            break ;
        }
        // Scan through the file one chunk at a time
        while ( offset < m_end )
        {
            // Read the chunk prefix
            /* NOTE: The record size and record number fields have intentionally    *
            *   been swapped.  This way, we can distinguish between a full prefix   *
            *   being read (result = sizeof (prefix)), an end-of-file marker in the *
            *   middle of the prefix (0 < result < sizeof (prefix), the only error  *
            *   condition), and a proper end-of-file at the end of the last chunk   *
            *   (result = 0)                                                        */

            readin = Read((char*)&prefix, SVD_PREFIX_SIZE);

            if (SVD_PREFIX_SIZE != readin)
            {
                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "\n" << GetName() << "is corrupt.\n" 
                    << "Prefix could not be be read at offset " << Tell() << "\n"
                    << " Expected Read Bytes: " << SVD_PREFIX_SIZE << "\n"
                    << " Actual Read Bytes: "  << readin << "\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

                rv = false;
                break;
            }
            else
            {
                 m_svdConvertor->convertPrefix( prefix ) ;
                stringstream l_stdfatal;
                switch (prefix.tag)
                {

                case SVD_TAG_HEADER1:

                    tagname = SVD_TAG_HEADER1_NAME;
                    ANNOUNCE_TAG(tagname);
                    rv = ReadSVD1(prefix, diffptr);
                    break;

                case SVD_TAG_HEADER_V2:

                    tagname = SVD_TAG_HEADER2_NAME;
                    ANNOUNCE_TAG(tagname);
                    m_v2=true;
                    rv = ReadSVD2(prefix, diffptr);
                    break;

                case SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE:

                    tagname = SVD_TAG_TSFC_NAME; 
                    ANNOUNCE_TAG(tagname);
                    rv = ReadTSFC(prefix, diffptr);
                    break;

                case SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2:

                    tagname = SVD_TAG_TSFC_V2_NAME; 
                    ANNOUNCE_TAG(tagname);
                    rv = ReadTFV2(prefix, diffptr);
                    break;

                case SVD_TAG_USER:

                    tagname = SVD_TAG_USER_NAME;
                    ANNOUNCE_TAG(tagname);
                    rv = ReadUSER(prefix, diffptr, dataFormatFlags);
                    break;

                case SVD_TAG_LENGTH_OF_DRTD_CHANGES:

                    tagname = SVD_TAG_LODC_NAME; 
                    ANNOUNCE_TAG(tagname);
                    rv = ReadLODC(prefix, diffptr);
                    break;

                case SVD_TAG_DATA_INFO:

                    tagname = SVD_TAG_DATA_INFO_NAME;
                    ANNOUNCE_TAG(tagname);
                    rv = ReadDATA(prefix, diffptr);
                    newformat = true;
                    break;

                case SVD_TAG_DIRTY_BLOCK_DATA:

                    tagname = SVD_TAG_DIRTY_BLOCK_DATA_NAME;
                    ANNOUNCE_TAG(tagname);
                    rv = ReadDRTD(prefix, diffptr);
                    break;

                case SVD_TAG_DIRTY_BLOCK_DATA_V2:

                    tagname = SVD_TAG_DIRTY_BLOCK_DATA_V2_NAME;
                    ANNOUNCE_TAG(tagname);
                    rv = ReadDRTDV2(prefix, diffptr);
                    break;

                case SVD_TAG_DIRTY_BLOCK_DATA_V3:

                    tagname = SVD_TAG_DIRTY_BLOCK_DATA_V3_NAME;
                    ANNOUNCE_TAG(tagname);
                    rv = ReadDRTDV3(prefix, diffptr);
                    break;


                case SVD_TAG_TIME_STAMP_OF_LAST_CHANGE:

                    tagname = SVD_TAG_TSLC_NAME;
                    ANNOUNCE_TAG(tagname);
                    rv = ReadTSLC(prefix, diffptr);
                    done = true;
                    break;

                case SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2:

                    tagname = SVD_TAG_TSLC_V2_NAME;
                    ANNOUNCE_TAG(tagname);
                    rv = ReadTLV2(prefix, diffptr);
                    done = true;
                    break;

                case SVD_TAG_SKIP:

                    tagname = SVD_TAG_SKIP_NAME;
                    ANNOUNCE_TAG(tagname);
                    rv = SkipBytes(prefix, diffptr);
                    break;

                    // Unkown chunk
                default:

                    l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                        << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                        << "\n" << GetName()   << "is corrupt.\n" 
                        << "Encountered an unknown tag: " << prefix.tag << " at offset " 
                        << Tell() << "\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
                    rv = false;
                    break;
                }
            }

            offset = Tell();		
            if ( !rv || done )
            {
                break;
            }
        }

        if ( rv )
        {
            if(!newformat)
            {
                diffptr -> EndOffset(offset);
                break;
            } else
            {
                Seek(diffptr -> EndOffset(), FromBeg);
                break;
            }
        }


    } while (0);

    if(rv)
    {
        rv = ParseandVerifyFileName(diffptr);
    }

    if(rv)
    {
        rv = VerifyIoOrdering(diffptr);
    }

	if(rv && m_volsize)
    {
        rv = VerifyDataBoundaries(diffptr);
    }
	
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::VerifyDataBoundaries(DiffPtr & diffptr)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    do
    {
        cdp_drtdv2s_iter_t drtds_iter = diffptr -> DrtdsBegin();
        cdp_drtdv2s_iter_t drtds_end = diffptr -> DrtdsEnd();
        for( ; drtds_iter != drtds_end ; ++drtds_iter)
        {
            cdp_drtdv2_t drtd = *drtds_iter;

            if(drtd.get_length() == 0)
            {
                stringstream l_stdfatal ;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "\n" << GetName()   
                    << "contains drtd of length zero.\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());

                ChangeLenZeroAlert a(GetName());

                SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);

                rv = false;
                break;
            }

            if(m_volsize && (drtd.get_volume_offset() + drtd.get_length() > m_volsize))
            {                   
                stringstream l_stdfatal ;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "\n" << GetName()   
                    << "contains data beyond source volume capacity.\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());

                ChangeBeyondSourceSizeAlert a(GetName());

                SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);

                rv = false;
                break;
            }
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::VerifyIoOrdering(DiffPtr & diffptr)
{
    bool rv = true;

	if( (diffptr -> type() == OUTOFORDER_TS) || (diffptr -> type() == OUTOFORDER_SEQ) )
	{
		// for these files we know that the timestamp/seq is out of order
		// As we let them be out of order we need not check them
		return true;
	}

    //
    // non per i/o checks
    // 1. verify end time >= start time
    // 2. end seq >= start seq
    //
    // per i/o checks
    // 3. if file is per i/o format, verify ...
    //    a) end seq > start seq
    //    b) sequenceno. delta in the file are in increasing order
    //    c) tsfc seq no + last i/o seqnodelta <= tslcseq
    //    d) TimeDelta in the file are in non decreasing order
    //    e) tsfc time + timedelta of last i/o <= tslc time

    do
    {
        SV_ULONGLONG diffStartTime = diffptr -> starttime();
        SV_ULONGLONG diffStartTimeSeq = diffptr ->StartTimeSequenceNumber();
        SV_ULONGLONG diffEndTime = diffptr -> endtime();
        SV_ULONGLONG diffEndTimeSeq = diffptr -> EndTimeSequenceNumber();

        if( diffEndTime < diffStartTime )
        {
            stringstream l_stdfatal, l_stdalert ;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()   << "is corrupt.\n" 
                << "Encountered a differential with end time less than start time\n" 
                << "Start Time:" << diffStartTime <<"\n"
                << "Start Time Seq:" << diffStartTimeSeq << "\n"
                << "End Time:" << diffEndTime << "\n"
                << "End Time Seq:" << diffEndTimeSeq <<"\n";

            DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

            l_stdalert << "Differential file : " << GetName() 
                << " has end time less than the start time\n"
                << "Differential file cannot be applied.\n"
                << "Please do a resync.\n\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());
            rv = false;
            break;
        }

        if( ( diffEndTime == diffStartTime) && (diffEndTimeSeq < diffStartTimeSeq))
        {
            stringstream l_stdfatal, l_stdalert ;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()   << "is corrupt.\n" 
                << "Encountered a differential with end sequence Id less than start sequence Id\n" 
                << "Start Time:" << diffStartTime <<"\n"
                << "Start Time Seq:" << diffStartTimeSeq << "\n"
                << "End Time:" << diffEndTime << "\n"
                << "End Time Seq:" << diffEndTimeSeq <<"\n";

            DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

            l_stdalert << "Differential file : " << GetName() 
                << " has end sequence Id less than the start sequence Id\n"
                << "Differential file cannot be applied.\n"
                << "Please do a resync.\n\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());

            rv = false;
            break;
        }


        if(diffptr ->version() >= SVD_PERIOVERSION)
        {
            SV_UINT seqdelta = 0;
            SV_UINT timedelta = 0;
            SV_UINT changeNo =0;

            cdp_drtdv2s_iter_t drtds_iter = diffptr -> DrtdsBegin();
            cdp_drtdv2s_iter_t drtds_end = diffptr -> DrtdsEnd();
            for( ; drtds_iter != drtds_end ; ++drtds_iter)
            {
                cdp_drtdv2_t drtd = *drtds_iter;
                ++changeNo;

                if(1 == changeNo)
                {
                    //
                    // first change always has sequencenumberdelta/TimeDelta as zero in data and metadata mode
                    // but it can have a non zero positive value in case of bitmap mode
                    // no checks to be done

                } else
                {
                    //
                    // sequencenumberdelta should be monontonically increasing
                    // change no N SequenceDelta > change No (N-1) Sequence Delta
                    //
					// we are also accepting equal sequencedelta
					// because of the following reason
					// if we get a drtd of length > 16M, we break into more than 
					// one
					if(drtd.get_seqdelta() < seqdelta)
                    {
                        stringstream l_stdfatal, l_stdalert ;
                        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                            << "\n" << GetName()   << "is corrupt.\n" 
                            << "Encountered a differential with a non monotonic sequence Id\n" 
                            << "Start Time:" << diffStartTime <<"\n"
                            << "Start Time Seq:" << diffStartTimeSeq << "\n"
                            << "End Time:" << diffEndTime << "\n"
                            << "End Time Seq:" << diffEndTimeSeq <<"\n"
                            << "Change Record:" << changeNo << "\n"
							<< "Sequence Number Delta:" << drtd.get_seqdelta() << "\n"
                            << "Previous change Sequence No:" << seqdelta << "\n" ;

                        DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

                        l_stdalert << "Differential file : " << GetName() 
                            << " has a non monotonic sequence Id\n"
                            << "Differential file cannot be applied.\n"
                            << "Please do a resync.\n\n";

                        DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());
                        rv = false;
                        break;
                    }

                    //
                    // Timedelta should be non decreasing. you can have same timedelta
                    // for two i/os if they occur at same time
                    // change no N TimeDelta >= change No (N-1) Time Delta
                    //
					if(drtd.get_timedelta() < timedelta)
                    {
                        stringstream l_stdfatal, l_stdalert ;
                        l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                            << "\n" << GetName()   << "is corrupt.\n" 
                            << "Encountered a differential with a decreasing time delta\n" 
                            << "Start Time:" << diffStartTime <<"\n"
                            << "Start Time Seq:" << diffStartTimeSeq << "\n"
                            << "End Time:" << diffEndTime << "\n"
                            << "End Time Seq:" << diffEndTimeSeq <<"\n"
                            << "Change Record:" << changeNo << "\n"
							<< "Time Delta:" << drtd.get_timedelta() << "\n"
                            << "Previous change Sequence No:" << timedelta << "\n" ;

                        DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

                        l_stdalert << "Differential file : " << GetName() 
                            << " has a non monotonic sequence Id\n"
                            << "Differential file cannot be applied.\n"
                            << "Please do a resync.\n\n";

                        DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());

                        rv = false;
                        break;
                    }
                }

				seqdelta = drtd.get_seqdelta();
				timedelta = drtd.get_timedelta();
            }

            if(!rv)
                break;

			if(0 == changeNo)
			{
				// below per/io checks are not applicable 
				// for tso files
				rv = true;
				break;
			}

			if(diffStartTimeSeq + seqdelta != diffEndTimeSeq)
			{
				stringstream l_stdfatal, l_stdalert ;
				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\n" << GetName()   << "is corrupt.\n" 
					<< "Encountered a differential with end sequence Id not matching the last change\n" 
					<< "Start Time:" << diffStartTime <<"\n"
					<< "Start Time Seq:" << diffStartTimeSeq << "\n"
					<< "End Time:" << diffEndTime << "\n"
					<< "End Time Seq:" << diffEndTimeSeq <<"\n"
					<< "Last Change Record Sequence Id:" << seqdelta << "\n"
					<< "Issue: StartTimeSequence + LastIoSequence != EndTimeSequence" << "\n" ;

				DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

				l_stdalert << "Differential file : " << GetName() 
					<< " has end sequence Id not matching the last change\n"
                    << "Differential file cannot be applied.\n"
					<< "Please do a resync.\n\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());

				rv = false;
				break;
			}

			if(diffStartTime + timedelta != diffEndTime)
			{
				stringstream l_stdfatal, l_stdalert ;
				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\n" << GetName()   << "is corrupt.\n" 
					<< "Encountered a differential with end timestamp not matching the last change\n" 
					<< "Start Time:" << diffStartTime <<"\n"
					<< "Start Time Seq:" << diffStartTimeSeq << "\n"
					<< "End Time:" << diffEndTime << "\n"
					<< "End Time Seq:" << diffEndTimeSeq <<"\n"
					<< "Last Change Record Time Delta:" << timedelta << "\n"
					<< "Issue: StartTimeStamp + LastIoTimeDelta != EndTimeStamp" << "\n" ;

				DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

				l_stdalert << "Differential file : " << GetName() 
					<< " has a non monotonic timestamp\n"
                    << "Differential file cannot be applied.\n"
					<< "Please do a resync.\n\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());

				rv = false;
				break;
			}
        }

        rv = true;
    } while (0);

    return rv;
}

bool InMemoryDiffFile::ParseandVerifyFileName(DiffPtr & diffptr)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    // skip past .dat[.gz]
    std::string id = basename_r(GetName().c_str());

   std::string retentionSplitIoFilePattern =  CDPDATAFILEPREFIX;
    std::string retentionNonSplitIoFilePattern =  CDPDATAFILEPREFIX;
    std::string retentionResyncFilePattern =  CDPDATAFILEPREFIX;
    std::string retentionOutOfOrderTsFilePattern = CDPDATAFILEPREFIX;
    std::string retentionOutOfOrderSeqFilePattern = CDPDATAFILEPREFIX;
    
    std::string retentionOutOfOrderTsFilePatternObs = CDPDATAFILEPREFIX;
    std::string retentionOutOfOrderSeqFilePatternObs = CDPDATAFILEPREFIX;
    
    //
    // until 4.3, we did not differentiate between split and non split i/o
    // if we get any older retention files, they are considered as non split i/o
    //
    std::string retentionDiffsyncFilePattern = CDPDATAFILEPREFIX;

    retentionResyncFilePattern += "*";
    retentionResyncFilePattern += CDPRESYNCDATAFILE;
    retentionResyncFilePattern += "*.dat" ;

    retentionDiffsyncFilePattern += "*";
    retentionDiffsyncFilePattern += CDPDIFFSYNCDATAFILE;
    retentionDiffsyncFilePattern += "*.dat" ;

    retentionNonSplitIoFilePattern += "*";
    retentionNonSplitIoFilePattern += CDPDIFFSYNCDATAFILE;
    retentionNonSplitIoFilePattern += CDPNONSPLITIOFILE;
    retentionNonSplitIoFilePattern += "*.dat" ;

    retentionSplitIoFilePattern += "*";
    retentionSplitIoFilePattern += CDPDIFFSYNCDATAFILE;
    retentionSplitIoFilePattern += CDPSPLITIOFILE;
    retentionSplitIoFilePattern += "*.dat" ;

    retentionOutOfOrderTsFilePattern += "*";
    retentionOutOfOrderTsFilePattern += CDPDIFFSYNCDATAFILE;
    retentionOutOfOrderTsFilePattern += CDPOUTOFORDERTSFILE;
    retentionOutOfOrderTsFilePattern += "*.dat" ;

    retentionOutOfOrderSeqFilePattern += "*";
    retentionOutOfOrderSeqFilePattern += CDPDIFFSYNCDATAFILE;
    retentionOutOfOrderSeqFilePattern += CDPOUTOFORDERSEQFILE;
    retentionOutOfOrderSeqFilePattern += "*.dat" ;

    retentionOutOfOrderTsFilePatternObs += "*";
    retentionOutOfOrderTsFilePatternObs += CDPDIFFSYNCDATAFILE;
    retentionOutOfOrderTsFilePatternObs += CDPOUTOFORDERTSFILEOBS;
    retentionOutOfOrderTsFilePatternObs += "*.dat" ;

    retentionOutOfOrderSeqFilePatternObs += "*";
    retentionOutOfOrderSeqFilePatternObs += CDPDIFFSYNCDATAFILE;
    retentionOutOfOrderSeqFilePatternObs += CDPOUTOFORDERSEQFILEOBS;
    retentionOutOfOrderSeqFilePatternObs += "*.dat" ;

    if(RegExpMatch(retentionResyncFilePattern.c_str(),id.c_str()))
    {
        diffptr -> type(RESYNC_MODE);
        return true;
    }

    if(RegExpMatch(retentionNonSplitIoFilePattern.c_str(),id.c_str()))
    {
		diffptr -> type(NONSPLIT_IO);
        return true;
    }

    if(RegExpMatch(retentionSplitIoFilePattern.c_str(), id.c_str()))
    {
        diffptr -> type(SPLIT_IO);
        return true;
    }

    if(RegExpMatch(retentionOutOfOrderTsFilePattern.c_str(), id.c_str()))
    {
        diffptr -> type(OUTOFORDER_TS);
        return true;
    }

    if(RegExpMatch(retentionOutOfOrderSeqFilePattern.c_str(), id.c_str()))
    {
        diffptr -> type(OUTOFORDER_SEQ);
        return true;
    }

    if(RegExpMatch(retentionOutOfOrderTsFilePatternObs.c_str(),id.c_str()))
    {
        diffptr -> type(OUTOFORDER_TS);
        return true;
    }

    if(RegExpMatch(retentionOutOfOrderSeqFilePatternObs.c_str(), id.c_str()))
    {
        diffptr -> type(OUTOFORDER_SEQ);
        return true;
    }

    if(RegExpMatch(retentionDiffsyncFilePattern.c_str(), id.c_str()))
    {
        diffptr -> type(NONSPLIT_IO);
        return true;
    }

    // if the file name does not match any of the above and it is not a differential file
    // we will assume it is a resync file

    const std::string strdiffMatchPrefix = diffMatchPrefix;
    if(0 != strncmp(id.c_str(), strdiffMatchPrefix.c_str(), strdiffMatchPrefix.length()))
    {
        diffptr -> type(RESYNC_MODE);
        return true;
    }

    do
    {
        try
        {
			DiffSortCriterion diffSortCriterion(id.c_str(), "");
			diffptr -> IdVersion(diffSortCriterion.IdVersion());
			diffptr -> SetContinuationId(diffSortCriterion.ContinuationId());
			if(diffSortCriterion.IsNonSplitIo())
			{
				diffptr -> type(NONSPLIT_IO);
			}
			else
			{
				diffptr -> type(SPLIT_IO);
			}

			SV_ULONGLONG tslc = diffSortCriterion.EndTime();
			if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
			{
				tslc += SVEPOCHDIFF;
			}

            if (tslc != diffptr -> endtime())
            {
                stringstream l_stdfatal, l_stdalert ;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "\n" << GetName()   << "is not a valid differential file name.\n"
                    << "End TimeStamp in file name: " << tslc << "\n"
                    << "End Timestamp as per file contents: " << diffptr -> endtime() << "\n\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

                l_stdalert << "Differential file : " << GetName() 
                    << " is not a valid differential file name.\n"
                << "Differential file cannot be applied.\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());

                rv = false;
                break;
            }

            if (diffSortCriterion.EndTimeSequenceNumber() != diffptr -> EndTimeSequenceNumber())
            {
                stringstream l_stdfatal, l_stdalert ;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "\n" << GetName()   << "is not a valid differential file name.\n"
                    << "End Sequence in file name: " << diffSortCriterion.EndTimeSequenceNumber() << "\n"
                    << "End Sequence as per file contents: " << diffptr -> EndTimeSequenceNumber()
                    << "\n\n";

                DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

                l_stdalert << "Differential file : " << GetName() 
                    << " is not a valid differential file name.\n"
                    << "Differential file cannot be applied.\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());

                rv = false;
                break;
            }

            if (diffptr-> IdVersion() == DIFFIDVERSION2 &&
				diffSortCriterion.ResyncCounter() != diffptr -> ResyncCounter())
            {
                stringstream l_stdfatal, l_stdalert ;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "\n" << GetName()   << "is not a valid differential file name.\n"
                    << "Resync Counter in file name: " << diffSortCriterion.ResyncCounter() << "\n"
                    << "Resync Counter as per file contents: " << diffptr -> ResyncCounter()
                    << "\n\n";

                DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

                l_stdalert << "Differential file : " << GetName() 
                    << " is not a valid differential file name.\n"
                    << "Differential file cannot be applied.\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());

                rv = false;
                break;
            }

            if (diffptr-> IdVersion() == DIFFIDVERSION2 &&
				diffSortCriterion.DirtyBlockCounter() != diffptr -> DirtyBlockCounter())
            {
                stringstream l_stdfatal, l_stdalert ;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "\n" << GetName()   << "is not a valid differential file name.\n"
                    << "Dirty Block Counter in file name: " << diffSortCriterion.DirtyBlockCounter() << "\n"
                    << "Dirty Block Counter as per file contents: " << diffptr -> DirtyBlockCounter()
                    << "\n\n";

                DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

                l_stdalert << "Differential file : " << GetName() 
                    << " is not a valid differential file name.\n"
                    << "Differential file cannot be applied.\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());

                rv = false;
                break;
            }
        }
		catch (boost::bad_lexical_cast& e) 
        {
            stringstream l_stdfatal, l_stdalert ;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()   << "is not a valid differential file name.\n"
                << e.what() << "\n";

            DebugPrintf(SV_LOG_DEBUG, "%s", l_stdfatal.str().c_str());

            l_stdalert << "Differential file : " << GetName() 
                << " is not a valid differential file name.\n"
                << "Differential file cannot be applied.\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdalert.str().c_str());

            rv = false;
            break;
        }
    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool InMemoryDiffFile::ReadSVD1(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    assert(prefix.count == 1);

    bool rv = false;
    int readin = 0;

    // Read the header chunk
	SVD_HEADER1 hdr;
    readin = FullRead((char*)(&hdr),SVD_HEADER1_SIZE);
    if (SVD_HEADER1_SIZE == readin)
    {
		m_svdConvertor->convertHeader( hdr ) ;
        ANNOUNCE_SVDATA(hdr);

		diffptr -> Hdr(hdr);
        rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::ReadSVD2(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    assert(prefix.count == 1);

    bool rv = false;
    int readin = 0;

    // Read the header chunk
	SVD_HEADER_V2 hdr;
    readin = FullRead((char*)(&hdr),SVD_HEADER2_SIZE); 
    if (SVD_HEADER2_SIZE == readin)
    {
		m_svdConvertor->convertHeaderV2( hdr ) ;
		ANNOUNCE_SVDATA(hdr);
		diffptr -> Hdr(hdr);
        rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::ReadTSFC(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    assert(prefix.count == 1);

    bool rv = false;
    int readin = 0;

    // Read the timestamp chunk
    SVD_TIME_STAMP tsfc;
    readin = FullRead((char*)&(tsfc),SVD_TIMESTAMP_SIZE);

    if (SVD_TIMESTAMP_SIZE == readin)
    {
        m_svdConvertor->convertTimeStamp( tsfc ) ;
        ANNOUNCE_SVDATA(tsfc);
		if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
		{
			tsfc.TimeInHundNanoSecondsFromJan1601 += SVEPOCHDIFF;
		}

		diffptr -> StartTime(tsfc);

        rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::ReadTFV2(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    assert(prefix.count == 1);

    bool rv = false;
    int readin = 0;

    // Read the timestamp chunk
	SVD_TIME_STAMP_V2 tsfc;
    readin = FullRead((char*)&(tsfc),SVD_TIMESTAMP_V2_SIZE);

    if (SVD_TIMESTAMP_V2_SIZE == readin)
    {
        m_svdConvertor->convertTimeStampV2( tsfc );
        ANNOUNCE_SVDATA(tsfc);

		if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
		{
			tsfc.TimeInHundNanoSecondsFromJan1601 += SVEPOCHDIFF;
		}

		diffptr -> StartTime(tsfc);

        //
        // temporary hack until source side starts sending proper header
        //
        if(!m_v2)
        {
            diffptr -> version(SVD_PERIOVERSION_MAJOR, SVD_PERIOVERSION_MINOR);
        }
		rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::ReadTSLC(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    assert(prefix.count == 1);

    bool rv = false;
    int readin = 0;

    // Read the timestamp chunk
    SVD_TIME_STAMP tslc;
    readin = FullRead((char*)&(tslc),SVD_TIMESTAMP_SIZE);

    if (SVD_TIMESTAMP_SIZE == readin)
    {
        m_svdConvertor->convertTimeStamp( tslc ) ;
        ANNOUNCE_SVDATA(tslc);

		if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
		{
			tslc.TimeInHundNanoSecondsFromJan1601 += SVEPOCHDIFF;
		}

        diffptr -> EndTime(tslc);
        rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::ReadTLV2(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    assert(prefix.count == 1);

    bool rv = false;
    int readin = 0;

    // Read the timestamp chunk
	SVD_TIME_STAMP_V2 tslc;
    readin = FullRead((char*)&(tslc),SVD_TIMESTAMP_V2_SIZE);

    if (SVD_TIMESTAMP_V2_SIZE == readin)
    {
        m_svdConvertor->convertTimeStampV2( tslc ); 
        ANNOUNCE_SVDATA(tslc);

		if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
		{
			tslc.TimeInHundNanoSecondsFromJan1601 += SVEPOCHDIFF;
		}

		diffptr -> EndTime(tslc);

        rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::ReadUSER(const SVD_PREFIX & prefix, DiffPtr & diffptr, SV_UINT& dataFormatFlags)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    assert(prefix.count == 1);

    bool rv = false;
    SV_ULONG readin = 0;

    do 
    {
        boost::shared_array<char> buffer(new (nothrow) char[prefix.Flags]);
        if ( !buffer )
        {
            break;
        }

        // Read the user chunk
        readin = FullRead(buffer.get(), prefix.Flags);

        if ( prefix.Flags != readin )
        {
            break;
        }

        boost::shared_ptr<SV_MARKER> marker(new (nothrow) SV_MARKER(buffer, prefix.Flags));
        rv = marker ->Parse(dataFormatFlags);

        if(rv)
        {
            diffptr -> AddBookMark(marker);
            ANNOUNCE_SVDATA(*marker.get());
        }else
        {
            DebugPrintf(SV_LOG_ERROR, "Encountered an unparseable Tag in %s.Ignoring...\n", m_path.c_str());
        }

    } while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    // we ignore tags which could not be parsed
    // return rv;
    return true;
}

bool InMemoryDiffFile::ReadLODC(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    assert(prefix.count == 1);

    bool rv = false;
    SV_UINT readin = 0;
    SV_OFFSET_TYPE offset;
    SVD_PREFIX pfx;

    do {

        // Read the timestamp chunk
		SV_ULARGE_INTEGER lodc;
        readin = FullRead((char*)&(lodc), SVD_LODC_SIZE);

        lodc.QuadPart = m_svdConvertor->getBaseConvertor()->convertULONGLONG( lodc.QuadPart ); 

        if (SVD_LODC_SIZE != readin)
        {
            rv = false;
            break;
        }


        //Store the current offset
        offset = Tell();

        if((SV_OFFSET_TYPE)(offset + lodc.QuadPart) > m_end)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()    << "is corrupt.\n" 
                << "Encountered Length of data changes  greater than end offset of the file " << "\n"
				<< "Length of Data Changes " << lodc.QuadPart << "\n"
                << "Current Offset: " << offset << "\n"
                << "End Offset: " << m_end << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

            rv = false;
            break;
        }

        //Seek to the end of LODC chunk
        Seek((SV_OFFSET_TYPE)lodc.QuadPart, FromCur);

        //After LODC, we expect a TSLC tag. Validate it.
        readin = FullRead((char*)&pfx,SVD_PREFIX_SIZE);

        m_svdConvertor->convertPrefix( pfx ) ;
        if (SVD_PREFIX_SIZE != readin )
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()    << "is corrupt.\n" 
                << "FAILED to read expected SVD_PREFIX tag after DRTD changes " << "\n"
                << " Expected Read Bytes: " << SVD_PREFIX_SIZE << "\n"
                << " Actual Read Bytes: "  << readin << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        if(pfx.tag != SVD_TAG_TIME_STAMP_OF_LAST_CHANGE && pfx.tag != SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2)
        {	
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()    << "is corrupt.\n" 
                << "Error detected  After DRTD changes, expecting TSLC/TLV2 but got tag " 
                <<  pfx.tag << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        //Restore the offset.
        Seek(offset, FromBeg);
		diffptr -> LengthOfDataChanges(lodc.QuadPart);
        rv = true;
        ANNOUNCE_SVDATA(lodc);

    } while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool InMemoryDiffFile::ReadDATA(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    assert(prefix.count == 1);

    bool rv = true;
    SV_UINT readin = 0;
    SV_OFFSET_TYPE offset;
    SVD_PREFIX pfx;
    SVD_DATA_INFO dataInfo;
    std::string tagname;

    do {

        // Read the timestamp chunk
        readin = FullRead((char*)&(dataInfo), SVD_DATA_INFO_SIZE);

        if (SVD_DATA_INFO_SIZE != readin)
        {
            rv = false;
            break;
        }

        /* NEEDTOSWAP : START */
        m_svdConvertor->convertSVDDataInfo( dataInfo ) ;
        /* NEEDTOSWAP : END */

        diffptr -> LengthOfDataChanges(dataInfo.drtdLength.QuadPart);
        diffptr -> EndOffset(dataInfo.diffEndOffset);
        ANNOUNCE_SVDATA(dataInfo);


        //Store the current offset
        offset = Tell();

        if((SV_OFFSET_TYPE)(dataInfo.drtdStartOffset + dataInfo.drtdLength.QuadPart) > m_end)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()    << "is corrupt.\n" 
                << "Encountered Length of data changes  greater than end offset of the file " << "\n"
                << "Data Start Offset: " << dataInfo.drtdStartOffset << "\n"
                << "Data Length: " << dataInfo.drtdLength << "\n"
                << "End Offset: " << m_end << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

            rv = false;
            break;
        }

        if(dataInfo.diffEndOffset > m_end)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()    << "is corrupt.\n" 
                << "Encountered differential end offset value greater than end of file " << "\n"
                << "Differential End Offset: " << dataInfo.diffEndOffset << "\n"
                << "End Offset: " << m_end << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());

            rv = false;
            break;
        }

        //Seek to the start of data chunk
        Seek(dataInfo.drtdStartOffset, FromBeg);

        //At dataStartOffset, we expect a DDV3 tag. Validate it.
        readin = FullRead((char*)&pfx,SVD_PREFIX_SIZE);
        if (SVD_PREFIX_SIZE != readin )
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()    << "is corrupt.\n" 
                << "FAILED to read expected SVD_PREFIX tag after DRTD changes " << "\n"
                << " Expected Read Bytes: " << SVD_PREFIX_SIZE << "\n"
                << " Actual Read Bytes: "  << readin << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        if(pfx.tag != SVD_TAG_DIRTY_BLOCK_DATA_V3)
        {	
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()    << "is corrupt.\n" 
                << "Offset: " << dataInfo.drtdStartOffset
                << "Error detected  after DATA tag, expecting DDV3 but got tag " 
                <<  pfx.tag << "\n";
                
            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        tagname = SVD_TAG_DATA_INFO_NAME;
        ANNOUNCE_TAG(tagname);

        rv = ReadDRTDV3(pfx, diffptr);

        //Restore the offset.
        Seek(offset, FromBeg);  

    } while (0);


    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::SkipBytes(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    // skip specified bytes
    Seek(prefix.count, FromCur);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool InMemoryDiffFile::ReadDRTD(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    bool rv =true;
    SV_ULONG i = 0;
    SV_UINT  readin = 0;

    SV_OFFSET_TYPE   offset = 0;
    SVD_DIRTY_BLOCK header = {0};

	SV_UINT length_processed = 0;
	SV_UINT length_pending = 0;

    // Cycle through all records, reading each one
    for (i = 0; i < prefix.count; i++)
    {	
        stringstream l_stdout;
        l_stdout << "Processing Record: " << i << endl;
        DebugPrintf(SV_LOG_DEBUG, "%s", l_stdout.str().c_str());

        // Read the record header
        readin = Read((char*)&header, SVD_DIRTY_BLOCK_SIZE);
        m_svdConvertor->convertDBHeader( header ) ;
        if ( SVD_DIRTY_BLOCK_SIZE != readin)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "\n" << GetName()    << "is corrupt.\n" 
                << " Record " << (i+1) << " of " << SVD_DIRTY_BLOCK_SIZE << " does not exist" << "\n" 
                << " Expected Read Bytes: " << SVD_PREFIX_SIZE << "\n"
                << " Actual Read Bytes: "  << readin << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        offset = Tell();

        // If, for some reason we're biting off more than we can chew we stop
        if (header.Length > ULONG_MAX)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "\n" << GetName()    << "is corrupt.\n"
                << "Block header length " << (unsigned long)header.Length << 
                "exceeds max limit" << ULONG_MAX << "\n" ;

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        //Check if we are crossing an end-of-file mark.
        if ( (offset + header.Length) > m_end) {

            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "\n" << GetName()    << "is corrupt.\n"
                << "Encountered unexpected end-of-file at offset " << m_end << "\n"
                << " While trying to seek " << header.Length << " bytes from offset "
                << offset << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        // Try to skip past the actual data.
        Seek((SV_OFFSET_TYPE)header.Length, FromCur);

		if(header.Length <= m_max_io_size)
		{
			cdp_drtdv2_t drtd(header.Length,header.ByteOffset,offset,0,0,0);
			diffptr -> AddDRTD(drtd);
			ANNOUNCE_SVDATA(drtd);
		} else
		{
			length_processed = 0;
			length_pending = header.Length;
			while(length_pending > 0)
			{
				cdp_drtdv2_t sub_drtd(min(m_max_io_size, length_pending),
					header.ByteOffset +length_processed,
					offset + length_processed,
					0,0,0);

				length_processed += sub_drtd.get_length();
				length_pending -= sub_drtd.get_length();
				diffptr -> AddDRTD(sub_drtd);
				ANNOUNCE_SVDATA(sub_drtd);
			}
		}

        rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool InMemoryDiffFile::ReadDRTDV2(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    bool rv =true;
    SV_ULONG i = 0;
    SV_UINT  readin = 0;

    SV_OFFSET_TYPE   offset = 0;
    SVD_DIRTY_BLOCK_V2 header = {0};

	SV_UINT length_processed = 0;
	SV_UINT length_pending = 0;

    // Cycle through all records, reading each one
    for (i = 0; i < prefix.count; i++)
    {	
        stringstream l_stdout;
        l_stdout << "Processing Record: " << i << endl;
        DebugPrintf(SV_LOG_DEBUG, "%s", l_stdout.str().c_str());

        // Read the record header
        readin = Read((char*)&header, SVD_DIRTY_BLOCK_V2_SIZE);
        m_svdConvertor->convertDBHeaderV2( header ) ;
        if ( SVD_DIRTY_BLOCK_V2_SIZE != readin)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "\n" << GetName()    << "is corrupt.\n" 
                << " Record " << (i+1) << " of SVD_DIRTY_BLOCK_V2 does not exist" << "\n" 
                << " Expected Read Bytes: " << SVD_DIRTY_BLOCK_V2_SIZE << "\n"
                << " Actual Read Bytes: "  << readin << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        offset = Tell();

        //Check if we are crossing an end-of-file mark.
        if ( (offset + header.Length) > m_end) 
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "\n" << GetName()    << "is corrupt.\n"
                << "Encountered unexpected end-of-file at offset " << m_end << "\n"
                << " While trying to seek " << header.Length << " bytes from offset "
                << offset << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        // Try to skip past the actual data.
        Seek((SV_OFFSET_TYPE)header.Length, FromCur);

		if(header.Length <= m_max_io_size)
		{
			cdp_drtdv2_t drtd(header.Length,header.ByteOffset,offset,header.uiSequenceNumberDelta, header.uiTimeDelta,0);
			diffptr -> AddDRTD(drtd);
			ANNOUNCE_SVDATA(drtd);
		} else
		{
			length_processed = 0;
			length_pending = header.Length;
			while(length_pending > 0)
			{
				cdp_drtdv2_t sub_drtd(min(m_max_io_size, length_pending),
					header.ByteOffset +length_processed,
					offset + length_processed,
					header.uiSequenceNumberDelta, header.uiTimeDelta,0);

				length_processed += sub_drtd.get_length();
				length_pending -= sub_drtd.get_length();
				diffptr -> AddDRTD(sub_drtd);
				ANNOUNCE_SVDATA(sub_drtd);
			}
		}
        rv = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool InMemoryDiffFile::ReadDRTDV3(const SVD_PREFIX & prefix, DiffPtr & diffptr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    bool rv =true;
    SV_ULONG i = 0;
    SV_UINT  readin = 0;

	SV_UINT length_processed = 0;
	SV_UINT length_pending = 0;

    SV_OFFSET_TYPE   dataStartOffset = 0;
    SV_OFFSET_TYPE   dataCurOffset = 0;
    SV_ULONGLONG     dataLength = 0;
    SVD_DIRTY_BLOCK_V2 header = {0};

    dataStartOffset = Tell();
    dataStartOffset += (prefix.count * SVD_DIRTY_BLOCK_V2_SIZE);
    dataCurOffset = dataStartOffset;


    // Cycle through all records, reading each one
    for (i = 0; i < prefix.count; i++)
    {	
        stringstream l_stdout;
        l_stdout << "Processing Record: " << i << endl;
        DebugPrintf(SV_LOG_DEBUG, "%s", l_stdout.str().c_str());

        // Read the record header
        readin = Read((char*)&header, SVD_DIRTY_BLOCK_V2_SIZE);
        m_svdConvertor->convertDBHeaderV2( header ) ;
        if ( SVD_DIRTY_BLOCK_V2_SIZE != readin)
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                << "\n" << GetName()    << "is corrupt.\n" 
                << " Record " << (i+1) << " of SVD_DIRTY_BLOCK_V2 does not exist" << "\n" 
                << " Expected Read Bytes: " << SVD_DIRTY_BLOCK_V2_SIZE << "\n"
                << " Actual Read Bytes: "  << readin << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

		if(header.Length <= m_max_io_size)
		{
			cdp_drtdv2_t drtd(header.Length,header.ByteOffset,dataCurOffset,header.uiSequenceNumberDelta, header.uiTimeDelta,0);
			diffptr -> AddDRTD(drtd);
			ANNOUNCE_SVDATA(drtd);
		} else
		{
			length_processed = 0;
			length_pending = header.Length;
			while(length_pending > 0)
			{
				cdp_drtdv2_t sub_drtd(min(m_max_io_size, length_pending),
					header.ByteOffset +length_processed,
					dataCurOffset + length_processed,
					header.uiSequenceNumberDelta, header.uiTimeDelta,0);

				length_processed += sub_drtd.get_length();
				length_pending -= sub_drtd.get_length();
				diffptr -> AddDRTD(sub_drtd);
				ANNOUNCE_SVDATA(sub_drtd);
			}
		}

        dataCurOffset += header.Length;
        dataLength += header.Length;
    }


    if(rv)
    {

        do
        {
            //Check if we are crossing an end-of-file mark.
            if ( (dataStartOffset + dataLength) > m_end) 
            {

                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                    << "\n" << GetName()    << "is corrupt.\n"
                    << "Encountered unexpected end-of-file at offset " << m_end << "\n"
                    << " While trying to seek " << dataLength << " bytes from offset "
                    << dataStartOffset  << "\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
                rv = false;
                break;
            }

            SV_OFFSET_TYPE offset = Tell();

            if(offset != dataStartOffset )
            {
                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
                    << "\n" << GetName() << " internal parser error.\n"
                    << " expected offset: " << dataStartOffset << "\n"
                    << " actual offset: "  << offset << "\n";

                DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
                rv = false;
                break;
            }

            // skip past the actual data.
            Seek((SV_OFFSET_TYPE)dataLength, FromCur);
  
        } while (0);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


SV_UINT InMemoryDiffFile::Read(char* buffer, SV_UINT length)
{
    SV_UINT bytesToRead = length;
    if(m_curpos + length > m_end )	{
        bytesToRead = static_cast<SV_UINT>( m_end - m_curpos );
    }

    if(bytesToRead == 0)
        return 0;

	inm_memcpy_s(buffer, length, m_source + m_curpos, bytesToRead);
    m_curpos += bytesToRead;
    return bytesToRead;
}

SV_ULONG InMemoryDiffFile::FullRead(char* buffer, SV_ULONG length)
{	 	 	
    SV_ULONG bytesToRead = length;
    if(m_curpos + length > m_end )	{
        bytesToRead = static_cast<SV_UINT>( m_end - m_curpos );
    }

    if(bytesToRead == 0)
        return 0;

	inm_memcpy_s(buffer, length, m_source + m_curpos, bytesToRead);
    m_curpos += bytesToRead;
    return bytesToRead;
}

SV_LONGLONG InMemoryDiffFile::Seek(SV_LONGLONG offset, SeekFrom from)
{    
    switch (from) {
         case FromBeg:
             if(m_start + offset > m_end)
                 m_curpos = m_end;
             else
                 m_curpos = m_start + offset;

             break;
         case FromCur:
             if(m_curpos + offset > m_end)
                 m_curpos = m_end;
             else
                 m_curpos = m_curpos + offset;

             break;
    }

    return m_curpos;
}


//=========================================================================

