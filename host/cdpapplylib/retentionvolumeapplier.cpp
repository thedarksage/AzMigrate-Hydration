#include <string>
#include <sstream>

#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_unistd.h>
#include <ace/ACE.h>

#include "svdparse.h"
#include "retentionvolumeapplier.h"
#include "portablehelpers.h"
#include "logger.h"
#include "inmsafeint.h"
#include "inmageex.h"

static int const headerSize = sizeof(SVD_PREFIX)+sizeof(SVD_HEADER1);
static int const dbHeaderSize = sizeof(SVD_PREFIX)+sizeof(SVD_DIRTY_BLOCK);

RetentionVolumeApplier::RetentionVolumeApplier(VolumeApplierFormationInputs_t &inputs, 
                                               FileNameProvider_t &filenameprovider,
                                               CDPApplierInstantiator_t &cdpapplierinstantiator, 
                                               CDPApplierDestroyer_t &cdpapplierdestroyer,
                                               AppliedBytesUser_t &appliedbytesuser,
                                               ShouldApply_t &shouldapply)
   : m_TargetVolumeName(inputs.m_Name),
   m_lastChangeOffset(0),
   m_AccumulatedChangesForRetention(0),
   m_FileNameProvider(filenameprovider),
   m_CDPApplierInstantiator(cdpapplierinstantiator),
   m_CDPApplierDestroyer(cdpapplierdestroyer),
   m_AppliedBytesUser(appliedbytesuser),
   m_ShouldApply(shouldapply),
   m_pApplier(0),
   m_Profile(inputs.m_Profile),
   m_TimeForApply(0),
   m_TimeForOverhead(0)
{
	m_Applychanges[0] = &RetentionVolumeApplier::NoProfileApplychanges;
	m_Applychanges[1] = &RetentionVolumeApplier::ProfileApplychanges;

	m_Cacher[0] = &RetentionVolumeApplier::NoProfileCacher;
	m_Cacher[1] = &RetentionVolumeApplier::ProfileCacher;

	m_AppliedBytesUserFunction[0] = &RetentionVolumeApplier::NoProfileAppliedByteUserFunction;
	m_AppliedBytesUserFunction[1] = &RetentionVolumeApplier::ProfileAppliedByteUserFunction;
}


RetentionVolumeApplier::~RetentionVolumeApplier()
{
    if (m_pApplier)
    {
        m_CDPApplierDestroyer(m_pApplier);
        m_pApplier = 0;
    }
}


bool RetentionVolumeApplier::IsValid(void)
{
    bool isvalid = true;

    if (!m_FileNameProvider)
    {
        isvalid = false;
        m_ErrorMessage = " No file name provider specified.";
    }

    if (!m_CDPApplierInstantiator)
    {
        isvalid = false;
        m_ErrorMessage = " No cdp applier instantiator specified.";
    }

    if (!m_CDPApplierDestroyer)
    {
        isvalid = false;
        m_ErrorMessage = " No applier destroyer specified.";
    }

    if (!m_AppliedBytesUser)
    {
        isvalid = false;
        m_ErrorMessage = " No applied bytes user specified.";
    }

    if (!m_ShouldApply)
    {
        isvalid = false;
        m_ErrorMessage = " No apply consenter specified.";
    }

    return isvalid;
}


