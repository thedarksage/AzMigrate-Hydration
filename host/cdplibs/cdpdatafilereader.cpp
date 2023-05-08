//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpdatafilereader.cpp
//
// Description: 
//

#include "cdpdatafilereader.h"

using namespace std;

CDPDataFileReader::CDPDataFileReader(const string & path)
:m_path(path), m_source(NULL), m_sourceLen(0)
{
    std::vector<char> vBasename(SV_MAX_PATH, '\0');
    BaseName(m_path.c_str(), &vBasename[0], vBasename.size());

    m_basename = std::string(vBasename.data());

    std::string::size_type idx = m_basename.find("_tso_");
    if (std::string::npos == idx ) {
        m_btsoFile = false;
    } else {
        m_btsoFile = true;
    }
}


CDPDataFileReader::CDPDataFileReader(const string & path, char * source, SV_ULONG sourceLen)
:m_path(path),m_source(source),m_sourceLen(sourceLen),m_btsoFile(false)
{
    std::vector<char> vBasename(SV_MAX_PATH, '\0');
    BaseName(m_path.c_str(), &vBasename[0], vBasename.size());

    m_basename = std::string(vBasename.data());
}


SV_ULONGLONG CDPDataFileReader::StartTime() 
{
    DiffIterator diff_iter = DiffsBegin();
    if(diff_iter != DiffsEnd())
    {
        DiffPtr diffptr = *diff_iter;
        return diffptr -> starttime();
    }

    return 0;
}

SV_ULONGLONG CDPDataFileReader::EndTime()
{
    DiffRevIterator diff_iter = m_diffs.rbegin();
    if(diff_iter != m_diffs.rend())
    {
        DiffPtr diffptr = *diff_iter;
        return diffptr -> endtime();
    }

    return 0;
}

SV_ULONGLONG CDPDataFileReader::StartSeq() 
{
    DiffIterator diff_iter = DiffsBegin();
    if(diff_iter != DiffsEnd())
    {
        DiffPtr diffptr = *diff_iter;
        return diffptr -> StartTimeSequenceNumber();
    }

    return 0;
}

SV_ULONGLONG CDPDataFileReader::EndSeq() 
{
    DiffRevIterator diff_iter = m_diffs.rbegin();
    if(diff_iter != m_diffs.rend())
    {
        DiffPtr diffptr = *diff_iter;
        return diffptr -> EndTimeSequenceNumber();
    }

    return 0;
}

SV_ULONGLONG CDPDataFileReader::PhysicalSize()
{
    if(tsoFile())
        return TSO_FILE_SIZE;

    if(m_source)
        return m_sourceLen;
    return File::GetSizeOnDisk(m_path);
}

SV_UINT CDPDataFileReader::NumEvents()
{
    DiffIterator diff_iter = DiffsBegin();
    DiffIterator diff_end = DiffsEnd();
    SV_UINT numevents =0;

    for ( ; diff_iter != diff_end ; ++diff_iter)
    {
        DiffPtr diffptr = *diff_iter;
        numevents += diffptr -> NumUserTags();
    }

    return numevents;
}
