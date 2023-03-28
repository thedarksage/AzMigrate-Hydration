#ifndef _CONSISTENCY_THREAD_CS_H
#define _CONSISTENCY_THREAD_CS_H

#include "consistencythread.h"
#include "rpcconfigurator.h"
#include "ConsistencySettings.h"


class ConsistencyThreadCS : public ConsistencyThread
{
public:
    ConsistencyThreadCS(Configurator* configurator);
    ~ConsistencyThreadCS();

protected:
    virtual void GetConsistencyIntervals(uint64_t &crashInterval, uint64_t &appInterval);
    virtual bool IsMasterNode() const;
    virtual void SendAlert(InmAlert &inmAlert) const;
    virtual void AddDiskInfoInTagStatus(const TagTelemetry::TagType &tagType,
        const std::string &cmdOutput,
        TagTelemetry::TagStatus &tagStatus) const;
    virtual uint32_t GetNoOfNodes();
    virtual bool IsTagFailureHealthIssueRequired() const;

private:
    CS::ConsistencySettings m_consistencySettings;
    Configurator* m_configurator;

    SVSTATUS GetProtectedDeviceIds(std::set<std::string> &deviceIds) const;
};


#endif // _CONSISTENCY_THREAD_CS_H
