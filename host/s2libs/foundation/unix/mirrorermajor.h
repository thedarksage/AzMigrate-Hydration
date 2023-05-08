#ifndef _MIRRORER__MAJOR__H_
#define _MIRRORER__MAJOR__H_

#include <set>
#include <string>
#include "mirrorer.h"
#include "drivermirrorerror.h"
#include "devicefilter.h"
#include "volumegroupsettings.h"

class MirrorerMajor : public Mirrorer
{
    mirror_conf_info_t m_MirrorConfInfo;
    DriverMirrorError m_DriverMirrorError;
    void InitMirrorConfInfo(void);
    void DeInitMirrorConfInfo(void);
    void PrintMirrorConfInfo(void);
    bool GetVolumeVendorForInVolFlt(const VolumeSummary::Vendor uservendor, inm_vendor_t *pinvolfltvendor);
    bool FillSrcAttrsInSetupInput(const PRISM_VOLUME_INFO &prismvolumeinfo, const VolumeReporter::VolumeReport_t &vr);
public:
    MirrorerMajor();
    ~MirrorerMajor();
    int FormMirrorSetupInput(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                             const std::set<std::string> &atlunnames,
                             const VolumeReporter::VolumeReport_t &vr,
                             PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror);

    virtual void RemoveMirrorSetupInput(void);

    int SetupMirror(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                    DeviceStream &stream, 
                    PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror);

    int DeleteMirror(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                     DeviceStream &stream, 
                     PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror);

    typedef std::map<VolumeSummary::Vendor, inm_vendor_t> UserToInVolFltVendor_t;
    typedef UserToInVolFltVendor_t::const_iterator ConstUserToInVolFltVendorIter_t;
};

#endif /* _MIRRORER__MAJOR__H_ */
