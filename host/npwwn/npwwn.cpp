#include "npwwn.h"
/* #include "npwwnminor.h" */
#include "logger.h"
#include "portable.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "event.h"
#include "inmstrcmp.h"


void InsertNPWwn(const NwwnPwwn_t &npwwn, NwwnPwwns_t &npwwns)
{
    /* TODO: should we insert only when pwwn is non empty */
    if ( /* !npwwn.m_Nwwn.empty() || */ !npwwn.m_Pwwn.empty())
    {
        npwwns.push_back(npwwn);
    }
}


void GetWwnFromNVPair(
                      const std::string &namevaluepair,
                      const std::string &sep,
                      std::string &value
                     )
{
    size_t validx = namevaluepair.find(sep);
    if ((std::string::npos != validx) && (validx < (namevaluepair.length() - 1)))
    {
        validx++;
        value = namevaluepair.substr(validx);
    }
}


void PrintNPWwns(const NwwnPwwns_t &npwwns)
{
    for_each(npwwns.begin(), npwwns.end(), PrintNPwwn);
}


void PrintNPwwn(const NwwnPwwn &npwwn)
{
    DebugPrintf(SV_LOG_DEBUG, "======\n");
    DebugPrintf(SV_LOG_DEBUG, "Node WWN: %s\n", npwwn.m_Nwwn.c_str());
    DebugPrintf(SV_LOG_DEBUG, "Port WWN: %s\n", npwwn.m_Pwwn.c_str());
    DebugPrintf(SV_LOG_DEBUG, "======\n");
}


void GetRemotePwwns(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    bool bneedtoquit = (qf && qf(0));
    if (!bneedtoquit)
    {
        GetRemotePwwnsFuns[pLocalPwwnInfo->m_PwwnSrc](pLocalPwwnInfo, qf);
    }
}


void GetRemotePwwnsFromUnknownSrc(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /* TODO: should this be error ? */
    DebugPrintf(SV_LOG_ERROR, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
    DebugPrintf(SV_LOG_ERROR, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
}


void GetATLunPathFromUnknownSrc(const PIAT_LUN_INFO &piatluninfo,
                                const LocalPwwnInfo_t &lpwwn,
                                const bool &bshouldscan,
                                Channels_t &channelnumbers,
                                QuitFunction_t &qf,
                                RemotePwwnInfo_t &rpwwn)
{
    DebugPrintf(SV_LOG_ERROR, "local pwwn %s, remote pwwn %s, came from unknown source for "
                              " source LUN ID %s to get at lun paths\n", lpwwn.m_Pwwn.m_Name.c_str(), rpwwn.m_Pwwn.m_Name.c_str(),
                              piatluninfo.sourceLUNID.c_str());
}


void DeleteATLunFromUnknownSrc(const PIAT_LUN_INFO &piatluninfo, 
                               const LocalPwwnInfo_t &lpwwn, 
                               Channels_t &channels,
                               QuitFunction_t &qf,
                               RemotePwwnInfo_t &rpwwn)
{
}


void GetATLunNames(const PIAT_LUN_INFO &piatluninfo,
                   QuitFunction_t qf, 
                   ATLunNames_t &atlunnames)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
    LocalPwwnInfos_t lpwwns;
    ATLunNames_t atlundirectpaths;

    GetLocalPwwnInfos(lpwwns);
    bool bshouldscan = true;
    GetATLunPaths(piatluninfo, bshouldscan, qf, lpwwns, atlundirectpaths);
    bool bneedtoquit = (qf && qf(0));
    if (bneedtoquit)
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is requested while getting AT lun names for "
                                  "source LUN ID %s\n", piatluninfo.sourceLUNID.c_str());
    }
    else
    {
        CopyATLunNames(lpwwns, atlundirectpaths, atlunnames);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}


void GetATLunPaths(const PIAT_LUN_INFO &piatluninfo, 
                   const bool &bshouldscan, 
                   QuitFunction_t &qf,
                   LocalPwwnInfos_t &lpwwns,
                   ATLunNames_t &atlundirectpaths)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
    bool bneedtoquit = (qf && qf(0));
    bool bfoundcxsentremoteport = false;
    bool bneedrescan = true;

