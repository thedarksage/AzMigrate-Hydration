#ifndef _MIRRORER__H_
#define _MIRRORER__H_

#include <set>
#include <string>
#include "prismsettings.h"
#include "devicestream.h"
#include "error.h"
#include "npwwnif.h"
#include "volumereporter.h"

class Mirrorer
{
public:
    virtual int FormMirrorSetupInput(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                                     const ATLunNames_t &atlunnames,
                                     const VolumeReporter::VolumeReport_t &vr,
                                     PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror) = 0; 
  
    virtual void RemoveMirrorSetupInput(void) = 0;

    virtual int SetupMirror(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                            DeviceStream &stream, 
                            PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror) = 0;

    virtual int DeleteMirror(const PRISM_VOLUME_INFO &prismvolumeinfo, 
                             DeviceStream &stream, 
                             PRISM_VOLUME_INFO::MIRROR_ERROR *pmirrorerror) = 0;

    virtual ~Mirrorer() {}
};

#endif /* _MIRRORER__H_ */
