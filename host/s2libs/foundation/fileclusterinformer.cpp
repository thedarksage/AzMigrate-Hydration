#include "fileclusterinformer.h"
#include "logger.h"
#include "portablehelpers.h"


void FileClusterInformer::FileClusterRanges::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, "File cluster range%s:\n", (m_ClusterRanges.size()>1) ? "s" : "");
    DebugPrintf(SV_LOG_DEBUG, "support state: %s\n", StrFeatureSupportState[m_FeatureSupportState]);
    if (E_FEATURESUPPORTED == m_FeatureSupportState)
    {
        for (FileSystem::ClusterRangesIter_t it = m_ClusterRanges.begin(); it != m_ClusterRanges.end(); it++)
        {
            it->Print();
        }
    }
}