rescan:

    std::list<std::string>::const_iterator piiter = piatluninfo.physicalInitiatorPWWNs.begin();
    for ( /* empty */ ; (piiter != piatluninfo.physicalInitiatorPWWNs.end()) && !bneedtoquit; piiter++)
    {
        const std::string &piwwn = *piiter;

        LocalPwwnInfos_t::iterator liter = lpwwns.find(piwwn);
        /* TODO: how to avoid duplicate PIs sent by CX ? 
         * similarly for duplicate ATs ? */
        if (liter != lpwwns.end())
        {
        for (/*empty*/; liter != lpwwns.end() && !bneedtoquit; liter++)
        {
            LocalPwwnInfo_t &lpwwn = liter->second;
            GetRemotePwwns(&lpwwn, qf); // Sunil: Called only from here, just one place,
												// gets only valid remote-pwwns, non-negative scsi_target_id, "FCP Target" role, "online" state
												// What if remote-port-wwn list for this Cx supplied PI is empty? or all/some are present but invalid?
             
            bneedtoquit = (qf && qf(0));
            if (!bneedtoquit)
            {
                if (PWWNSRCUNKNOWN == lpwwn.m_PwwnSrc)
                {
                    DebugPrintf(SV_LOG_ERROR, "The physical initiator pwwn %s is not present in host. source LUN ID is %s\n", 
                                lpwwn.m_Pwwn.m_Name.c_str(), 
                                piatluninfo.sourceLUNID.c_str());
                    /* TODO: should we return from here ? 
                     * looks to be error on which nothing should proceed */
                    /* If we scan for unknown src, it should be just be nop */
                }
                else if (lpwwn.m_RemotePwwnInfos.empty())
                {
                    /* TODO: should this also be error ? 
                     *       need to immediately know how
                     *       CX is sending the PIs and then 
                     *       this can be recorded as error */
                    DebugPrintf(SV_LOG_WARNING, "For physical initiator pwwn %s, there are no remote ports. "
                                                "source LUN ID is %s\n", lpwwn.m_Pwwn.m_Name.c_str(), 
                                                piatluninfo.sourceLUNID.c_str());
					// Sunil: May be we should do a Full Scan if remote pwwns, though invalid appear in PIs remote-pwwn list
                }
                else 
                {
                    std::list<std::string>::const_iterator atiter = piatluninfo.applianceTargetPWWNs.begin();
                    for ( /* empty */ ; (atiter != piatluninfo.applianceTargetPWWNs.end()) && !bneedtoquit; atiter++)
                    {
                        const std::string &atpwwn = *atiter;
                        RemotePwwnInfos_t::iterator riter = lpwwn.m_RemotePwwnInfos.find(atpwwn);
                        if (riter != lpwwn.m_RemotePwwnInfos.end()) // Sunil: What if none of Cx suppplied remote-ports are valid?
                        {
                            DebugPrintf(SV_LOG_DEBUG, "CX sent AT %s, is remote port of PI %s\n", 
                                        atpwwn.c_str(), lpwwn.m_FormattedName.c_str());
                            RemotePwwnInfo_t &rpwwn = riter->second;
                            rpwwn.m_IsSentFromCX = true;
                            bfoundcxsentremoteport = true;
                        }
                    }
                }
                PrintLocalPwwn(*liter);
            }
        }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "PI PWWN %s sent by CX is not found locally\n", piwwn.c_str());
        }
    }

    if (!bfoundcxsentremoteport)
    {
        DiscoverRemoteFcTargets();
        DiscoverRemoteIScsiTargets(piatluninfo.applianceNetworkAddress);
    }
    
    if (bneedrescan)
    {
        bneedrescan=false;
        goto rescan;
    }

    if (!bneedtoquit && !lpwwns.empty())
    {
        GetATLunPathsFromPIList(piatluninfo, bshouldscan, qf, lpwwns, atlundirectpaths);
    }
     
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}


