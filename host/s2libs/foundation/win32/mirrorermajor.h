#ifndef _MIRRORER__MAJOR__H_
#define _MIRRORER__MAJOR__H_

#include <set>
#include <string>
#include "mirrorer.h"
#include "error.h"

class MirrorerMajor : public Mirrorer
{
public:
    int FormMirrorSetupInput(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                             const ATLunNames_t &atlunnames,
                             const VolumeReporter::VolumeReport_t &vr,
                             PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror)
    {
        return SV_FAILURE;
    }

    void RemoveMirrorSetupInput(void)
    {
    }

    int SetupMirror(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                    DeviceStream &stream, 
                    PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror)
    {
        return SV_FAILURE;
    }

    int DeleteMirror(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                     DeviceStream &stream, 
                     PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror)
    {
        return SV_FAILURE;
    }

};

#endif /* _MIRRORER__MAJOR__H_ */
