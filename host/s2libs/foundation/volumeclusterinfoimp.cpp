#include "volumeclusterinfoimp.h"
#include "inmstrcmp.h"
#include "inmnofsclusterinfo.h"
#include "logger.h"
#include "portablehelpers.h"


VolumeClusterInformerImp::VolumeClusterInformerImp() : 
m_pVciIf(0),
m_featureSupportState(E_FEATURECANTSAY)
{
}


bool VolumeClusterInformerImp::Init(const VolumeDetails &vd)
{
    bool brval = (ACE_INVALID_HANDLE != vd.m_Handle);

    if (brval)
    {
        brval = InitClusterInfoIf(vd);
        if (brval)
        {
            m_featureSupportState = m_pVciIf ? E_FEATURESUPPORTED : E_FEATURENOTSUPPORTED;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "failed to initialize cluster info interface\n");
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "invalid handle supplied for getting cluster info\n");
    }

    return brval;
}


bool VolumeClusterInformerImp::InitializeNoFsCluster(const SV_LONGLONG size, const SV_LONGLONG startoffset, const SV_UINT minimumclustersizepossible)
{
    bool binit = false;

    if (E_FEATURENOTSUPPORTED == m_featureSupportState)
    {
        if (0 == m_pVciIf)
        {
            m_pVciIf = new (std::nothrow) InmNoFsClusterInfo(size, startoffset, minimumclustersizepossible); 
            binit = m_pVciIf;
            if (false == binit)
            {
                DebugPrintf(SV_LOG_ERROR, "could not allocate memory for no file system cluster info\n");
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "no file system capabilities asked for even though some capability is set\n"); 
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "feature support state is %s and no file system capabilities asked for\n", 
                                  StrFeatureSupportState[m_featureSupportState]);
    }

    return binit;
}


SV_ULONGLONG VolumeClusterInformerImp::GetNumberOfClusters(void)
{
    SV_ULONGLONG nc = 0;
    
    if (m_pVciIf)
    {
        nc = m_pVciIf->GetNumberOfClusters();
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "number of clusters asked for, even though feature not supported\n");
    }

    return nc;
}


bool VolumeClusterInformerImp::GetVolumeClusterInfo(VolumeClusterInfo &vci)
{
    bool brval = false;
    
    if (m_pVciIf)
    {
        brval = m_pVciIf->GetVolumeClusterInfo(vci);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "information of clusters asked for, even though feature not supported\n");
    }

    return brval;
}


FeatureSupportState_t VolumeClusterInformerImp::GetSupportState(void)
{
    return m_featureSupportState;
}


VolumeClusterInformerImp::~VolumeClusterInformerImp()
{
    if (m_pVciIf)
    {
        delete m_pVciIf;
        m_pVciIf = 0;
    }
}