void GetATLunPathsFromPIList(const PIAT_LUN_INFO &piatluninfo, 
                             const bool &bshouldscan,
                             QuitFunction_t &qf,
                             LocalPwwnInfos_t &lpwwns, 
                             std::set<std::string> &atlundirectpaths)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
    bool bneedtoquit = (qf && qf(0));

    LocalPwwnInfos_t::iterator liter = lpwwns.begin();
    /* TODO: remove the fc check once we implement iscsi */
    for ( /* empty */ ; (liter != lpwwns.end()) && !bneedtoquit; liter++)
    {
        LocalPwwnInfo_t &lpwwn = liter->second;
     //   if (PWWNTYPEFC == lpwwn.m_Pwwn.m_Type)
        {
            GetATLunPathsFromPI(piatluninfo, bshouldscan, qf, lpwwn);
        }
#if 0
        else
        {
            /* TODO: should this be warning ? */
            DebugPrintf(SV_LOG_ERROR, "PI pwwn type %s is not currently handled for "
                                      "scanning; PI wwn %s, source LUN ID %s\n", 
                                      PwwnTypeStr[lpwwn.m_Pwwn.m_Type], 
                                      lpwwn.m_Pwwn.m_Name.c_str(),
                                      piatluninfo.sourceLUNID.c_str());
        }
#endif
        bneedtoquit = (qf && qf(0));

    }

    if (!bneedtoquit && AreLpwwnsCumulative(lpwwns))
    {
        DebugPrintf(SV_LOG_DEBUG, "For source LUN ID %s, lpwwns are cumulative\n", 
                                  piatluninfo.sourceLUNID.c_str());
        GetATLunPathsFromCumulation(piatluninfo, bshouldscan, qf, lpwwns, atlundirectpaths); // Sunil: stub for linux
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}


void GetATLunPathsFromPI(const PIAT_LUN_INFO &piatluninfo, 
                         const bool &bshouldscan, 
                         QuitFunction_t &qf, 
                         LocalPwwnInfo_t &lpwwn)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with source LUN ID: %s, PI: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str(),
                             lpwwn.m_Pwwn.m_Name.c_str());
    bool bneedtoquit = (qf && qf(0));
    
    RemotePwwnInfos_t &rpwwns = lpwwn.m_RemotePwwnInfos;
    RemotePwwnInfos_t::iterator riter = rpwwns.begin();
    /* TODO: remove the fc check once we implement iscsi */
    for ( /* empty */; (riter != rpwwns.end()) && !bneedtoquit; riter++)
    {
        RemotePwwnInfo_t &rpwwn = riter->second;
#if 0
        if (PWWNTYPEFC != rpwwn.m_Pwwn.m_Type)
        {
            DebugPrintf(SV_LOG_ERROR, "AT pwwn type %s is not currently handled for "
                                      "scanning; PI wwn %s, AT wwn %s, source LUN ID %s\n", 
                                      PwwnTypeStr[rpwwn.m_Pwwn.m_Type], 
                                      lpwwn.m_Pwwn.m_Name.c_str(),
                                      rpwwn.m_Pwwn.m_Name.c_str(),
                                      piatluninfo.sourceLUNID.c_str());
        }
        else 
#endif
        if (rpwwn.m_IsSentFromCX)
        {
            Channels_t channels;
            GetChannelNumbers(channels);
            GetATLunPathFromPIAT[lpwwn.m_PwwnSrc](piatluninfo, lpwwn,
                                                  bshouldscan, channels,
                                                  qf, rpwwn);
            /* No need of checking quit here and 
             * logging also, since quit is checked at
             * upper level and logging is inside the 
             * functions */
            /* TODO: Need to discuss this policy where in
             * currently it follows at least one at lun  
             * visible, then return success */
        }
        bneedtoquit = (qf && qf(0));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with source LUN ID: %s, PI: %s\n",
                              FUNCTION_NAME, piatluninfo.sourceLUNID.c_str(), 
                              lpwwn.m_Pwwn.m_Name.c_str());
}