bool RetentionVolumeApplier::Init(const SV_UINT &nchanges, const SV_UINT &bufsize)
{
    if (!IsValid())
    {
        return false;
    }

    SV_UINT bufanddbheadersize;
    INM_SAFE_ARITHMETIC(bufanddbheadersize = InmSafeInt<SV_UINT>::Type(bufsize)+dbHeaderSize, INMAGE_EX(bufsize)(dbHeaderSize))
    SV_UINT syncdatalength;
    INM_SAFE_ARITHMETIC(syncdatalength = (InmSafeInt<SV_UINT>::Type(nchanges)*bufanddbheadersize) + headerSize, INMAGE_EX(nchanges)(bufanddbheadersize)(headerSize))
    bool initialized = m_RetentionApplyBuffer.Allocate(syncdatalength);
	  
    if (initialized)
    {
        char *p = m_RetentionApplyBuffer.GetDataForUse();
        memset(p, 0, headerSize);
        reinterpret_cast<SVD_PREFIX*>(p)->tag = SVD_TAG_HEADER1;
        reinterpret_cast<SVD_PREFIX*>(p)->count = 1;
        m_RetentionApplyBuffer.AddToUsedLength(headerSize);

        m_pApplier = m_CDPApplierInstantiator(m_TargetVolumeName);
        if (0 == m_pApplier)
        {
            m_ErrorMessage = "Failed to instantiate applier.";
            initialized = false;
        }
    }
    else
    {
        std::stringstream msg;
        msg << "Failed to allocate sync data buffer of length " << syncdatalength
            << " for target volume " << m_TargetVolumeName;
        m_ErrorMessage = msg.str();
    }

    return initialized;
}


bool RetentionVolumeApplier::Apply(const SV_LONGLONG &offset, const SV_UINT &length, char *data)
{
    bool bapplied = true;
    DebugPrintf(SV_LOG_DEBUG, "Checking for target %s, from offset " LLSPEC ", length %u, "
                              " if this can be accumulated in retention buffer.\n",
                              m_TargetVolumeName.c_str(), offset, length);
    if (!m_RetentionApplyBuffer.IsSpaceAvailable(dbHeaderSize+length))
    {
        DebugPrintf(SV_LOG_DEBUG, "Could not be accomodated. Hence applying accomodated data to retention\n");
        /* TODO: this should set used data to SVD1 header after apply finishes.
         *       Also should return success if no changes are present to apply */
        if (!Flush())
        {
            bapplied = false;
        }
    }

    if (bapplied)
    {
		Cacher_t cp = m_Cacher[m_Profile];
        char *p = m_RetentionApplyBuffer.GetDataForUse();
		SV_UINT UsedLength = m_RetentionApplyBuffer.GetLength() - m_RetentionApplyBuffer.GetUsedLength();
        m_AccumulatedChangesForRetention++;
        SVD_PREFIX* dbPrefix = reinterpret_cast<SVD_PREFIX*>(p);
        dbPrefix->tag = SVD_TAG_DIRTY_BLOCK_DATA;
        dbPrefix->count = 1;
        dbPrefix->Flags = 0;
    
        SVD_DIRTY_BLOCK* dbHeader = reinterpret_cast<SVD_DIRTY_BLOCK*>(p + sizeof(SVD_PREFIX) );
        dbHeader->ByteOffset = offset ;
        dbHeader->Length = length;

		(this->*cp)(p + dbHeaderSize, (UsedLength - dbHeaderSize), data, length);

        m_RetentionApplyBuffer.AddToUsedLength(dbHeaderSize+length);
        m_lastChangeOffset = offset;
    }

    return bapplied;
}


void RetentionVolumeApplier::NoProfileCacher(char *cache, SV_UINT size, char *data, const SV_UINT &length)
{
	inm_memcpy_s(cache, size, data, length);
}


void RetentionVolumeApplier::ProfileCacher(char *cache, SV_UINT size, char *data, const SV_UINT &length)
{
	ACE_Time_Value TimeBeforeCache, TimeAfterCache;

	TimeBeforeCache = ACE_OS::gettimeofday();
	inm_memcpy_s(cache, size, data, length);
    TimeAfterCache = ACE_OS::gettimeofday();

    m_TimeForOverhead += (TimeAfterCache-TimeBeforeCache);
}


