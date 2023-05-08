
///
/// \file unix/targetupdaterimpmajor.h
///
/// \brief
///

#ifndef TARGETUPDATERIMPMAJOR_H
#define TARGETUPDATERIMPMAJOR_H

#include "targetupdaterimp.h"

namespace HostToTarget {

    class TargetUpdaterImpMajor : public TargetUpdaterImp {
    public:
        explicit TargetUpdaterImpMajor(HostToTargetInfo const& info)
            :  TargetUpdaterImp(info)
            {
            }

        ~TargetUpdaterImpMajor()
            {
            }

        virtual bool validate();

        virtual bool update(int opts);

    protected:

    private:
        
    };


} // namesapce HostToTarget

#endif // TARGETUPDATERIMPMAJOR_H