bool DeleteATLunNames(const PIAT_LUN_INFO &piatluninfo, QuitFunction_t qf)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
    LocalPwwnInfos_t lpwwns;
    std::set<std::string> atlundirectpaths;
    bool bshouldscan = false;
    bool bgotatluns = true;

    GetLocalPwwnInfos(lpwwns);
    GetATLunPaths(piatluninfo, bshouldscan, qf, lpwwns, atlundirectpaths);

    // Sunil: For Cx supplied PIs, find valid remote-wwns, for cx supplied valid remote-ports
    // , find lun state => go over all channels => open /sys/class/scsi_device file,
    // /dev file, do lun open/scsi-inquiry/proc lookup and find lun state
    // (valid, hidden, needs-cleanup, not visible)

    bool bneedtoquit = (qf && qf(0));
    if (!bneedtoquit)
    {
        /* TODO: confirm that if quit is not requested, 
         *       then GetATLunPaths gives atleast one AT
         *       Lun; else regardless of its return value, 
         *       if atleast one AT Lun is visible, delete 
         *       but for now return value check is enough */
        bgotatluns = GetCountOfATLuns(lpwwns, atlundirectpaths); // Sunil: for all Cx supplied PIs and ATs, why do this ???
        if (bgotatluns)
        {
            DebugPrintf(SV_LOG_DEBUG, "AT Luns are visible for source LUN ID %s. " 
                                      "Hence deleting them.\n", 
                                      piatluninfo.sourceLUNID.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "AT luns are not visible for source LUN ID %s.\n",
                                      piatluninfo.sourceLUNID.c_str());
            // Sunil: why go further ?
        }

        DeleteATLuns(piatluninfo, lpwwns, qf);
        bgotatluns = GetCountOfIndividualATLuns(lpwwns);

        if (AreLpwwnsCumulative(lpwwns))
        {
            DebugPrintf(SV_LOG_DEBUG, "For source lun ID %s, pwwns are cumulative\n", 
                                      piatluninfo.sourceLUNID.c_str());
            bool bdeletedfromcumulation = DeleteATLunsFromCumulation(piatluninfo, qf, lpwwns);
            bgotatluns = (false == bdeletedfromcumulation);
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is requested while deleting AT lun names for "
                                  "source LUN ID %s\n", piatluninfo.sourceLUNID.c_str());
    }
  

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
    return (false == bgotatluns);
}


void DeleteATLuns(const PIAT_LUN_INFO &piatluninfo, 
                  LocalPwwnInfos_t &lpwwns, 
                  QuitFunction_t &qf)
{
    std::stringstream msg;
    msg << "source LUN ID: " << piatluninfo.sourceLUNID
        << ", AT Lun Name: " << piatluninfo.applianceTargetLUNName
        << ", AT Lun Number: " << piatluninfo.applianceTargetLUNNumber;
    bool bneedtoquit = (qf && qf(0));
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with details: %s\n",
                              FUNCTION_NAME, msg.str().c_str());

    LocalPwwnInfos_t::iterator liter = lpwwns.begin();
    for ( /* empty */ ; (liter != lpwwns.end()) && !bneedtoquit; liter++)
    {
        LocalPwwnInfo_t &lpwwn = liter->second;
#if 0
        if (PWWNTYPEFC != lpwwn.m_Pwwn.m_Type)
        {
            DebugPrintf(SV_LOG_ERROR, "PI pwwn type %s is not currently handled for "
                                      "deletion; PI wwn %s, source LUN ID %s\n", 
                                      PwwnTypeStr[lpwwn.m_Pwwn.m_Type], 
                                      lpwwn.m_Pwwn.m_Name.c_str(),
                                      piatluninfo.sourceLUNID.c_str());
            bneedtoquit = (qf && qf(0));
        }
        else
#endif
        {
            DeleteATLunsFromPI(piatluninfo, lpwwn, qf);
            bneedtoquit = (qf && qf(0));
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());
}


