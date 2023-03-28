#ifndef SIMPLE_VOLUME_APPLIER_H_
#define SIMPLE_VOLUME_APPLIER_H_

#include <string>

#include "ace/Time_Value.h"

#include "svtypes.h"
#include "volumeapplier.h"
#include "cdpvolume.h"
#include "portablehelpers.h"

class SimpleVolumeApplier : public VolumeApplier
{
public:
	SimpleVolumeApplier(VolumeApplierFormationInputs_t &inputs);
    ~SimpleVolumeApplier();
    bool Apply(const SV_LONGLONG &offset, const SV_UINT &length, char *data);
    bool Flush(void);
	ACE_Time_Value GetTimeForApply(void);
	ACE_Time_Value GetTimeForOverhead(void);
    std::string GetErrorMessage(void);

private:
	typedef bool (SimpleVolumeApplier::*Write_t)(const SV_LONGLONG &offset, const SV_UINT &length, char *data);

	bool Write(const SV_LONGLONG &offset, const SV_UINT &length, char *data);
	bool NoProfileWrite(const SV_LONGLONG &offset, const SV_UINT &length, char *data);
	bool ProfileWrite(const SV_LONGLONG &offset, const SV_UINT &length, char *data);

private:
    cdp_volume_t *m_pVolume;
	bool m_Profile;
    std::string m_ErrorMessage;
	ACE_Time_Value m_TimeForApply;
	Write_t m_Write[NBOOLS];
};

#endif /* SIMPLE_VOLUME_APPLIER_H_ */