bool RetentionVolumeApplier::Flush(void)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if (0 == m_AccumulatedChangesForRetention)
    {
        DebugPrintf(SV_LOG_DEBUG, "For target %s, no accumulated changes to apply for retention\n", m_TargetVolumeName.c_str());
    }
    else
    {
        SV_LONGLONG totalDataApplied=0;

        /* TODO: Earlier lastReadOffset was being used for file name.
         * NEEDTOASK: supply the last offset in accumulated changes. 
         *            will this work ? */
        // construct unique file name to differentiate the file we store in t_applied table.
        VolumeWithOffsetToApply_t vo(m_TargetVolumeName, m_lastChangeOffset);
        std::string filenametoapply = m_FileNameProvider(vo);
        DebugPrintf(SV_LOG_DEBUG, "Applying %s\n", filenametoapply.c_str());

		if(!ApplyRetentionBuffer(filenametoapply,
					             totalDataApplied,
						         m_RetentionApplyBuffer.GetData(), 
						         m_RetentionApplyBuffer.GetUsedLength()))
        {
            std::stringstream msg;
            msg << "In RetentionApplier, " 
                << "Applychanges() failed while applying file: "
                << filenametoapply
                << ", for target " << m_TargetVolumeName;
            m_ErrorMessage += msg.str();
            rv = false;
        }

        if (rv)
        {
            /* NEEDTOASK: Is this used by CX to show unmatched bytes 
             * if yes, then unused clusters are not needed to send 
             * they come as matched ? */
			AppliedBytesUserFunction_t abf = m_AppliedBytesUserFunction[m_Profile];
            if (!(this->*abf)(totalDataApplied))
            {
                std::ostringstream msg;
                msg << "In RetentionApplier for " << m_TargetVolumeName << ", applier bytes user failed";
                m_ErrorMessage = msg.str();
                rv = false;
            }
        }

        if (rv)
        {
            //Reset the below counters properly to start over again.
            m_AccumulatedChangesForRetention = 0;
            m_RetentionApplyBuffer.ResetUsedLength();
            m_RetentionApplyBuffer.AddToUsedLength(headerSize);
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


bool RetentionVolumeApplier::ApplyRetentionBuffer(const std::string & filename,
												  long long& totalBytesApplied,
												  char* source,
												  const SV_ULONG sourceLength)
{
	bool applied = false;

    if (m_ShouldApply(m_TargetVolumeName))
	{
		Applychanges_t ac = m_Applychanges[m_Profile];
        applied = (this->*ac)(filename,
                              totalBytesApplied,
                              source,
                              sourceLength);
	}
	else
    {
        m_ErrorMessage = "could not apply, as pre processing before apply failed.";
    }

	return applied;
}


bool RetentionVolumeApplier::NoProfileApplychanges(const std::string & filename,
												  long long& totalBytesApplied,
												  char* source,
												  const SV_ULONG sourceLength)
{
	return m_pApplier->Applychanges(filename, totalBytesApplied, source, sourceLength);
}


bool RetentionVolumeApplier::ProfileApplychanges(const std::string & filename,
                             long long& totalBytesApplied,
		    				 char* source,
							 const SV_ULONG sourceLength)
{
	ACE_Time_Value TimeBeforeApply, TimeAfterApply;

    TimeBeforeApply = ACE_OS::gettimeofday();
    bool rval = m_pApplier->Applychanges(filename, totalBytesApplied, source, sourceLength);
    TimeAfterApply = ACE_OS::gettimeofday();

    m_TimeForApply += (TimeAfterApply-TimeBeforeApply);

    return rval;
}


bool RetentionVolumeApplier::NoProfileAppliedByteUserFunction(const SV_LONGLONG &dataapplied)
{
	return m_AppliedBytesUser(dataapplied);
}


bool RetentionVolumeApplier::ProfileAppliedByteUserFunction(const SV_LONGLONG &dataapplied)
{
	ACE_Time_Value TimeBeforeUse, TimeAfterUse;

    TimeBeforeUse = ACE_OS::gettimeofday();
	bool applied = m_AppliedBytesUser(dataapplied);
	TimeAfterUse = ACE_OS::gettimeofday();

	m_TimeForOverhead += (TimeAfterUse-TimeBeforeUse);

	return applied;
}


ACE_Time_Value RetentionVolumeApplier::GetTimeForApply(void)
{
	return m_TimeForApply;
}


ACE_Time_Value RetentionVolumeApplier::GetTimeForOverhead(void)
{
	return m_TimeForOverhead;
}


std::string RetentionVolumeApplier::GetErrorMessage(void)
{
    return m_ErrorMessage;
}