void DeleteATLunsFromPI(const PIAT_LUN_INFO &piatluninfo, 
                        LocalPwwnInfo_t &lpwwn, 
                        QuitFunction_t &qf)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with source LUN ID: %s, PI: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str(),
                             lpwwn.m_Pwwn.m_Name.c_str());
    bool bneedtoquit = (qf && qf(0));

    RemotePwwnInfos_t &rpwwns = lpwwn.m_RemotePwwnInfos;
    RemotePwwnInfos_t::iterator riter = rpwwns.begin();
    for ( /* empty */; (riter != rpwwns.end()) && !bneedtoquit; riter++)
    {
        RemotePwwnInfo_t &rpwwn = riter->second;
#if 0
        if (PWWNTYPEFC != rpwwn.m_Pwwn.m_Type)
        {
            DebugPrintf(SV_LOG_ERROR, "AT pwwn type %s is not currently handled for "
                                      "deletion; PI wwn %s, AT wwn %s, source LUN ID %s\n", 
                                      PwwnTypeStr[rpwwn.m_Pwwn.m_Type], 
                                      lpwwn.m_Pwwn.m_Name.c_str(),
                                      rpwwn.m_Pwwn.m_Name.c_str(),
                                      piatluninfo.sourceLUNID.c_str());
        }
        else 
#endif
        if (rpwwn.m_IsSentFromCX)
        {
            Channels_t channels;
            GetChannelNumbers(channels);
            DeleteATLunFromPIAT[lpwwn.m_PwwnSrc](piatluninfo, lpwwn, channels, qf, rpwwn);
        }
        bneedtoquit = (qf && qf(0));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with source LUN ID: %s, PI: %s\n",
                              FUNCTION_NAME, piatluninfo.sourceLUNID.c_str(), 
                              lpwwn.m_Pwwn.m_Name.c_str());
}


void PrintPwwn(const PwwnInfo_t &pwwn)
{
    DebugPrintf(SV_LOG_DEBUG, "pwwn: %s\n", pwwn.m_Name.c_str());
    DebugPrintf(SV_LOG_DEBUG, "number: " LLSPEC "\n", pwwn.m_Number);
    DebugPrintf(SV_LOG_DEBUG, "type: %s\n", PwwnTypeStr[pwwn.m_Type]);
    DebugPrintf(SV_LOG_DEBUG, "nwwn: %s\n", pwwn.m_Nwwn.c_str());
    DebugPrintf(SV_LOG_DEBUG, "formatted nwwn: %s\n", pwwn.m_FormattedNwwn.c_str());
}


void PrintLocalPwwn(const LocalPwwnPair_t &lpwwnpair)
{
    const LocalPwwnInfo_t &lpwwn = lpwwnpair.second;
    const std::string &pwwn = lpwwnpair.first;
    DebugPrintf(SV_LOG_DEBUG, "======\n");
    DebugPrintf(SV_LOG_DEBUG, "Local Pwwn Information:\n");
    DebugPrintf(SV_LOG_DEBUG, "Formatted PWWN(map key): %s\n", pwwn.c_str());
    DebugPrintf(SV_LOG_DEBUG, "formatted pwwn: %s\n", lpwwn.m_FormattedName.c_str());
    PrintPwwn(lpwwn.m_Pwwn);
    DebugPrintf(SV_LOG_DEBUG, "pwwn source: %s\n", PwwnSrcStr[lpwwn.m_PwwnSrc]);
    for_each(lpwwn.m_RemotePwwnInfos.begin(), lpwwn.m_RemotePwwnInfos.end(), PrintRemotePwwn);
    DebugPrintf(SV_LOG_DEBUG, "======\n");
}


void PrintRemotePwwn(const RemotePwwnPair_t &rpwwnpair)
{
    const RemotePwwnInfo_t &rpwwn = rpwwnpair.second;
    const std::string &pwwn = rpwwnpair.first;
    DebugPrintf(SV_LOG_DEBUG, "---\n");
    DebugPrintf(SV_LOG_DEBUG, "Remote Pwwn Information:\n");
    DebugPrintf(SV_LOG_DEBUG, "Formatted PWWN(map key): %s\n", pwwn.c_str());
    DebugPrintf(SV_LOG_DEBUG, "formatted pwwn: %s\n", rpwwn.m_FormattedName.c_str());
    PrintPwwn(rpwwn.m_Pwwn);
    DebugPrintf(SV_LOG_DEBUG, "Is sent from CX: %s\n", STRBOOL(rpwwn.m_IsSentFromCX));
    DebugPrintf(SV_LOG_DEBUG, "channel number: " LLSPEC "\n", rpwwn.m_ChannelNumber);
    DebugPrintf(SV_LOG_DEBUG, "AT LUN path: %s\n", rpwwn.m_ATLunPath.c_str());
    DebugPrintf(SV_LOG_DEBUG, "---\n");
}

