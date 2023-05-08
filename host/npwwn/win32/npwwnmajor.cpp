#include "npwwn.h"

void GetLocalPwwnInfos(LocalPwwnInfos_t &lpwwns)
{
    /* nothing to do for now */
}

void DeleteATLunFromFcHost(const PIAT_LUN_INFO &piatluninfo,
                           const LocalPwwnInfo_t &lpwwn,
                           Channels_t &channels,
                           QuitFunction_t &qf,
                           RemotePwwnInfo_t &rpwwn)
{
}


void DeleteATLunFromScsiHost(const PIAT_LUN_INFO &piatluninfo,
                             const LocalPwwnInfo_t &lpwwn,
                             Channels_t &channels,
                             QuitFunction_t &qf,
                             RemotePwwnInfo_t &rpwwn)
{
}


void DeleteATLunFromQla(const PIAT_LUN_INFO &piatluninfo,
                        const LocalPwwnInfo_t &lpwwn,
                        Channels_t &channels,
                        QuitFunction_t &qf,
                        RemotePwwnInfo_t &rpwwn)
{
}


void DeleteATLunFromFcInfo(const PIAT_LUN_INFO &piatluninfo,
                           const LocalPwwnInfo_t &lpwwn,
                           Channels_t &channels,
                           QuitFunction_t &qf,
                           RemotePwwnInfo_t &rpwwn)
{
}


void DeleteATLunFromScli(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn,
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
}


void DeleteATLunFromLpfc(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn,
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
}


void GetATLunPathFromFcHost(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            const bool &bshouldscan,
                            Channels_t &channelnumbers,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn)
{
}


void GetATLunPathFromScsiHost(const PIAT_LUN_INFO &piatluninfo,
                              const LocalPwwnInfo_t &lpwwn,
                              const bool &bshouldscan,
                              Channels_t &channelnumbers,
                              QuitFunction_t &qf,
                              RemotePwwnInfo_t &rpwwn)
{
}


void GetATLunPathFromQla(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn,
                         const bool &bshouldscan,
                         Channels_t &channelnumbers,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
}

void GetATLunPathFromLpfc(const PIAT_LUN_INFO &piatluninfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn)
{
}
 
void GetATLunPathFromScli(const PIAT_LUN_INFO &piatluninfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn)
{
}

void GetATLunPathFromFcInfo(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            const bool &bshouldscan,
                            Channels_t &channelnumbers,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn)
{
}


void GetRemotePwwnsFromFcHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
}


void GetRemotePwwnsFromScsiHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
}


void GetRemotePwwnsFromQla(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
}


void GetRemotePwwnsFromFcInfo(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
}


void GetRemotePwwnsFromScli(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
}


void GetRemotePwwnsFromLpfc(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
}


void GetChannelNumbers(Channels_t &ChannelNumbers)
{
}


bool DeleteATLunsFromCumulation(const PIAT_LUN_INFO &piatluninfo, 
                                QuitFunction_t &qf, 
                                LocalPwwnInfos_t &lpwwns)
{
    return false;
}


void GetATLunPathsFromCumulation(const PIAT_LUN_INFO &piatluninfo, 
                                 const bool &bshouldscan,
                                 QuitFunction_t &qf, 
                                 LocalPwwnInfos_t &lpwwns, 
                                 std::set<std::string> &atlundirectpaths)
{
}

void DeleteATLunFromIScsiHost(const PIAT_LUN_INFO &prismvolumeinfo,
                         const LocalPwwnInfo_t &lpwwn,
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
    return;
}

void GetATLunPathFromIScsiHost(const PIAT_LUN_INFO &prismvolumeinfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn)
{
    return;
}

void GetRemotePwwnsFromIScsiHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    return;
}

void DiscoverRemoteIScsiTargets(std::list<std::string> networkAddresses)
{
    return;
}

void DiscoverRemoteFcTargets()
{
    return;
}
