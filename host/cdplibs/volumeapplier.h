#ifndef VOLUME_APPLIER_H_
#define VOLUME_APPLIER_H_

#include <string>

#include "ace/Time_Value.h"

#include "svtypes.h"
#include "cdpvolume.h"

class VolumeApplier
{
public:

	typedef struct VolumeApplierFormationInputs
	{
		cdp_volume_t *m_pVolume;
		bool m_Profile;
		std::string m_Name;

		VolumeApplierFormationInputs(cdp_volume_t *pvolume, const bool &profile, const std::string &deviceName)
        : m_pVolume(pvolume),
		  m_Profile(profile),
		  m_Name(deviceName)
		{
		}

	} VolumeApplierFormationInputs_t;

    virtual bool Apply(const SV_LONGLONG &offset, const SV_UINT &length, char *data) = 0;
    virtual bool Flush(void) = 0;
    virtual std::string GetErrorMessage(void) = 0;
	virtual ACE_Time_Value GetTimeForApply(void) = 0;
	/* other than apply, time for caching etc,. */
	virtual ACE_Time_Value GetTimeForOverhead(void) = 0;
    virtual ~VolumeApplier() {}
};

#endif /* VOLUME_APPLIER_H_ */
