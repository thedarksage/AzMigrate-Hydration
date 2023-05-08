//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : CDPV1DataFileReader.cpp
//
// Description: 
//

#include "cdpv1datafilereader.h"
#include "cdputil.h"
#include "error.h"
#include <boost/lexical_cast.hpp>

using namespace std;

CDPV1DataFileReader::CDPV1DataFileReader(const string & name)
:m_file(name),CDPDataFileReader(name)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    m_start = 0;
    m_end   = 0;
    m_baseline = SV_WIN_OS;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

CDPV1DataFileReader::~CDPV1DataFileReader()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool CDPV1DataFileReader::Init()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = false;

    do
    {
        // we do not download tso files as the filename contains complete information
        if(tsoFile())
        {
            m_start =0;
            m_end = TSO_FILE_SIZE;
            rv = true;
            break;
        }

        if( SV_SUCCESS != m_file.Open(BasicIo::BioOpen |BasicIo::BioRead |  BasicIo::BioShareRW | BasicIo::BioBinary) )
        {
            rv = false;
            break;
        }

        m_file.Seek(0, BasicIo::BioEnd);
        if(!m_file.Good())
        {
            break;
        }

        m_start = 0;
        m_end   = m_file.Tell();
        rv = true;

    } while (FALSE);

    if (!m_file.Good())
    {
        stringstream l_stderr;
        l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << "Error while initializing " << m_file.GetName() << " for read operation.\n"
            << "Error Code: " << m_file.LastError() << "\n"
            << "Error Message: " << Error::Msg(m_file.LastError()) << "\n"
            << USER_DEFAULT_ACTION_MSG << "\n";


        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        rv = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1DataFileReader::Parse()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    if(tsoFile())
    {
        return ParseTsoFile();
    }

    m_diffs.clear();
    m_file.ANNOUNCE_FILE();
    m_file.Seek(m_start, BasicIo::BioBeg);
    SV_OFFSET_TYPE offset = m_file.Tell();

    while ( offset < m_end )
    {
        m_file.SetStartOffset(offset);
        m_file.SetEndOffset(m_end);

        DiffPtr diffptr(new (nothrow) Differential);
        if(!diffptr)
        {
            rv = false;
            break;
        }

        bool rv_parsediff = m_file.ParseDiff(diffptr);
        if(!rv_parsediff)
        {
            rv = false;
            break;
        }

        m_diffs.push_back(diffptr);
        offset = m_file.Tell();
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

//bool CDPV1DataFileReader::FetchDRTDReader(const cdp_drtd_t &drtd, boost::shared_ptr<DRTDReader> & sp)
//{
//    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
//    bool rv = false;
//
//    CDPV1DataFileReader::CDPV1DRTDReader * drtdreader 
//        = new (nothrow) CDPV1DataFileReader::CDPV1DRTDReader(m_file, drtd);
//    if(!drtdreader)
//    {
//        rv = false;
//    }
//    else
//    {
//        sp.reset(drtdreader);
//        rv = true;
//    }
//
//    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
//    return rv;
//}
//
//bool CDPV1DataFileReader::FetchDRTDReader(const cdp_drtdv2_t &drtdv2, boost::shared_ptr<DRTDReader> & sp)
//{
//    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
//    bool rv = false;
//
//    do
//    {
//		cdp_drtdptr_t change(new (nothrow) cdp_drtd_t(drtdv2.get_length(),
//			drtdv2.get_volume_offset(), drtdv2.get_file_offset()));
//
//        if(!change)
//        {
//            stringstream l_stdfatal;
//            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
//                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
//                << "Error: insufficient memory to allocate buffers\n";
//
//            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
//            rv = false;
//            break;
//        }
//
//
//        CDPV1DataFileReader::CDPV1DRTDReader * drtdreader 
//            = new (nothrow) CDPV1DataFileReader::CDPV1DRTDReader(m_file, *(change.get()));
//        if(!drtdreader)
//        {
//            stringstream l_stdfatal;
//            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
//                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
//                << "Error: insufficient memory to allocate buffers\n";
//
//            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
//            rv = false;
//            break;
//        }
//        else
//        {
//            sp.reset(drtdreader);
//            rv = true;
//        }
//
//    } while (0);
//
//    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
//    return rv;
//}

//CDPV1DataFileReader::CDPV1DRTDReader::CDPV1DRTDReader(DifferentialFile & diff_file, const cdp_drtd_t & drtd)
//:m_file(diff_file), m_drtd(drtd), m_bytesRead(0)
//{
//    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
//
//    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
//}
//
//SV_UINT CDPV1DataFileReader::CDPV1DRTDReader::read(char * buffer, SV_UINT length)
//{
//    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
//    SV_UINT readin = 0;
//
//	m_file.Seek( (m_drtd.get_file_offset()+ m_bytesRead), BasicIo::BioBeg);
//    readin = m_file.FullRead(buffer,length);
//
//    if( readin != length )
//    {
//        stringstream l_stdfatal;
//        l_stdfatal   << "Error detected  " << "in Function:" << FUNCTION_NAME 
//            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
//            << "Error during read operation on differential file:" << m_file.GetName() << "\n"
//			<< " Offset : " << ( m_drtd.get_file_offset()+ m_bytesRead ) << "\n"
//            << "Expected Read:" << length << "\n"
//            << "Actual Read:"   << readin << "\n"
//            << " Error Code: " << m_file.LastError() << "\n"
//            << " Error Message: " << Error::Msg(m_file.LastError()) << "\n";
//
//        DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
//    }
//
//    if ( readin )
//        m_bytesRead += readin;
//
//    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
//    return readin;
//}

bool CDPV1DataFileReader::ParseTsoFile()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    m_diffs.clear();

    do
    {

        DiffPtr diffptr(new (nothrow) Differential);
        if(!diffptr)
        {
            rv = false;
            break;
        }

		diffptr -> initialize();
		diffptr -> StartOffset(m_start);
		diffptr -> EndOffset(m_end);

        // skip past .dat[.gz]
        std::string id = GetBaseName();
        std::string::size_type dotPos = id.rfind(".dat");
        // find the last '_' 
        std::string::size_type continuationTagEndPos = id.rfind("_", dotPos - 1);

        std::string::size_type continuationTagPos = id.rfind("_", continuationTagEndPos - 1);
        if (std::string::npos == continuationTagPos 
            || !('W' == id[continuationTagPos + 1] 
            || 'M' == id[continuationTagPos + 1]
            || 'F' == id[continuationTagPos + 1])) {

                // OK using an old format
                diffptr -> SetContinuationId(static_cast<SV_UINT>(-1));

                // get cx time stamp
                std::string::size_type cxTimeStamp= id.rfind("_", dotPos - 1);

                // in this case use the cx timestamp as both start and end time
				SV_ULONGLONG ts = boost::lexical_cast<unsigned long long>(id.substr(cxTimeStamp + 1, dotPos - (cxTimeStamp + 1)).c_str());
                diffptr -> StartTime(ts);
				diffptr -> EndTime(ts);
            } else 	 {

				SV_UINT ctid = 
                    boost::lexical_cast<SV_UINT>(id.substr(continuationTagPos + 3, 
                    continuationTagEndPos - (continuationTagPos + 3)).c_str());

                diffptr -> SetContinuationId(ctid);

                if(('E' == id[continuationTagPos + 2]) && 
                    ((diffptr -> ContinuationId() == 1) || (diffptr -> ContinuationId() == 0)))
                {
                    diffptr -> type(NONSPLIT_IO);
                } else
                {
                    diffptr -> type(SPLIT_IO);
                }

                // get end time sequence number
                std::string::size_type endTimeSequnceNumberPos = id.rfind("_", continuationTagPos - 1);
				SV_ULONGLONG tslc_seq = boost::lexical_cast<unsigned long long>(id.substr(endTimeSequnceNumberPos + 1, continuationTagPos - (endTimeSequnceNumberPos + 1)).c_str());
                diffptr -> EndTimeSequenceNumber(tslc_seq);

                // get end time
                std::string::size_type endTimePos = id.rfind("_", endTimeSequnceNumberPos - 1);
				SV_ULONGLONG tslc = boost::lexical_cast<unsigned long long>(id.substr(endTimePos + 2, endTimeSequnceNumberPos - (endTimePos + 2)).c_str());

                if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
                {
                    tslc += SVEPOCHDIFF;
                }
                diffptr -> EndTime(tslc);

                // get start time sequence number
                std::string::size_type startTimeSequenceNumberPos = id.rfind("_", endTimePos - 1);
				SV_ULONGLONG tsfc_seq = boost::lexical_cast<unsigned long long>(id.substr(startTimeSequenceNumberPos + 1, endTimePos - (startTimeSequenceNumberPos + 1)).c_str());
                diffptr -> StartTimeSequenceNumber(tsfc_seq);


                // get start time
                std::string::size_type startTimePos = id.rfind("_", startTimeSequenceNumberPos - 1);
				SV_ULONGLONG tsfc = boost::lexical_cast<unsigned long long>(id.substr(startTimePos + 2, startTimeSequenceNumberPos - (startTimePos + 2)).c_str());

                if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
                {
                    tsfc += SVEPOCHDIFF;
                }
				diffptr -> StartTime(tslc);
            }

            m_diffs.push_back(diffptr);

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}
