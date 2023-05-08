//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : diffsortcriterion.cpp
//
// Description: 
//

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include "diffsortcriterion.h"
#include "dataprotectionexception.h"
#include "portablehelpers.h"

// --------------------------------------------------------------------------
// constructor
// --------------------------------------------------------------------------
DiffSortCriterion::DiffSortCriterion(char const * diffId, std::string const & volumeName)
: m_Id(diffId),
  m_SortValue(FROM_FILE_SORT_VALUE),
  m_IdVersion(0),
  m_ResyncCounter(0),
  m_DirtyBlockCounter(0),
  m_CxTime(0),
  m_PrevEndTime(0),
  m_PrevEndTimeSequenceNumber(0),
  m_PrevContinuationId(0),
  m_EndTime(0),
  m_EndTimeSequenceNumber(0),
  m_ContinuationId(0),
  m_EndContinuation(true),
  m_VolumeName(volumeName)
{
    std::string id(diffId);
    ParseIdBasedOnVersion(id);
}

// --------------------------------------------------------------------------
// constructor
// --------------------------------------------------------------------------
DiffSortCriterion::DiffSortCriterion(SORT_VALUES sortValue)
: m_SortValue(sortValue),
  m_IdVersion(0)
{  
    if (MIN_SORT_VALUE == sortValue) {
		m_ResyncCounter = 0;
		m_DirtyBlockCounter = 0;
        m_CxTime = 0;
        m_PrevEndTime = 0;
        m_PrevEndTimeSequenceNumber = 0;
        m_PrevContinuationId = 0;        
        m_EndTime = 0;
        m_EndTimeSequenceNumber = 0;
        m_ContinuationId = 0;        
    } else {
		m_ResyncCounter = ~0;
		m_DirtyBlockCounter = ~0;
        m_CxTime = ~0;
        m_PrevEndTime = ~0;
        m_PrevEndTimeSequenceNumber = ~0;
        m_PrevContinuationId = ~0;        
        m_EndTime = ~0;
        m_EndTimeSequenceNumber = ~0;
        m_ContinuationId = ~0;           
    }
    m_EndContinuation = true;
}

// --------------------------------------------------------------------------
// constructor
// --------------------------------------------------------------------------
DiffSortCriterion::DiffSortCriterion(DiffSortCriterion const & rhs)
: m_Id(rhs.m_Id),
  m_SortValue(rhs.m_SortValue),
  m_IdVersion(rhs.m_IdVersion),
  m_ResyncCounter(rhs.m_ResyncCounter),
  m_DirtyBlockCounter(rhs.m_DirtyBlockCounter),
  m_CxTime(rhs.m_CxTime),
  m_PrevEndTime(rhs.m_PrevEndTime),
  m_PrevEndTimeSequenceNumber(rhs.m_PrevEndTimeSequenceNumber),
  m_PrevContinuationId(rhs.m_PrevContinuationId),
  m_EndTime(rhs.m_EndTime),
  m_EndTimeSequenceNumber(rhs.m_EndTimeSequenceNumber),
  m_ContinuationId(rhs.m_ContinuationId),
  m_EndContinuation(rhs.m_EndContinuation),
  m_VolumeName(rhs.m_VolumeName)  
{  
}

