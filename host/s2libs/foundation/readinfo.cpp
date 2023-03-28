#include <string>
#include <sstream>
#include "readinfo.h"
#include "logger.h"
#include "portablehelpers.h"


ReadInfo::ReadInfo()
: m_AllocatedLen(0)
{
    Reset();
}


bool ReadInfo::Init(const SV_UINT &allocatelen, const SV_UINT &alignment)
{
    bool binitialized = AllocateReadBuffer(allocatelen, alignment); 
    if (binitialized)
    {
        m_AllocatedLen = allocatelen;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not allocate read buffer length: %lu\n", allocatelen);
    }

    return binitialized;
}


ReadInfo::~ReadInfo()
{
    m_AllocatedLen = 0;
}


void ReadInfo::Reset(void)
{
    m_Length = 0;
    m_Offset = 0;
    m_ErrorMessage.clear();
}


char * ReadInfo::GetData(void)
{
    return m_Data.Get();
}


SV_UINT ReadInfo::GetLength(void)
{
    return m_Length;
}


SV_LONGLONG ReadInfo::GetOffset(void)
{
    return m_Offset;
}


void ReadInfo::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, " ===== read info: start =====\n");
    DebugPrintf(SV_LOG_DEBUG, "data pointer = %p\n", m_Data.Get());
    DebugPrintf(SV_LOG_DEBUG, "allocated length = %u\n", m_AllocatedLen);
    DebugPrintf(SV_LOG_DEBUG, "read length = %u\n", m_Length);
    DebugPrintf(SV_LOG_DEBUG, "read offset = " LLSPEC "\n", m_Offset);
    DebugPrintf(SV_LOG_DEBUG, " ===== read info: end =====\n");
}


/* TODO: verify if asked length greater than allocated length ? */
bool ReadInfo::Read(ReadInputs_t &ri)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with offset = " LLSPEC
        ", length = %u, should seek = %s, volume = %s\n", FUNCTION_NAME,
		ri.m_Offset, ri.m_Length, ri.m_ShouldSeek ? "yes" : "no", ri.m_pVolume->GetName().c_str());
    bool bretval;
    SV_UINT count;
    unsigned int nretry = 0;

    do
    {
		/* seek if asked for or
		* if not asked for, then
		* seek if we are retrying */
		if (ri.m_ShouldSeek || (nretry>0))
		{
            ri.m_pVolume->Seek(ri.m_Offset, BasicIo::BioBeg);
		}
		
        count = ri.m_pVolume->FullRead(m_Data.Get(), ri.m_Length);
   
		bretval = (count==ri.m_Length);
        if (bretval)
        {
            break;
        }
        else 
        {
            std::stringstream msg;
            if (ri.m_pVolume->Eof())
            {
                // ASSUMPTION: don't use an assert here as we want this test in the field
                // we should never see this as the targets should be aware of the sources size
                // and never send offsets outside their range. sources should always be
                // less then or equal to the size of their targets so sources would never send
                // out of range offsets to a target
                ri.m_pVolume->Seek(0, BasicIo::BioEnd);
				msg << "Volume: " << ri.m_pVolume->GetName()
					<< ", ReadInfo::ReadFromVolume failed out of range: wanted offset (0x"
                    << std::hex << ri.m_Offset << ") max offset (0x" << std::hex
                    << ri.m_pVolume->Tell() << ")\n";
				m_ErrorMessage = msg.str();
				return bretval;
            }
			msg << "CRITICAL ReadInfo::ReadFromVolume read failed: "
                << "Volume: " << ri.m_pVolume->GetName()
                << ", offset:" << ri.m_Offset
                << ", bytes read:" << count << " "
                << ", bytes requested:" << ri.m_Length << " "
                << ", Error:" << ri.m_pVolume->LastError() << '.';
            if (nretry == ri.m_ReadRetryInfo.m_NumberOfRetries)
            {  
                if (!ri.m_ReadRetryInfo.m_PutZerosForFailure)
                {
                    msg << "\n";
					m_ErrorMessage = msg.str();
					return bretval;
                }
                msg << " NOTE: Considering all zeros as contents for above, since same is asked for.\n";
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                memset(m_Data.Get(), 0, ri.m_Length);
                bretval = true;
            }
            else
            {
                msg << " Retrying read after " << ri.m_ReadRetryInfo.m_RetryInterval << " seconds.\n";
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            }
        }
        nretry++;
    } while ((nretry <= ri.m_ReadRetryInfo.m_NumberOfRetries) && !ri.m_QuitFunction(ri.m_ReadRetryInfo.m_RetryInterval));

    if (bretval)
    {
        m_Offset = ri.m_Offset;
        m_Length = ri.m_Length;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bretval;
}


std::string ReadInfo::GetErrorMessage(void)
{
    return m_ErrorMessage;
}


ReadInfo::ReadInputs::ReadInputs(cdp_volume_t *pvolume, const SV_LONGLONG &offset,
                                 const unsigned int &length, const bool &shouldseek,
                                 const ReadRetryInfo_t &rri,
                                 QuitFunction_t &quitfunction) 
 : m_pVolume(pvolume),
   m_Offset(offset),
   m_Length(length),
   m_ShouldSeek(shouldseek),
   m_ReadRetryInfo(rri),
   m_QuitFunction(quitfunction)
{
}


ReadInfo::ReadRetryInfo::ReadRetryInfo(const bool &putzerosforfailure, 
                                       const unsigned int &numberofretries,
                                       const unsigned int &retryinterval)
 : m_PutZerosForFailure(putzerosforfailure),
   m_NumberOfRetries(numberofretries),
   m_RetryInterval(retryinterval)
{
}


ReadInfo::ReadRetryInfo::ReadRetryInfo(const ReadRetryInfo &rhs)
 : m_PutZerosForFailure(rhs.m_PutZerosForFailure),
   m_NumberOfRetries(rhs.m_NumberOfRetries),
   m_RetryInterval(rhs.m_RetryInterval)
{
}


ReadInfo::ReadRetryInfo::ReadRetryInfo() 
 : m_PutZerosForFailure(false),
   m_NumberOfRetries(0),
   m_RetryInterval(0)
{
}


bool ReadInfo::AllocateReadBuffer(const SV_UINT &allocatelen, const SV_UINT &alignment)
{
    m_Data.Reset(allocatelen, alignment);
    return m_Data;
}
