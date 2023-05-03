///
///  \file  irprofiling.h
///
///  \brief
///

#ifndef IRPROFILING_H
#define IRPROFILING_H

#include "genericprofiler.h"

#define HCDCALC "hcdCalc"
#define HCDDOWNLOAD "hcdDownload"
#define CLUSTERLIST "clusterList"
#define CLUSTERUPLOAD "clusterUpload"
#define DISKREAD "diskRead"
#define SYNCUPLOAD "syncUpload"
#define HCDUPLOAD "hcdUpload"
#define HCDFILEUPLOAD "hcdFileUpload"
#define GETHCDAZUREAGENT "getHcdAzureAgent"
#define HCDLIST "hcdList"
#define SYNCDOWNLOAD "syncDownload"
#define SYNCLIST "syncList"
#define SYNCUNCOMPRESS "syncUncompress"
#define CLUSTERDOWNLOAD "clusterDownload"
#define SYNCUPLOADTOAZURE "syncUploadToAzure"
#define SYNCAPPLY "syncApply"

#define DEFAULT_NORM_SIZE (1024*1024) // default normalization size 1 MB for variable size profiling buckets

#define NEWPROFBKT(name) new ProfilingBuckets(name)
#define NEWPROFILER(name, verbose, file, delayedWrite) ProfilerFactory::getProfiler(GENERIC_PROFILER, name, NEWPROFBKT(name), verbose, file, delayedWrite)

#define NEWPROFBKT_WITHTS(name) new ProfilingBuckets(name, true)
#define NEWPROFILER_WITHTSBKT(name, verbose, file, delayedWrite) ProfilerFactory::getProfiler(GENERIC_PROFILER, name, NEWPROFBKT_WITHTS(name), verbose, file, delayedWrite)

#define NEWNORMPROFBKT_WITHTS(name) new NormProfilingBuckets(DEFAULT_NORM_SIZE, name, true)
#define NEWPROFILER_WITHNORMANDTSBKT(name, verbose, file, delayedWrite) ProfilerFactory::getProfiler(GENERIC_PROFILER, name, NEWNORMPROFBKT_WITHTS(name), verbose, file, delayedWrite)

class IRSummaryPostException
{
public:
    std::string m_PostDPException_args;
    std::vector<PrBuckets> m_IRSmry;

public:
    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "IRSummaryPostException", false);
        JSON_E(adapter, m_PostDPException_args);
        JSON_T(adapter, m_IRSmry);
    }

    void UpdateIRProfilingSummaryOnException(const std::string &strdpargs)
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        std::vector<PrBuckets> IRSmry;
        ProfilingSummaryFactory::summaryAsJson(IRSmry);
        if (!IRSmry.empty())
        {
            m_PostDPException_args = strdpargs;
            m_IRSmry = IRSmry;
            std::string smry = ";IRSS;" + JSON::producer<IRSummaryPostException>::convert(*this) + ";IRSE;";
            DebugPrintf(SV_LOG_ALWAYS, "%s: %s.\n", FUNCTION_NAME, smry.c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
    }
};

#endif //IRPROFILING_H