bool DiffSortCriterion::Lessthan(DiffSortCriterion const & rhs) const
{
	if ( m_SortValue == MIN_SORT_VALUE || rhs.m_SortValue == MAX_SORT_VALUE )
	{
		return true;
	}
	else if ( m_SortValue == MAX_SORT_VALUE || rhs.m_SortValue == MIN_SORT_VALUE )
	{
		return false;
	}

    if (m_IdVersion == rhs.m_IdVersion)
	{
		switch(m_IdVersion)
		{
		case DIFFIDVERSION1:
			if (0 == m_CxTime || 0 == rhs.m_CxTime || m_CxTime == rhs.m_CxTime)
			{
				if (m_EndTime == rhs.m_EndTime)
				{
					if (m_EndTimeSequenceNumber == rhs.m_EndTimeSequenceNumber)
					{
						if(m_ContinuationId == rhs.m_ContinuationId)
						{
							return m_VolumeName < rhs.m_VolumeName;
						}
						else
						{
							return m_ContinuationId < rhs.m_ContinuationId;
						}
					}
					else
					{
						return (m_EndTimeSequenceNumber < rhs.m_EndTimeSequenceNumber);
					}
				}
				else
				{
					return (m_EndTime < rhs.m_EndTime);
				}
			}
			else
			{
				return m_CxTime < rhs.m_CxTime;
			}
			break;

		case DIFFIDVERSION2:
			if (m_ResyncCounter == rhs.m_ResyncCounter)
			{
				if (m_DirtyBlockCounter == rhs.m_DirtyBlockCounter)
				{
					if (m_EndTime == rhs.m_EndTime)
					{
						if (m_EndTimeSequenceNumber == rhs.m_EndTimeSequenceNumber)
						{
							if(m_ContinuationId == rhs.m_ContinuationId)
							{
								if(m_CxTime == rhs.m_CxTime)
								{
									return m_VolumeName < rhs.m_VolumeName;
								} else
								{
									return m_CxTime < rhs.m_CxTime;
								}
							}
							else
							{
								return m_ContinuationId < rhs.m_ContinuationId;
							}
						}
						else
						{
							return (m_EndTimeSequenceNumber < rhs.m_EndTimeSequenceNumber);
						}
					}
					else
					{
						return (m_EndTime < rhs.m_EndTime);
					}
				}
				else
				{
					return (m_DirtyBlockCounter < rhs.m_DirtyBlockCounter);
				}
			}
			else
			{
		        return (m_ResyncCounter < rhs.m_ResyncCounter);
			}
			break;

		case DIFFIDVERSION3:
			if (m_EndTime == rhs.m_EndTime)
			{
				if (m_EndTimeSequenceNumber == rhs.m_EndTimeSequenceNumber)
				{
					if(m_ContinuationId == rhs.m_ContinuationId)
					{
                        if(m_CxTime == rhs.m_CxTime)
                        {
						return m_VolumeName < rhs.m_VolumeName;
                        } else
                        {
                            return m_CxTime < rhs.m_CxTime;
                        }
					}
					else
					{
						return m_ContinuationId < rhs.m_ContinuationId;
					}
				}
				else
				{
					return (m_EndTimeSequenceNumber < rhs.m_EndTimeSequenceNumber);
				}
			}
			else
			{
				return (m_EndTime < rhs.m_EndTime);
			}
			break;
		}
    }
    return (m_IdVersion < rhs.m_IdVersion);
}

// --------------------------------------------------------------------------
// compares diff infos for equality using
// cx time, end time, end time sequence number, continuation, volume name
// return true if all equal to rhs else false
// --------------------------------------------------------------------------
bool DiffSortCriterion::Equal(DiffSortCriterion const & rhs) const 
{
	if(m_SortValue != FROM_FILE_SORT_VALUE || rhs.m_SortValue != FROM_FILE_SORT_VALUE)
	{
		return (m_SortValue == rhs.m_SortValue);
	}
	
	if(m_IdVersion == rhs.m_IdVersion)
	{
		switch(m_IdVersion)
		{
		case DIFFIDVERSION1:
			return (m_CxTime == rhs.m_CxTime && m_EndTime == rhs.m_EndTime && m_EndTimeSequenceNumber == rhs.m_EndTimeSequenceNumber
				&& m_ContinuationId == rhs.m_ContinuationId && m_VolumeName == rhs.m_VolumeName);            
			break;
		case DIFFIDVERSION2:
			return (m_ResyncCounter == rhs.m_ResyncCounter && m_DirtyBlockCounter == rhs.m_DirtyBlockCounter
				&& m_EndTime == rhs.m_EndTime && m_EndTimeSequenceNumber == rhs.m_EndTimeSequenceNumber
				&& m_ContinuationId == rhs.m_ContinuationId &&  m_VolumeName == rhs.m_VolumeName);
			break;
		case DIFFIDVERSION3:
			return (m_CxTime == rhs.m_CxTime
				&& m_EndTime == rhs.m_EndTime && m_EndTimeSequenceNumber == rhs.m_EndTimeSequenceNumber
				&& m_ContinuationId == rhs.m_ContinuationId && m_PrevContinuationId == rhs.m_PrevContinuationId
				&& m_PrevEndTime == rhs.m_PrevEndTime && m_PrevEndTimeSequenceNumber == rhs.m_PrevEndTimeSequenceNumber
				&& m_VolumeName == rhs.m_VolumeName);
			break;
		}
	}

	return false;
}