bool IsIscsiPwwn(const std::string &pwwn)
{
    bool bisiscsipwwn = false;
    const char *p = pwwn.c_str();

    for (int i = 0; i < NELEMS(IscsiPwwnPrefixes); i++)
    {
        size_t prefixlen = strlen(IscsiPwwnPrefixes[i]);
        if (0 == strncmp(p, IscsiPwwnPrefixes[i], prefixlen))
        {
            bisiscsipwwn = true;
            break;
        }
    }
  
    return bisiscsipwwn;
}


void FormatWwn(const E_PORTWWNSRC epwwnsrc, const std::string &unformattedwwn, std::string &formattedwwn, 
               E_PORTWWNTYPE *pepwwntype)
{
    std::stringstream msg;
    msg << "wwn source: " << PwwnSrcStr[epwwnsrc]
        << ", unformatted wwn: " << unformattedwwn;
    /* DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n", FUNCTION_NAME, msg.str().c_str()); */

    /* some times, the iscsi pwwn is enclosed in 
     * double quotes; first need to trim these if present; 
     * current character that are trimmed are "\"\';" */
    std::string trimmedwwn = unformattedwwn;
    Trim(trimmedwwn, CHARSTOTRIMFROMWWN);

    /*
    DebugPrintf(SV_LOG_DEBUG, "For %s, after trimming, wwn is %s\n", 
                msg.str().c_str(), trimmedwwn.c_str());
    */

    *pepwwntype = PWWNTYPEUNKNOWN;
    if (trimmedwwn.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, the wwn is empty\n", msg.str().c_str());
    }
    else if (IsIscsiPwwn(trimmedwwn))
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, the wwn is iscsi\n", msg.str().c_str());
        formattedwwn = trimmedwwn;
        *pepwwntype = PWWNTYPEISCSI;
    }
    else if (IsWwnFormatted<WWNFC_t>(trimmedwwn))
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, the wwn is already in format\n", msg.str().c_str());
        formattedwwn = trimmedwwn;
        *pepwwntype = PWWNTYPEFC;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, the wwn is not iscsi\n", msg.str().c_str());
        WWNFC_t wwn = 0;
        FormUniversalWwn<WWNFC_t>(trimmedwwn, &wwn);
        if (wwn)
        {
            /*
            std::stringstream ssnumber;
            ssnumber << wwn;
            DebugPrintf(SV_LOG_DEBUG, "For %s, the numerical wwn is: %s\n", 
                                      msg.str().c_str(), ssnumber.str().c_str());
            */
            FormatUniversalWwn<WWNFC_t>(&wwn, formattedwwn);
            DebugPrintf(SV_LOG_DEBUG, "For %s, the formatted wwn is %s\n", msg.str().c_str(), 
                                      formattedwwn.c_str());
            if (false == IsWwnFormatted<WWNFC_t>(formattedwwn))
            {
                DebugPrintf(SV_LOG_WARNING, "for %s, could not format wwn\n", msg.str().c_str());
                formattedwwn.clear();
            }
            else
            {
                *pepwwntype = PWWNTYPEFC;
            }
        }
        else
        {
            /* TODO: should this be error ? */
            DebugPrintf(SV_LOG_WARNING, "For %s, the numerical wwn got is zero."
                                        "Hence wwn cannot be formatted\n", msg.str().c_str());
        }
    }

    /* DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); */
}


void InsertLocalPwwn(LocalPwwnInfos_t &lpwwns, const LocalPwwnInfo_t &lpwwn)
{
    if (!lpwwn.m_FormattedName.empty())
    {
        lpwwns.insert(std::make_pair(lpwwn.m_FormattedName, lpwwn));
    }
}


void InsertRemotePwwn(RemotePwwnInfos_t &rpwwns, const RemotePwwnInfo_t &rpwwn)
{
    if (!rpwwn.m_FormattedName.empty())
    {
        rpwwns.insert(std::make_pair(rpwwn.m_FormattedName, rpwwn));
    }
}


void InsertRemotePwwnData(MultiRemotePwwnData_t &mrpwwns, const RemotePwwnInfo_t &rpwwn)
{
    if (!rpwwn.m_FormattedName.empty())
    {
        MultiRemotePwwnData_t::iterator riter = mrpwwns.find(rpwwn.m_FormattedName);
        if (riter != mrpwwns.end())
        {
            RemotePwwnData_t &rpwwndata = riter->second;
            rpwwndata.push_back(rpwwn);
        }
        else
        {
            RemotePwwnData_t rpwwndata;
            rpwwndata.push_back(rpwwn);
            mrpwwns.insert(std::make_pair(rpwwn.m_FormattedName, rpwwndata));
        }
    }
}


