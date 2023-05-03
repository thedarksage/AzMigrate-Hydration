#ifndef _CONSISTENCY_THREAD_RCM_H
#define _CONSISTENCY_THREAD_RCM_H

#include "consistencythread.h"
#include "RcmConfigurator.h"
#include "TagTelemetry.h"

using namespace RcmClientLib;
using namespace TagTelemetry;


class ConsistencyThreadRCM : public ConsistencyThread
{
public:
    ConsistencyThreadRCM() {};
    ~ConsistencyThreadRCM() {};

protected:
    virtual void GetConsistencyIntervals(uint64_t &crashInterval, uint64_t &appInterval);
    virtual bool IsMasterNode() const;
    virtual void SendAlert(InmAlert &inmAlert) const;
    virtual void AddDiskInfoInTagStatus(const TagTelemetry::TagType &tagType,
        const std::string &cmdOutput,
        TagTelemetry::TagStatus &tagStatus) const;
    uint32_t GetNoOfNodes();
    virtual bool IsTagFailureHealthIssueRequired() const;

private:
    ConsistencySettings m_consistencySettings;

    void GetConsistencySettings();
};


#endif // _CONSISTENCY_THREAD_RCM_H