void DiffSortCriterion::ParseIdBasedOnVersion(std::string const& id)
{
    std::stringstream msg;
    
    std::string::size_type dotPos = id.rfind(".dat");
    std::string::size_type lastUnderScorePos = id.rfind("_", dotPos - 1);
	std::string::size_type lastButOneUnderScorePos = id.rfind("_", lastUnderScorePos - 1);
    if( std::string::npos == dotPos ||
		std::string::npos == lastUnderScorePos ||
		std::string::npos == lastButOneUnderScorePos )
	{
        msg << "FAILED DiffSortCriterion::ParseId: filename format(" << id << ") is invalid\n";
        throw DataProtectionException(msg.str());
    }

	do
	{
		//check if we have P in 6th or 7th position from ending of filename
	    std::string::size_type underScorePos = id.rfind("_");
		for(int i = 1; i < 6; i++)
		{
			underScorePos = id.rfind("_", underScorePos -1 );
			if(std::string::npos == underScorePos)
			{
				msg << "FAILED DiffSortCriterion::ParseId: filename format(" << id << ") is invalid\n";
				throw DataProtectionException(msg.str());
			}
		}

		if( 'P' == id[underScorePos + 1] )
		{
			m_IdVersion = DIFFIDVERSION3;
			break;
		}
		else
		{
			underScorePos = id.rfind("_", underScorePos -1 );
			if(std::string::npos == underScorePos)
			{
				msg << "FAILED DiffSortCriterion::ParseId: filename format(" << id << ") is invalid\n";
				throw DataProtectionException(msg.str());
			}

			if( 'P' == id[underScorePos + 1] )
			{
				m_IdVersion = DIFFIDVERSION3;
				break;
			}
		}

		// check for continuation maker at last or last but one positions
		if( 'R' == id[lastUnderScorePos + 1] )
		{
			m_IdVersion = DIFFIDVERSION2;
			break;
		}
		else if( 'R' == id[lastButOneUnderScorePos + 1] )
		{
			m_IdVersion = DIFFIDVERSION2;
			break;
		}

		// check for continuation maker at last or last but one positions
		if( 'W' == id[lastUnderScorePos + 1] ||
			'M' == id[lastUnderScorePos + 1] ||
			'F' == id[lastUnderScorePos + 1] )
		{
			m_IdVersion = DIFFIDVERSION1;
			break;
		}
		else if( 'W' == id[lastButOneUnderScorePos + 1] ||
				'M' == id[lastButOneUnderScorePos + 1] ||
				'F' == id[lastButOneUnderScorePos + 1] )
		{
			m_IdVersion = DIFFIDVERSION1;
			break;
		}

	}while(false);

	switch(m_IdVersion)
	{
	case DIFFIDVERSION1:
		ParseIdV1(id);
		break;

	case DIFFIDVERSION2:
		ParseIdV2(id);
		break;

	case DIFFIDVERSION3:
		ParseIdV3(id);
		break;

	default:
        msg << "FAILED DiffSortCriterion: Unknown Id version for id(" << id << ")\n\n";
        throw DataProtectionException(msg.str());
		break;
	}
}