void GetNwwnPwwns(NwwnPwwns_t &npwwns)
{
    LocalPwwnInfos_t lpwwns;
    
    GetLocalPwwnInfos(lpwwns);
    CopyLocalPwwnInfos(lpwwns, npwwns);
}


void CopyLocalPwwnInfos(const LocalPwwnInfos_t &lpwwns, NwwnPwwns_t &npwwns)
{
    LocalPwwnInfos_t::const_iterator liter = lpwwns.begin();
    for ( /* empty */; liter != lpwwns.end(); liter++)
    {
        const LocalPwwnInfo_t &lpwwn = liter->second;
        NwwnPwwn_t npwwn;
        npwwn.m_Pwwn = lpwwn.m_FormattedName;
        npwwn.m_Nwwn = lpwwn.m_Pwwn.m_FormattedNwwn;
        InsertNPWwn(npwwn, npwwns);
    }
}


void CopyATLunNames(const LocalPwwnInfos_t &lpwwns, const ATLunNames_t &atlundirectpaths, ATLunNames_t &atlunnames)
{
    LocalPwwnInfos_t::const_iterator liter = lpwwns.begin();

    for ( /* empty */ ; liter != lpwwns.end(); liter++)
    {
        const LocalPwwnInfo_t &lpwwn = liter->second;
        RemotePwwnInfos_t::const_iterator riter = lpwwn.m_RemotePwwnInfos.begin();
        for ( /* empty */ ; riter != lpwwn.m_RemotePwwnInfos.end(); riter++)
        {
            const RemotePwwnInfo_t &rpwwn = riter->second;
            if ((LUNSTATE_VALID == rpwwn.m_ATLunState) && !rpwwn.m_ATLunPath.empty())
            {
                atlunnames.insert(rpwwn.m_ATLunPath);
            }
        }
    } 

    ATLunNames_t::const_iterator atiter = atlundirectpaths.begin();
    for ( /* empty */ ; atiter != atlundirectpaths.end(); atiter++)
    {
        const std::string &atname = *atiter;
        if (!atname.empty())
        {
            atlunnames.insert(atname);
        }
    }
}


SV_ULONGLONG GetCountOfIndividualATLuns(const LocalPwwnInfos_t &lpwwns)
{
    LocalPwwnInfos_t::const_iterator liter = lpwwns.begin();
    SV_ULONGLONG count = 0;

    for ( /* empty */ ; liter != lpwwns.end(); liter++)
    {
        const LocalPwwnInfo_t &lpwwn = liter->second;
        RemotePwwnInfos_t::const_iterator riter = lpwwn.m_RemotePwwnInfos.begin();
        for ( /* empty */ ; riter != lpwwn.m_RemotePwwnInfos.end(); riter++)
        {
            const RemotePwwnInfo_t &rpwwn = riter->second;
            if (LUNSTATE_NOTVISIBLE != rpwwn.m_ATLunState) // Sunil: And Cx supplied remote port.
            {
                count++;
            }
        }
    } 

    return count;
}


SV_ULONGLONG GetCountOfATLuns(const LocalPwwnInfos_t &lpwwns, const ATLunNames_t &atlundirectpaths)
{
    SV_ULONGLONG count = 0;

    count += GetCountOfIndividualATLuns(lpwwns);
    count += atlundirectpaths.size();

    return count;
}


bool AreLpwwnsCumulative(const LocalPwwnInfos_t &lpwwns)
{
    bool biscumulative = !lpwwns.empty();
    LocalPwwnInfos_t::const_iterator liter = lpwwns.begin();
    for ( /* empty */ ; liter != lpwwns.end(); liter++)
    {
        const LocalPwwnInfo_t &lpwwn = liter->second;
        if ((SCLI != lpwwn.m_PwwnSrc) &&
            (LPFC != lpwwn.m_PwwnSrc))
        {
            biscumulative = false; 
            break;
        }
    }

    return biscumulative;
}

