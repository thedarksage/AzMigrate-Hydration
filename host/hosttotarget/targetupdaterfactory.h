
///
/// \file targetupdaterfactory.h
///
/// \brief target updater factory interface
///

#ifndef TARGETUPDATERFACTORY_H
#define TARGETUPDATERFACTORY_H

#include "targetupdaterimp.h"

namespace HostToTarget {

/// \brief target updater factory used to instantiate the correct target updater
    /// \param info host to target info used to determine which target updater to instantiate
    TargetUpdaterImp::ptr TargetUpdaterFactory(HostToTargetInfo const& info);
}


#endif // TARGETUPDATERFACTORY_H