// --------------------------------------------------------------------------
// tokenizes the diff info id into the parts needed for sorting
// expects the id in one of the following formats
// completed_[ediffcompleted_]diff_[tso_]P<time>__<pn>_<pc>_E<time>_<n>_<MC|ME|WC|WE><continuation number>[_<cx timestamp>].dat[.gz]
//  where
//    tso: indicates timestamps only
//    P<time>: end timestamp of the previous diff
//    <pn>: previous diff's end sequence number
//    <pc>: previous diff's continuation number
//    E<startTime>: timestamp from the dirty block for the end timestamp
//    <n>: the sequence number from the dirty block for end timestamp
//    <MC|ME|WC|WE>: is the continuation indicator
//      MC: meta data mode continuation more data coming with the same time stamps
//      ME: meta data mode continuation end
//      WC: write order mode continuation more data coming with the same time stamps
//      WE: write order mode continuation end
//    <continuation number> : continuation number
//    <cx timestamp>: timestamp generated by the cx when the diff file arrived
//    .gz: indicates the file is compressed
// e.g. 
//  completed_ediffcompleted_diff_P127794935171250000_15_1_E127794935212187500_20_ME1_1127961947.dat.gz
//  completed_diff_P127794935171250000_1000_5_E127794935212187500_2000_ME1_1c5c49f93984040.dat
//
// --------------------------------------------------------------------------
void DiffSortCriterion::ParseIdV3(std::string const & id)
{
    std::stringstream msg;    
    
    // skip past .dat[.gz]
    std::string::size_type dotPos = id.rfind(".dat");
    if (std::string::npos == dotPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing .dat id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    // find the last '_' 
    std::string::size_type continuationTagEndPos = id.rfind("_", dotPos - 1);

    if (std::string::npos == continuationTagEndPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing continuation or cx time id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    std::string::size_type continuationTagPos;
    // check for continuation maker as cx timestamp are not there for pre named files that the cx has not yet processed
    if (std::string::npos == continuationTagEndPos 
        || !('W' == id[continuationTagEndPos + 1] || 'M' == id[continuationTagEndPos + 1] || 'F' == id[continuationTagEndPos + 1])
    ) {
        // not contiuation maker, should be the cx time stamp
        try {
            m_CxTime = boost::lexical_cast<unsigned long long>(id.substr(continuationTagEndPos + 1, dotPos - (continuationTagEndPos + 1)).c_str());
        } catch (boost::bad_lexical_cast& e) {
            msg << "FAILED DiffSortCriterion::ParseId: invalid format missing cx time (" << id << ") -"
                << e.what() << '\n';
            throw DataProtectionException(msg.str());
        }

        continuationTagPos = id.rfind("_", continuationTagEndPos - 1);
        if (std::string::npos == continuationTagPos 
            || !('W' == id[continuationTagPos + 1] || 'M' == id[continuationTagPos + 1] || 'F' == id[continuationTagPos + 1])
        ) {
			msg << "FAILED DiffSortCriterion::ParseId: diff version may not be 3: id(" << id << ")\n";
			throw DataProtectionException(msg.str());
        }
    } else {
        // adjust to reflect that there was no cx timestamp
        continuationTagPos = continuationTagEndPos;
        continuationTagEndPos = dotPos;
    }

    // write order mode W|M|F
    m_Mode = id[continuationTagPos + 1];

    // is it a continuation end?
    m_EndContinuation = ('E' == id[continuationTagPos + 2]);

    // get the continuation id
    try {
        m_ContinuationId = boost::lexical_cast<unsigned int>(id.substr(continuationTagPos + 3, continuationTagEndPos - (continuationTagPos + 3)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing continuation id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get end time sequence number
    std::string::size_type endTimeSequnceNumberPos = id.rfind("_", continuationTagPos - 1);
    if (std::string::npos == endTimeSequnceNumberPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time sequence number(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_EndTimeSequenceNumber = boost::lexical_cast<unsigned long long>(id.substr(endTimeSequnceNumberPos + 1, continuationTagPos - (endTimeSequnceNumberPos + 1)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get end time
    std::string::size_type endTimePos = id.rfind("_", endTimeSequnceNumberPos - 1);
    if (std::string::npos == endTimePos || 'E' != id[endTimePos + 1]) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_EndTime = boost::lexical_cast<unsigned long long>(id.substr(endTimePos + 2, endTimeSequnceNumberPos - (endTimePos + 2)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get previous diff's continuation id
    std::string::size_type prevContinuationIdPos = id.rfind("_", endTimePos - 1);
    if (std::string::npos == prevContinuationIdPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing previous continuation id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_PrevContinuationId = boost::lexical_cast<unsigned int>(id.substr(prevContinuationIdPos + 1, endTimePos - (prevContinuationIdPos + 1)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing previous continuation id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get previous end time sequence number
    std::string::size_type prevEndTimeSequnceNumberPos = id.rfind("_", prevContinuationIdPos - 1);
    if (std::string::npos == prevEndTimeSequnceNumberPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing previous end time sequence number(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_PrevEndTimeSequenceNumber = boost::lexical_cast<unsigned long long>(id.substr(prevEndTimeSequnceNumberPos + 1, prevContinuationIdPos - (prevEndTimeSequnceNumberPos + 1)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing previous end time sequence number(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get previous end time
    std::string::size_type prevEndTimePos = id.rfind("_", prevEndTimeSequnceNumberPos - 1);
    if (std::string::npos == prevEndTimePos || 'P' != id[prevEndTimePos + 1]) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing previous end time id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_PrevEndTime = boost::lexical_cast<unsigned long long>(id.substr(prevEndTimePos + 2, prevEndTimeSequnceNumberPos - (prevEndTimePos + 2)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing previous end time id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }
}

// --------------------------------------------------------------------------
// tokenizes the diff info id into the parts needed for sorting
// expects the id in one of the following formats
// completed_[ediffcompleted_]diff_[tso_]S<time>__<n>_E<time>_<n>_<MC|ME|WC|WE><continuation number>_O<dirtyBlockCounter>_R<ResyncCounter>[_<cx timestamp>].dat[.gz]
//  where
//    tso: indicates timestamps only
//    S<startTime>: timestamp from the dirty block for the start timestamp
//    <n>: the sequence number from the dirty block for start timestamp
//    E<startTime>: timestamp from the dirty block for the end timestamp
//    <n>: the sequence number from the dirty block for end timestamp
//    <MC|ME|WC|WE>: is the continuation indicator
//      MC: meta data mode continuation more data coming with the same time stamps
//      ME: meta data mode continuation end
//      WC: write order mode continuation more data coming with the same time stamps
//      WE: write order mode continuation end
//    <continuation number> : continuation number
//    O<dirtyBlockCounter>: current dirty block number
//    R<ResyncCounter>: Resync counter of the source volume
//    <cx timestamp>: timestamp generated by the cx when the diff file arrived
//    .gz: indicates the file is compressed
// e.g. 
//  completed_ediffcompleted_diff_S127794935171250000_15_E127794935212187500_20_ME1_O1033_R1_1127961947.dat.gz
//  completed_diff_S127794935171250000_1000_E127794935212187500_2000_ME1_O2048_R5_1c5c49f93984040.dat
//
// --------------------------------------------------------------------------
void DiffSortCriterion::ParseIdV2(std::string const & id)
{
    std::stringstream msg;    
    
    // skip past .dat[.gz]
    std::string::size_type dotPos = id.rfind(".dat");
    if (std::string::npos == dotPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing .dat id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    // find the last '_' 
    std::string::size_type resyncCounterTagEndPos = id.rfind("_", dotPos - 1);

    if (std::string::npos == resyncCounterTagEndPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing resyncCounter or cx time id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    std::string::size_type resyncCounterTagPos;
    // check for resyncCounter maker as cx timestamp are not there for pre named files that the cx has not yet processed
    if (std::string::npos == resyncCounterTagEndPos || 'R' != id[resyncCounterTagEndPos + 1])
    {
        resyncCounterTagPos = id.rfind("_", resyncCounterTagEndPos - 1);
        if (std::string::npos == resyncCounterTagPos || 'R' != id[resyncCounterTagPos + 1])
        {
			msg << "FAILED DiffSortCriterion::ParseId: diff version may not be 2: id(" << id << ")\n";
			throw DataProtectionException(msg.str());
        }

        //should be the cx time stamp
        try {
            m_CxTime = boost::lexical_cast<unsigned long long>(id.substr(resyncCounterTagEndPos + 1, dotPos - (resyncCounterTagEndPos + 1)).c_str());
        } catch (boost::bad_lexical_cast& e) {
            msg << "FAILED DiffSortCriterion::ParseId: invalid format missing cx time (" << id << ") -"
                << e.what() << '\n';
            throw DataProtectionException(msg.str());
        }
    } else {
        // adjust to reflect that there was no cx timestamp
        resyncCounterTagPos = resyncCounterTagEndPos;
        resyncCounterTagEndPos = dotPos;
    }

    // get the resyncCounter
    try {
        m_ResyncCounter = boost::lexical_cast<unsigned long long>(id.substr(resyncCounterTagPos + 2, resyncCounterTagEndPos - (resyncCounterTagPos + 2)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing resyncCounter id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get dirtyBlockCounter
    std::string::size_type dirtyBlockCounterPos = id.rfind("_", resyncCounterTagPos - 1);
    if (std::string::npos == dirtyBlockCounterPos || 'O' != id[dirtyBlockCounterPos + 1]) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing dirtyBlockCounter id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_DirtyBlockCounter = boost::lexical_cast<unsigned long long>(id.substr(dirtyBlockCounterPos + 2, resyncCounterTagPos - (dirtyBlockCounterPos + 2)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get countinuation id & find if continuation end has reached
    std::string::size_type continuationTagPos = id.rfind("_", dirtyBlockCounterPos - 1);
    if (std::string::npos == continuationTagPos 
            || !('W' == id[continuationTagPos + 1] || 'M' == id[continuationTagPos + 1] || 'F' == id[continuationTagPos + 1])
	) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing continuation id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    // write order mode W|M|F
    m_Mode = id[continuationTagPos + 1];

    // is it a continuation end?
    m_EndContinuation = ('E' == id[continuationTagPos + 2]);

    // get the continuation id
    try {
        m_ContinuationId = boost::lexical_cast<unsigned int>(id.substr(continuationTagPos + 3, dirtyBlockCounterPos - (continuationTagPos + 3)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing continuation id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get end time sequence number
    std::string::size_type endTimeSequnceNumberPos = id.rfind("_", continuationTagPos - 1);
    if (std::string::npos == endTimeSequnceNumberPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time sequence number(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_EndTimeSequenceNumber = boost::lexical_cast<unsigned long long>(id.substr(endTimeSequnceNumberPos + 1, continuationTagPos - (endTimeSequnceNumberPos + 1)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get end time
    std::string::size_type endTimePos = id.rfind("_", endTimeSequnceNumberPos - 1);
    if (std::string::npos == endTimePos || 'E' != id[endTimePos + 1]) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_EndTime = boost::lexical_cast<unsigned long long>(id.substr(endTimePos + 2, endTimeSequnceNumberPos - (endTimePos + 2)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }
}

// --------------------------------------------------------------------------
// tokenizes the diff info id into the parts needed for sorting
// expects the id in one of the following formats
// completed_[ediffcompleted_]diff_[tso_]S<time>__<n>_E<time>_<n>_<MC|ME|WC|WE><continuation number>_[<cx timestamp>].dat[.gz]
//  where
//    tso: indicates timestamps only
//    S<startTime>: timestamp from the dirty block for the start timestamp
//    <n>: the sequence number from the dirty block for start timestamp
//    E<startTime>: timestamp from the dirty block for the end timestamp
//    <n>: the sequence number from the dirty block for end timestamp
//    <MC|ME|WC|WE>: is the continuation indicator
//      MC: meta data mode continuation more data coming with the same time stamps
//      ME: meta data mode continuation end
//      WC: write order mode continuation more data coming with the same time stamps
//      WE: write order mode continuation end
//    <continuation number> : continuation number
//    <cx timestamp>: timestamp generated by the cx when the diff file arrived
//    .gz: indicates the file is compressed
// e.g. 
//  completed_ediffcompleted_diff_S127794935171250000_10_E127794935212187500_15_ME1_1127961947.dat.gz
//  completed_diff_S127794935171250000_1800_E127794935212187500_2000_ME1_1c5c49f93984040.dat
//
// --------------------------------------------------------------------------
void DiffSortCriterion::ParseIdV1(std::string const & id)
{
    std::stringstream msg;    
    
    // skip past .dat[.gz]
    std::string::size_type dotPos = id.rfind(".dat");
    if (std::string::npos == dotPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing .dat id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    // find the last '_' 
    std::string::size_type continuationTagEndPos = id.rfind("_", dotPos - 1);

    if (std::string::npos == continuationTagEndPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing continuation or cx time id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    std::string::size_type continuationTagPos;
    // check for continuation maker as cx timestamp are not there for pre named files that the cx has not yet processed
    if (std::string::npos == continuationTagEndPos 
        || !('W' == id[continuationTagEndPos + 1] || 'M' == id[continuationTagEndPos + 1] || 'F' == id[continuationTagEndPos + 1])
    ) {
        // not contiuation maker, should be the cx time stamp
        try {
            m_CxTime = boost::lexical_cast<unsigned long long>(id.substr(continuationTagEndPos + 1, dotPos - (continuationTagEndPos + 1)).c_str());
        } catch (boost::bad_lexical_cast& e) {
            msg << "FAILED DiffSortCriterion::ParseId: invalid format missing cx time (" << id << ") -"
                << e.what() << '\n';
            throw DataProtectionException(msg.str());
        }

        continuationTagPos = id.rfind("_", continuationTagEndPos - 1);
        if (std::string::npos == continuationTagPos 
            || !('W' == id[continuationTagPos + 1] || 'M' == id[continuationTagPos + 1] || 'F' == id[continuationTagPos + 1])
        ) {
			msg << "FAILED DiffSortCriterion::ParseId: diff version may not be 1: id(" << id << ")\n";
			throw DataProtectionException(msg.str());
        }
    } else {
        // adjust to reflect that there was no cx timestamp
        continuationTagPos = continuationTagEndPos;
        continuationTagEndPos = dotPos;
    }

    // write order mode W|M|F
    m_Mode = id[continuationTagPos + 1];

    // is it a continuation end?
    m_EndContinuation = ('E' == id[continuationTagPos + 2]);

    // get the continuation id
    try {
        m_ContinuationId = boost::lexical_cast<unsigned int>(id.substr(continuationTagPos + 3, continuationTagEndPos - (continuationTagPos + 3)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing continuation id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get end time sequence number
    std::string::size_type endTimeSequnceNumberPos = id.rfind("_", continuationTagPos - 1);
    if (std::string::npos == endTimeSequnceNumberPos) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time sequence number(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_EndTimeSequenceNumber = boost::lexical_cast<unsigned long long>(id.substr(endTimeSequnceNumberPos + 1, continuationTagPos - (endTimeSequnceNumberPos + 1)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }

    // get end time
    std::string::size_type endTimePos = id.rfind("_", endTimeSequnceNumberPos - 1);
    if (std::string::npos == endTimePos || 'E' != id[endTimePos + 1]) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ")\n";
        throw DataProtectionException(msg.str());
    }

    try {
        m_EndTime = boost::lexical_cast<unsigned long long>(id.substr(endTimePos + 2, endTimeSequnceNumberPos - (endTimePos + 2)).c_str());
    } catch (boost::bad_lexical_cast& e) {
        msg << "FAILED DiffSortCriterion::ParseId: invalid format missing end time id(" << id << ") -"
            << e.what() << '\n';
        throw DataProtectionException(msg.str());
    }
}
