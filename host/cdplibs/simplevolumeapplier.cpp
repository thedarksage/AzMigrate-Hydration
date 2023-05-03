#include <string>
#include <sstream>

#include <ace/ACE.h>
#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_unistd.h>

#include "simplevolumeapplier.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "logger.h"
#include "cdpvolumeutil.h"

#define VERIFY_WRITE_AT_ZERO_OFFSET 1

SimpleVolumeApplier::SimpleVolumeApplier(VolumeApplierFormationInputs_t &inputs)
: m_pVolume(inputs.m_pVolume),
  m_Profile(inputs.m_Profile),
  m_TimeForApply(0)
{
	m_Write[0] = &SimpleVolumeApplier::NoProfileWrite;
	m_Write[1] = &SimpleVolumeApplier::ProfileWrite;
}


SimpleVolumeApplier::~SimpleVolumeApplier()
{
}


bool SimpleVolumeApplier::Apply(const SV_LONGLONG &offset, const SV_UINT &length, char *data)
{
    if (0 == m_pVolume)
    {
        m_ErrorMessage = "For simple volume applier, volume to apply unspecified.";
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with offset " LLSPEC " length %u, data %p\n",FUNCTION_NAME, offset, length, data);
    bool bapplied = true;
    
#ifdef SV_WINDOWS
	/* NOTE: logical offset assumed to have start 
	* of volume */
	if(0 == offset)
	{
		DebugPrintf(SV_LOG_DEBUG,"offset to apply in simple volume applier is zero for volume %s\n", m_pVolume->GetName().c_str());
		HideBeforeApplyingSyncData(data, length);
	}
#endif

	Write_t wr = m_Write[m_Profile];
	if (!(this->*wr)(offset, length, data))
	{
		bapplied = false;
	}
#ifdef VERIFY_WRITE_AT_ZERO_OFFSET
	else
	{
		cdp_volume_util u;
		if((0 == offset) && !u.DoesDataMatch(*m_pVolume, offset, length, data))
		{
			while (true)
			{
				DebugPrintf(SV_LOG_ERROR, "For volume %s, successfully written from offset " LLSPEC 
							", length %u, data %p, but read back data does not match\n",
							m_pVolume->GetName().c_str(), offset, length, data);
				ACE_OS::sleep(300);
			}
		}
	}
#endif
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bapplied;
}


bool SimpleVolumeApplier::Flush(void)
{
    return (0 != m_pVolume);
}


ACE_Time_Value SimpleVolumeApplier::GetTimeForApply(void)
{
	return m_TimeForApply;
}


ACE_Time_Value SimpleVolumeApplier::GetTimeForOverhead(void)
{
	return ACE_Time_Value(0);
}


std::string SimpleVolumeApplier::GetErrorMessage(void)
{
    return m_ErrorMessage;
}


bool SimpleVolumeApplier::NoProfileWrite(const SV_LONGLONG &offset, const SV_UINT &length, char *data)
{
	return Write(offset, length, data);
}


bool SimpleVolumeApplier::ProfileWrite(const SV_LONGLONG &offset, const SV_UINT &length, char *data)
{
	ACE_Time_Value TimeBeforeRead, TimeAfterRead;

    TimeBeforeRead = ACE_OS::gettimeofday();
    bool rval = Write(offset, length, data);
    TimeAfterRead = ACE_OS::gettimeofday();

    m_TimeForApply += (TimeAfterRead-TimeBeforeRead);

    return rval;
}


bool SimpleVolumeApplier::Write(const SV_LONGLONG &offset, const SV_UINT &length, char *data)
{
	bool bapplied = true;

	SV_UINT cbWritten;
	m_pVolume->Seek(offset, BasicIo::BioBeg);
	if( ((cbWritten = m_pVolume->Write(data, length)) != length)
		|| (!m_pVolume->Good()))
	{
		std::stringstream msg;
		msg << "Error during write operation on volume " << m_pVolume->GetName() << '\n'
			<< " Offset : " << m_pVolume->Tell()<< '\n'
			<< "Expected Write: " << length << '\n'
			<< "Actual Write Bytes: " << cbWritten << '\n'
			<< "Error Code: " << m_pVolume->LastError() << '\n'
			<< "Error Message: " << Error::Msg(m_pVolume->LastError()) << '\n';
		m_ErrorMessage = msg.str();
		bapplied = false;
	}

#ifndef SV_WINDOWS
	if(bapplied && ((m_pVolume->OpenMode()&BasicIo::BioDirect) == 0))
	{
		if(ACE_INVALID_HANDLE != m_pVolume->GetHandle())
		{
			if(ACE_OS::fsync(m_pVolume->GetHandle()) == -1)
			{
				m_ErrorMessage = "Flush buffers failed for ";
				m_ErrorMessage += m_pVolume->GetName();
				m_ErrorMessage += " with error ";
				m_ErrorMessage += ACE_OS::last_error();
				bapplied = false;
			}
		}
	}
#endif

	return bapplied;
}