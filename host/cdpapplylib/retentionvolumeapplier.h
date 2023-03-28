#ifndef RETENTION_VOLUME_APPLIER_H_
#define RETENTION_VOLUME_APPLIER_H_

#include <string>

#include "ace/Time_Value.h"
#include <boost/function.hpp>

#include "svtypes.h"
#include "volumeapplier.h"
#include "cdpvolume.h"
#include "inmdefines.h"
#include "cdpapply.h"
#include "dataprotectionutils.h"
#include "portablehelpers.h"

class RetentionVolumeApplier : public VolumeApplier
{
public:
    bool Apply(const SV_LONGLONG &offset, const SV_UINT &length, char *data);
    bool Flush(void);
	ACE_Time_Value GetTimeForApply(void);
	ACE_Time_Value GetTimeForOverhead(void);
    std::string GetErrorMessage(void);

public:
    typedef boost::function<std::string (const VolumeWithOffsetToApply_t volumewithoffsettoapply)> FileNameProvider_t;
    typedef boost::function<CDPApply * (const std::string deviceName)> CDPApplierInstantiator_t;
    typedef boost::function<void (CDPApply *pcdpapply)> CDPApplierDestroyer_t;
    typedef boost::function<bool (const SV_LONGLONG bytesapplied)> AppliedBytesUser_t;
    typedef boost::function<bool (const std::string deviceName)> ShouldApply_t;

    RetentionVolumeApplier(VolumeApplierFormationInputs_t &inputs, 
                           FileNameProvider_t &filenameprovider,
                           CDPApplierInstantiator_t &cdpapplierinstantiator, 
                           CDPApplierDestroyer_t &cdpapplierdestroyer,
                           AppliedBytesUser_t &appliedbytesuser,
                           ShouldApply_t &shouldapply);
    ~RetentionVolumeApplier();
    bool IsValid(void);
    bool Init(const SV_UINT &nchanges, const SV_UINT &bufsize);

private:
	typedef bool (RetentionVolumeApplier::*Applychanges_t)(const std::string & filename,
														   long long& totalBytesApplied,
														   char* source,
														   const SV_ULONG sourceLength);
	bool NoProfileApplychanges(const std::string & filename,
                               long long& totalBytesApplied,
							   char* source,
							   const SV_ULONG sourceLength);
	bool ProfileApplychanges(const std::string & filename,
                             long long& totalBytesApplied,
		    				 char* source,
							 const SV_ULONG sourceLength);
	bool ApplyRetentionBuffer(const std::string & filename,
							  long long& totalBytesApplied,
							  char* source,
							  const SV_ULONG sourceLength);

	typedef void (RetentionVolumeApplier::*Cacher_t)(char *cache, SV_UINT size, char *data, const SV_UINT &length);
	void NoProfileCacher(char *cache, SV_UINT size, char *data, const SV_UINT &length);
	void ProfileCacher(char *cache, SV_UINT size, char *data, const SV_UINT &length);

	typedef bool (RetentionVolumeApplier::*AppliedBytesUserFunction_t)(const SV_LONGLONG &dataapplied);
	bool NoProfileAppliedByteUserFunction(const SV_LONGLONG &dataapplied);
	bool ProfileAppliedByteUserFunction(const SV_LONGLONG &dataapplied);

private:
    DataWithUsedLength m_RetentionApplyBuffer;
    std::string m_TargetVolumeName;
    std::string m_ErrorMessage;
    SV_LONGLONG m_lastChangeOffset;
    SV_UINT m_AccumulatedChangesForRetention;
    FileNameProvider_t m_FileNameProvider;
    CDPApplierInstantiator_t m_CDPApplierInstantiator;
    CDPApplierDestroyer_t m_CDPApplierDestroyer;
    AppliedBytesUser_t m_AppliedBytesUser;
    ShouldApply_t m_ShouldApply;
    CDPApply *m_pApplier;
	bool m_Profile;
	ACE_Time_Value m_TimeForApply;
	ACE_Time_Value m_TimeForOverhead;
	Applychanges_t m_Applychanges[NBOOLS];
	Cacher_t m_Cacher[NBOOLS];
	AppliedBytesUserFunction_t m_AppliedBytesUserFunction[NBOOLS];
};

#endif /* RETENTION_VOLUME_APPLIER_H_ */
