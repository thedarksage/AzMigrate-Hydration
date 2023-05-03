// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : svmarker.cpp
//
// Description: 
//

#include "svmarker.h"
#include "cdpglobals.h"
#include "StreamEngine.h"
#include "VacpUtil.h"
#include <sstream>

#include <boost/lexical_cast.hpp>

using namespace std;

SV_MARKER::SV_MARKER(boost::shared_array<char> buffer, SV_ULONG length)
: m_buffer(buffer), m_length(length), m_lifetime(INM_BOOKMARK_LIFETIME_NOTSPECIFIED)
{
	m_tagtype = 0;
	m_appname = "";

	m_taglength = 0;
	m_bParsed = false;
	m_rv      = false;
    m_bRevocationTag = false;
}

SV_MARKER::~SV_MARKER()
{
}

bool SV_MARKER::Parse( SV_UINT& dataFormatFlags )
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
    SVERROR sv = SVS_OK;

	//__asm int 3;
		
	do 
	{
		if(m_bParsed)
		{
			break;
		}

		m_bParsed = true;

		if (!m_buffer)
		{
			stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					   << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME
					   << "Error: user tag buffer is null.\n";

		    DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
			break;
		}

		boost::shared_ptr<StreamEngine> Stream(new (nothrow) StreamEngine(eStreamRoleParser));
		if(!Stream)
		{
			stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					   << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					   << "Error: insufficient memory to allocate StreamEngine Object.\n";

		    DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
			break;
		}

		rv = Stream->RegisterDataSourceBuffer(m_buffer.get(), m_length);
		if ( !rv )
		{
			stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					   << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					   << "Error: StreamEngine::RegisterDataSourceBuffer routine "
					   << "returned failed status.\n";

		    DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
			break;
		}

        rv = Stream->ValidateStreamRecord(m_buffer.get(), m_length);
        if ( !rv )
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Error: StreamEngine::ValidateStreamRecord routine "
				<< "returned failed status.\n" << "Buffer: " << m_buffer.get() << "\nLength: " << m_length << "\n";

            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            break;
        }
		
        unsigned short record_type = 0;
        unsigned long record_length = 0;

        Stream -> SetStreamState(eStreamWaitingForHeader);
        while(sv == SVS_OK)
        {
            if(Stream -> GetStreamState() == eStreamWaitingForHeader)
            {
                sv = Stream->GetRecordHeader(&record_type, &record_length, dataFormatFlags);
                if(sv.succeeded())
                {
                    std::string appname;
                    if(VacpUtil::TagTypeToAppName(record_type, appname))
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Record Type : %s\n", appname.c_str());
                    }

                    if(STREAM_REC_TYPE_REVOCATION_TAG == record_type)
                    {
                        m_bRevocationTag = true;
                        Stream -> SetStreamState(eStreamWaitingForHeader);
                    } else
                    {
                        Stream -> SetStreamState(eStreamWaitingForData);
                    }
                }
            } else
            {
                char * record = new(nothrow) char[record_length];
                if(!record)
                {
                    stringstream l_stdfatal;
                    l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                        << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                        << "Error: insufficient memory to allocate" << record_length << " bytes.\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
                    sv = SVE_FAIL;
                    break;
                }

				sv = Stream->GetRecordData(record, record_length, record_length);
                Stream -> SetStreamState(eStreamWaitingForHeader);
                if(sv.succeeded())
                {
                    if(STREAM_REC_TYPE_TAGGUID_TAG == record_type)
                    {
                        m_guid.reset(record);
                        DebugPrintf(SV_LOG_DEBUG, "guid: %s\n", record);
                    } else
                        if(STREAM_REC_TYPE_LIFETIME == record_type)
                        {
                            m_lifetime = boost::lexical_cast<SV_ULONGLONG>(record);
                            DebugPrintf(SV_LOG_DEBUG, "lifetime:" ULLSPEC "\n", m_lifetime);
                        } else
                        {
                            m_tagtype = record_type;
                            m_tag.reset(record);
                        }
                }
            }
        }

        if(sv != SVS_FALSE)
        {
            DebugPrintf(SV_LOG_ERROR, "error during parsing bookmark stream.\n");
            rv = false;
            break;
        }

		rv = VacpUtil::TagTypeToAppName(m_tagtype, m_appname);
        if(rv)
            DebugPrintf(SV_LOG_DEBUG, "Application : %s\n", m_appname.c_str());
        else
            DebugPrintf(SV_LOG_DEBUG, "Invalid application type: %u\n", m_tagtype);
		
	} while (false);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


