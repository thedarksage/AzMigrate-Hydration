#include <ace/Synch.h>
#include "npwwn.h"
#include "npwwnminor.h"
#include "localconfigurator.h"
#include "logger.h"
#include "portable.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "voldefs.h"
#include "diskpartition.h"
#include "efiinfos.h"
#include "volumemanagerfunctionsminor.h"
#include "event.h"
#include "utildirbasename.h"
#include "inmstrcmp.h"
#include "scsicmd.h"
#include <algorithm>
#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"
#include "executecommand.h"


ACE_Thread_Mutex g_DevfsAdmMutex;
ACE_Condition<ACE_Thread_Mutex> g_DevfsAdmConditionVariable(g_DevfsAdmMutex);
bool g_DevfsAdmFlag = true;

bool RunATLunDisocovery()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool retValue = true;

    g_DevfsAdmMutex.acquire();

    if (g_DevfsAdmFlag)
    {
        g_DevfsAdmFlag = false;
        g_DevfsAdmMutex.release();

        DebugPrintf(SV_LOG_DEBUG,"Running AT Lun discovery...\n");
        retValue = RunDevfsAdm();

        g_DevfsAdmMutex.acquire();
        g_DevfsAdmFlag = true;
        g_DevfsAdmConditionVariable.broadcast();
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Waiting for AT Lun discovery to complete...\n");
        g_DevfsAdmConditionVariable.wait();
    }

    g_DevfsAdmMutex.release();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return retValue;
}

void GetLocalPwwnInfos(LocalPwwnInfos_t &lpwwns)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bgotwwns = true;

    GetNPWwnFcInfo(lpwwns);
    GetNPWwnIScsiAdm(lpwwns);
    bool blookspecificadapter = lpwwns.empty();
    
    /* This has to be done since
    *  on solaris 10, if scli, emlxadm utilities 
    *  are installed, we may report duplicates
    *  On linux, we are safe since we do not
    *  use utilities and system either propagates 
    *  fchost or scsihost and the like */
    if (blookspecificadapter)
    {
        DebugPrintf(SV_LOG_DEBUG, "Getting pwwn information from specific adapter\n");
        GetNPWwnScli(lpwwns);
        GetNPWwnLpfc(lpwwns);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void GetNPWwnFcInfo(LocalPwwnInfos_t &lpwwns)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);
    const std::string cmd("/usr/sbin/fcinfo hba-port | /usr/bin/egrep \'(HBA Port WWN:|OS Device Name:|State:|Node WWN:)\'");
    std::stringstream results;
    
    /* TODO: we should remove state field since do not know what else values it can have ? 
    * -bash-3.00# /usr/sbin/fcinfo hba-port | /usr/bin/egrep '(HBA Port WWN:|OS Device Name:|State:|Node WWN:)'
    * HBA Port WWN: 210000e08b11ff23
    *         OS Device Name: /dev/cfg/c2
    *         State: online
    *         Node WWN: 200000e08b11ff23
    * HBA Port WWN: 210100e08b31ff23
    *         OS Device Name: /dev/cfg/c4
    *         State: offline
    *         Node WWN: 200100e08b31ff23
    */
    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string hbatok, porttok, wwntok, nodetok;
            LocalPwwnInfo_t lpwwn;
            /* TODO: here we need to do format conversion of wwn */
            results >> hbatok >> porttok >> wwntok >> lpwwn.m_Pwwn.m_Name;
            if (hbatok.empty())
            {
                break;
            }
            std::string ostok, devicetok, nametok, statetok;
            std::string osdevicename, state;
            results >> ostok >> devicetok >> nametok >> osdevicename;
            results >> statetok >> state;
            /* TODO: here we need to do format conversion of wwn */
            results >> nodetok >> wwntok >> lpwwn.m_Pwwn.m_Nwwn;
            /*
            DebugPrintf(SV_LOG_DEBUG, "From fcinfo, got nwwn = %s, pwwn = %s\n", 
                        lpwwn.m_Pwwn.m_Nwwn.c_str(), lpwwn.m_Pwwn.m_Name.c_str());
            */
            FormatWwn(FCINFO, lpwwn.m_Pwwn.m_Nwwn, lpwwn.m_Pwwn.m_FormattedNwwn, &lpwwn.m_Pwwn.m_Type);
            FormatWwn(FCINFO, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);
            /*
            DebugPrintf(SV_LOG_DEBUG, "From fcinfo, formatted nwwn = %s, pwwn = %s\n", 
                        lpwwn.m_Pwwn.m_FormattedNwwn.c_str(), lpwwn.m_FormattedName.c_str());
            */
 
            /* TODO: should we report when status is offline ? 
            * There are fields also from scli (HBA Status) and emlxadm if so ?
            if (FCINFOHBASTATEOFFLINE != state)
            {
            */
                if (!lpwwn.m_FormattedName.empty())
                {
                    const char *p = GetStartOfLastNumber(osdevicename.c_str());
                    if (p && (1 == sscanf(p, LLSPEC, &lpwwn.m_Pwwn.m_Number)) && 
                             (lpwwn.m_Pwwn.m_Number >= 0))
                    {
                        lpwwn.m_PwwnSrc = FCINFO;
                        DebugPrintf(SV_LOG_DEBUG, "The local pwwn %s found with controller number " LLSPEC 
                                                  " source %s\n", lpwwn.m_Pwwn.m_Name.c_str(),
                                                  lpwwn.m_Pwwn.m_Number, 
                                                  PwwnSrcStr[lpwwn.m_PwwnSrc]);
                        InsertLocalPwwn(lpwwns, lpwwn);
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "The local pwwn %s found with source %s, "
                                                  "but getting its controller number did not succeed\n",
                                                  lpwwn.m_Pwwn.m_Name.c_str(),
                                                  PwwnSrcStr[FCINFO]);
                    }
                }
            /*
            }
            */
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetNPWwnIScsiAdm(LocalPwwnInfos_t &lpwwns)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);
    const std::string cmd("/usr/sbin/iscsiadm list initiator-node | /usr/bin/egrep \'Initiator node name:\' | /usr/bin/awk \'{print $NF}\'");
    std::stringstream results;
    
    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            LocalPwwnInfo_t lpwwn;
            /* TODO: here we need to do format conversion of wwn */
            results >> lpwwn.m_Pwwn.m_Name;

            DebugPrintf(SV_LOG_DEBUG, "From iscsiadm, got pwwn = %s\n", lpwwn.m_Pwwn.m_Name.c_str());

            // FormatWwn(FCINFO, lpwwn.m_Pwwn.m_Nwwn, lpwwn.m_Pwwn.m_FormattedNwwn, &lpwwn.m_Pwwn.m_Type);
            FormatWwn(ISCSIADM, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);
            /*
            DebugPrintf(SV_LOG_DEBUG, "From fcinfo, formatted nwwn = %s, pwwn = %s\n", 
                        lpwwn.m_Pwwn.m_FormattedNwwn.c_str(), lpwwn.m_FormattedName.c_str());
            */
 
            /* TODO: should we report when status is offline ? 
            * There are fields also from scli (HBA Status) and emlxadm if so ?
            if (FCINFOHBASTATEOFFLINE != state)
            {
            */
                if (!lpwwn.m_FormattedName.empty())
                {
                        lpwwn.m_PwwnSrc = ISCSIADM;
                        InsertLocalPwwn(lpwwns, lpwwn);
                }
            /*
            }
            */
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void
FindController(LocalPwwnInfo_t &lpwwn)
{
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    std::string intermpwwn = lpwwn.m_Pwwn.m_Name;
    RemoveChar(intermpwwn, WWN_SEP_SCLI);

    // WWNTYPE pwwnnumber;
    SV_LONGLONG pwwnnumber;
    std::stringstream ss(intermpwwn);
    ss >> std::hex >> pwwnnumber;
    std::stringstream ssnumber;
    ssnumber << std::hex << pwwnnumber; // Get local-pwwn in lower case format
    std::string strwwn = ssnumber.str();

    std::string cfgadmcmd_fabric = CFGADMCMD;
    cfgadmcmd_fabric += " | /usr/bin/egrep 'fc-fabric'";
    std::stringstream results_fabric;
    if (false == executePipe(cfgadmcmd_fabric, results_fabric))
    {
        /* This is error case; but should never occur;
         * TODO: currently this will be treated as success
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                 cfgadmcmd_fabric.c_str(), msg.str().c_str());
        return;
    } else if (results_fabric.str().empty())
    {
        DebugPrintf(SV_LOG_DEBUG,"command %s has returned nothing for %s\n",
                                 cfgadmcmd_fabric.c_str(), msg.str().c_str());
        return;
    }

    while (!results_fabric.eof())
    {
        std::string Cont, type, state, config, condition;
        results_fabric >> Cont >> type >> state >> config >> condition;

        std::string cfgadmcmd_device = CFGADMCMD;
        cfgadmcmd_device += " -lv ";
        cfgadmcmd_device += Cont;
        cfgadmcmd_device += " | /usr/bin/egrep 'devices'";
        std::stringstream results_device;
        if (false != executePipe(cfgadmcmd_device, results_device) &&
              !results_device.str().empty())
        {
            std::string state, type, condition, device;
            results_device >> state >> type >> condition >> device;

            std::string luxadmcmd = LUXADMCMD;
            luxadmcmd += " -e dump_map ";
            luxadmcmd += device;
            luxadmcmd += " | /usr/bin/egrep 'Host Bus' | /usr/bin/egrep ";
            luxadmcmd += strwwn;
            std::stringstream results_luxadm;
            if (false != executePipe(luxadmcmd, results_luxadm) &&
                      !results_luxadm.str().empty())
            {
                  lpwwn.m_Pwwn.controller = Cont;
                  DebugPrintf(SV_LOG_ERROR, "Found controller %s for pwwn %s\n",
                                      lpwwn.m_Pwwn.controller.c_str(), strwwn.c_str());
                  break;
            }
            else
            {
                  DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s"
                              " or returned nothing\n",
                  luxadmcmd.c_str(), msg.str().c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s or returned nothing\n",
                               cfgadmcmd_device.c_str(), msg.str().c_str());
        }
    }
 
    return;
}

void GetNPWwnScli(LocalPwwnInfos_t &lpwwns)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);
    LocalConfigurator theLocalConfigurator;
    std::string sclipath(theLocalConfigurator.getScliPath());
    std::string cmd(sclipath);
    cmd += UNIX_PATH_SEPARATOR;
    cmd += SCLICMD;
    std::stringstream results;
    
    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            /*
            Node Name                         : 20-00-00-E0-8B-19-E0-2A
            Port Name                         : 21-00-00-E0-8B-19-E0-2A
            */
            std::string porttok, nodetok, nametok, colon;
            LocalPwwnInfo_t lpwwn;
            /* TODO: here we need to do format conversion of wwn */
            results >> nodetok >> nametok >> colon >> lpwwn.m_Pwwn.m_Nwwn;
            if (nodetok.empty())
            {
                break;
            }
            /* TODO: here we need to do format conversion of wwn */
            results >> porttok >> nametok >> colon >> lpwwn.m_Pwwn.m_Name;
            std::string intermnwwn = lpwwn.m_Pwwn.m_Nwwn;
            RemoveChar(intermnwwn, WWN_SEP_SCLI);
            std::string intermpwwn = lpwwn.m_Pwwn.m_Name;
            RemoveChar(intermpwwn, WWN_SEP_SCLI);
            FormatWwn(SCLI, intermnwwn, lpwwn.m_Pwwn.m_FormattedNwwn, &lpwwn.m_Pwwn.m_Type);
            FormatWwn(SCLI, intermpwwn, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);
            /*
            DebugPrintf(SV_LOG_DEBUG, "From scli, got nwwn = %s, pwwn = %s\n", 
                        lpwwn.m_Pwwn.m_Nwwn.c_str(), lpwwn.m_Pwwn.m_Name.c_str());
            DebugPrintf(SV_LOG_DEBUG, "From scli, formatted nwwn = %s, pwwn = %s\n", 
                        lpwwn.m_Pwwn.m_FormattedNwwn.c_str(), lpwwn.m_FormattedName.c_str());
            */
            FindController(lpwwn);
            if (lpwwn.m_Pwwn.controller.empty())
            {
                  DebugPrintf(SV_LOG_DEBUG, "Could not find controller for local pwwn %s\n",
                                            lpwwn.m_Pwwn.m_Name.c_str());
            }

            if (!lpwwn.m_FormattedName.empty())
            {
                /* No pwwn number here */
                lpwwn.m_PwwnSrc = SCLI; 
                DebugPrintf(SV_LOG_DEBUG, "The local pwwn %s found with source %s\n", 
                                          lpwwn.m_Pwwn.m_Name.c_str(),
                                          PwwnSrcStr[lpwwn.m_PwwnSrc]);
                InsertLocalPwwn(lpwwns, lpwwn);
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void GetNPWwnLpfc(LocalPwwnInfos_t &lpwwns)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);

    LocalConfigurator theLocalConfigurator;
    /* TODO: remove path and add command itself as of greg for
     * dlnkmgr ? */
    std::string emlxadmpath(theLocalConfigurator.getEmlxAdmPath());
    std::string cmd(emlxadmpath);
    cmd += UNIX_PATH_SEPARATOR;
    cmd += EMLXADMCMD;
    std::stringstream results;
    
    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            /* TODO: we need to test this for more multiport hba */
            /*
            * bash-2.05# /opt/EMLXemlxu/bin/emlxadm / -y get_host_attrs all | /usr/bin/egrep '(Node WWN|Port WWN)' | 
            *            /usr/bin/awk '{ if ($0 ~ /Port WWN/) printf("%s\n%s\n", prev, $0); prev=$0}'
            * Node WWN                   = 20000000c9340028
            * Port WWN                   = 10000000c9340028
            */
            std::string porttok, nodetok, wwntok, eqtok;
            /* TODO: here we need to do format conversion of wwn */
            LocalPwwnInfo_t lpwwn;
            results >> nodetok >> wwntok >> eqtok >> lpwwn.m_Pwwn.m_Nwwn;
            if (nodetok.empty())
            {
                break;
            }
            /* TODO: here we need to do format conversion of wwn */
            results >> porttok >> wwntok >> eqtok >> lpwwn.m_Pwwn.m_Name;
            FormatWwn(LPFC, lpwwn.m_Pwwn.m_Nwwn, lpwwn.m_Pwwn.m_FormattedNwwn, &lpwwn.m_Pwwn.m_Type);
            FormatWwn(LPFC, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);
            /*
            DebugPrintf(SV_LOG_DEBUG, "From emlxadm, got nwwn = %s, pwwn = %s\n", 
                        lpwwn.m_Pwwn.m_Nwwn.c_str(), lpwwn.m_Pwwn.m_Name.c_str());
            DebugPrintf(SV_LOG_DEBUG, "From emlxadm, formatted nwwn = %s, pwwn = %s\n", 
                        lpwwn.m_Pwwn.m_FormattedNwwn.c_str(), lpwwn.m_FormattedName.c_str());
            */
            FindController(lpwwn);
            if (lpwwn.m_Pwwn.controller.empty())
            {
                  DebugPrintf(SV_LOG_DEBUG, "Could not find controller for local pwwn %s\n",
                                            lpwwn.m_Pwwn.m_Name.c_str());
            }

            if (!lpwwn.m_FormattedName.empty())
            {
                /* no pwwn number here */
                lpwwn.m_PwwnSrc = LPFC;
                DebugPrintf(SV_LOG_DEBUG, "The local pwwn %s found with source %s\n", 
                                          lpwwn.m_Pwwn.m_Name.c_str(),
                                          PwwnSrcStr[lpwwn.m_PwwnSrc]);
                InsertLocalPwwn(lpwwns, lpwwn);
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void GetRemotePwwnsFromFcInfo(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /* -bash-3.00# fcinfo remote-port -p 210000e08b11ff23
     * Remote Port WWN: 10000000c93dcc81
     *         Active FC4 Types: SCSI,IP
     *         SCSI Target: no
     *         Node WWN: 20000000c93dcc81
     * Remote Port WWN: 50060e801045a410
     *         Active FC4 Types: SCSI
     *         SCSI Target: unknown
     *         Node WWN: 50060e801045a410
     */
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
    std::string cmd("/usr/sbin/fcinfo remote-port -p");
    cmd += " ";
    /* TODO: here we need to do format conversion of wwn */
    cmd += pLocalPwwnInfo->m_Pwwn.m_Name;
    cmd += " | /usr/bin/grep \'Remote Port WWN:\'";
    std::stringstream results;

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string remote, port, wwn, rport;
            results >> remote >> port >> wwn >> rport;
            if (remote.empty())
            {
                break;
            }
            if (!rport.empty())
            {
                RemotePwwnInfo_t rpwwn;
                FormatWwn(FCINFO, rport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                if (!rpwwn.m_FormattedName.empty())
                {
                    rpwwn.m_Pwwn.m_Name = rport;
                    InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, rpwwn);
                }
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
}

void GetRemotePwwnsFromIScsiAdm(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
    /* in Solaris, listing target per initiator is not possible with iscsiadm command.
     * so all the target are shown as remote ports for the initiator ports
     */
    std::string cmd("/usr/sbin/iscsiadm list target | /usr/bin/grep \'Target:\' | /usr/bin/awk \'{print $NF}\'");
    std::stringstream results;

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string rport;
            results >> rport;

            if (!rport.empty())
            {
                RemotePwwnInfo_t rpwwn;
                FormatWwn(ISCSIADM, rport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                if (!rpwwn.m_FormattedName.empty())
                {
                    rpwwn.m_Pwwn.m_Name = rport;
                    InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, rpwwn);
                }
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
}

void GetRemotePwwnsFromScli(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /*
    * root@solaris154:/>scli -t 21-00-00-E0-8B-19-E0-2A
    * --------------------------------------------------------------------------------
    * HBA/OS Instance 0/0: QLA2340 Port 1 WWPN 21-00-00-E0-8B-19-E0-2A PortID 01-0D-00
    * --------------------------------------------------------------------------------
    * Path                              : 0
    * Target                            : 1
    * Device ID                         : 0x00
    * Product Vendor                    : InMageDR
    * Product ID                        : DUMMY_LUN_ZERO
    * Product Revision                  : 0001
    * Serial Number                     :
    * Node Name                         : 20-00-00-E0-8B-11-F3-30
    * Port Name                         : 21-00-00-E0-8B-11-F3-30
    * Port ID                           : 01-09-00
    * Product Type                      : Disk
    * LUN Count(s)                      : 5
    * OS Target Name                    : /dev/dsk/c0t1d0
    * Status                            : Online
    * ------------------------------------------------------------
    * Path                              : 0
    * Target                            : 0
    * Device ID                         : 0x01
    * Product Vendor                    : Pillar
    * Product ID                        : Axiom 500
    * Product Revision                  : 0000
    * Serial Number                     : 000B080569002158
    * Node Name                         : 20-01-00-0B-08-00-21-58
    * Port Name                         : 23-00-00-0B-08-04-42-C0
    * Port ID                           : 01-07-00
    * Product Type                      : Disk
    * LUN Count(s)                      : 5
    * OS Target Name                    : /dev/dsk/c0t0d0
    * Status                            : Online
    * ------------------------------------------------------------
    */

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
    LocalConfigurator theLocalConfigurator;
    std::string sclipath(theLocalConfigurator.getScliPath());
    std::string cmd(sclipath);
    cmd += UNIX_PATH_SEPARATOR;
    cmd += SCLICMDNAME;

    /* first scan is needed even to see remote ports */
    std::string scancmd = cmd;
    scancmd += " ";
    scancmd += SCLISCANOPT;
    scancmd += " ";
    scancmd += pLocalPwwnInfo->m_Pwwn.m_Name;
    scancmd += " ";
    scancmd += SCLISCANOPTSUFFIX;
    
    std::stringstream rportresults;
    bool bexecd = executePipe(scancmd, rportresults);
    (qf && qf(NSECS_TO_WAIT_AFTER_SCAN));
    bool bneedtoquit = (qf && qf(0));
    if (bexecd && !bneedtoquit)
    {
        std::string rportcmd = cmd;
        rportcmd += " ";
        rportcmd += SCLIRPORTOPT;
        rportcmd += " ";
        /* TODO: here we need to do format conversion of wwn */
        rportcmd += pLocalPwwnInfo->m_Pwwn.m_Name;
        rportcmd += " | /usr/bin/grep \'Port Name\'";
        std::stringstream results;
    
        if (executePipe(rportcmd, results))
        {
            while (!results.eof())
            {
                std::string port, name, colon, rport;
                results >> port >> name >> colon >> rport;
                if (port.empty())
                {
                    break;
                }
                if (!rport.empty())
                {
                    RemotePwwnInfo_t rpwwn;
                    std::string formattedrport = rport;
                    RemoveChar(formattedrport, WWN_SEP_SCLI);
                    FormatWwn(SCLI, formattedrport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                    if (!rpwwn.m_FormattedName.empty())
                    {
                        rpwwn.m_Pwwn.m_Name = rport;
                        InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, rpwwn);
                    }
                }
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
}


void GetRemotePwwnsFromLpfc(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /* bash-2.05# /opt/EMLXemlxu/bin/emlxadm / -y get_port_attrs all | egrep '(Host Port:|Port WWN)'
     * Host Port:
     *     Port WWN                   = 10000000c9340028
     *     Port WWN                   = 10000000c9341544
     *     Port WWN                   = 10000000c9308487
     */
    /* TODO: Assumes that Host Port: repeats
     *       in both cases of more than one 
     *       port in same HBA (or) more than
     *       one HBA
     * leave the first port within it is local; 
     * match this and upto next Host Port: token,
     * all are remote ports of this local port */
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
    /* TODO: For every local pwwn, we run command 
     * for all ports and every time. This is BUG
     * and should be fixed */
    LocalConfigurator theLocalConfigurator;
    /* TODO: remove path and add command itself as of greg for
    * dlnkmgr ? */
    std::string emlxadmpath(theLocalConfigurator.getEmlxAdmPath());
    std::string cmd(emlxadmpath);
    cmd += UNIX_PATH_SEPARATOR;
    cmd += EMLXADMCMDFORRPORTS;
    std::stringstream results;
    
    if (false == executePipe(cmd, results))
    {
        return;
    }

    if (results.str().empty())
    {
        return;
    }
    const std::string HOSTPORTTOKEN = "Host Port:";

    size_t currentStartPos = 0;
    size_t offset = 0;
    while ((std::string::npos != currentStartPos) && ((currentStartPos + offset) < results.str().length()))
    {
        size_t nextStartPos = results.str().find(HOSTPORTTOKEN, currentStartPos + offset);
        if (0 == offset)
        {
            offset = HOSTPORTTOKEN.size();
        }
        else
        {
            size_t hostportstreamlen = (std::string::npos == nextStartPos) ? std::string::npos : (nextStartPos - currentStartPos);
            std::string hostpwwninfo = results.str().substr(currentStartPos, hostportstreamlen);
            bool bisrportscollected = CollectRemotePortsForLpfcPwwn(hostpwwninfo, pLocalPwwnInfo); 
            if (bisrportscollected)
            {
                break;
            }
        }
        currentStartPos = nextStartPos;
    }
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
}


bool CollectRemotePortsForLpfcPwwn(const std::string &hostpwwninfo, LocalPwwnInfo_t *pLocalPwwnInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());

    bool biscollected = false;
    std::stringstream hostportstream(hostpwwninfo);
    std::string hostportline;
    std::getline(hostportstream, hostportline);
    if (!hostportline.empty())
    {
        std::string localportline;
        std::getline(hostportstream, localportline);
        if (!localportline.empty())
        {
            std::string port, wwn, equal, lport;
            std::stringstream sslocalport(localportline);
            sslocalport >> port >> wwn >> equal >> lport;
            if (!port.empty())
            {
                if (!lport.empty() && (lport == pLocalPwwnInfo->m_Pwwn.m_Name))
                {
                    while (!hostportstream.eof())
                    {
                        std::string port, wwn, equal, rport;
                        hostportstream >> port >> wwn >> equal >> rport;
                        if (port.empty())
                        {
                            break;
                        }
                        if (!rport.empty())
                        {
                            RemotePwwnInfo_t rpwwn;
                            FormatWwn(LPFC, rport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                            if (!rpwwn.m_FormattedName.empty())
                            {
                                rpwwn.m_Pwwn.m_Name = rport;
                                InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, rpwwn);
                            }
                        }
                    }
                    /* true if only matches; 
                    *  regardless of rpwwns 
                    *  present or not ? */
                    biscollected = true;
                }
            }
        }
    }
             
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
    return biscollected;
}


void GetRemotePwwnsFromFcHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /*
    * Should not come here
    */
}


void GetRemotePwwnsFromScsiHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /*
    * Should not come here
    */
}


void GetRemotePwwnsFromQla(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /*
    * Should not come here
    */
}

void GetChannelNumbers(Channels_t &ChannelNumbers)
{
    /* No implementation here */
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
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    if (bneedtoquit)
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is signalled before getting "
                                  "AT Lun path for %s\n", msg.str().c_str());
        return;
    }

    /* For PI - AT pair do "cfgadm -f -c configure c#:<PWWN> if needed */
    std::string intermpwwn = rpwwn.m_Pwwn.m_Name;
    RemoveChar(intermpwwn, WWN_SEP_SCLI);

    // WWNTYPE pwwnnumber;
    SV_LONGLONG pwwnnumber;
    std::stringstream ss(intermpwwn);
    ss >> std::hex >> pwwnnumber;
    std::stringstream ssnumber;
    ssnumber << std::hex << pwwnnumber; // Get remote-pwwn in lower case format
    std::string strwwn = ssnumber.str();

    std::string cfgadmcmd_al = CFGADMCMD;
    cfgadmcmd_al += " -al | /usr/bin/egrep ";
    std::string Controller_AT = lpwwn.m_Pwwn.controller;
    Controller_AT += CFGADMCONTROLLERATSEP;
    Controller_AT += strwwn;
    cfgadmcmd_al += Controller_AT;

    DebugPrintf(SV_LOG_DEBUG, "Controller, Remote-Pwwn to be used for cfgadm %s\n",
                              Controller_AT.c_str());

    std::stringstream results_al;
    if (false == executePipe(cfgadmcmd_al, results_al))
    {
        /* This is error case; but should never occur;
         * TODO: currently this will be treated as success
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                 cfgadmcmd_al.c_str(), msg.str().c_str());
        return;
    } else if (results_al.str().empty())
    {
        DebugPrintf(SV_LOG_DEBUG,"command %s has returned nothing for %s\n",
                                 cfgadmcmd_al.c_str(), msg.str().c_str());
        return;
    }

    std::string ContAT, type, state, config, condition;
    results_al >> ContAT >> type >> state >> config >> condition;
    if (type != "disk" || state != "connected" ||
            config != "configured" || condition != "unknown")
    {
            DebugPrintf(SV_LOG_DEBUG, "Need to run cfgadm -c unconfigure"
                      " -o unusable_FCP_dev <ContAT>\n");
            DebugPrintf(SV_LOG_DEBUG, "Need to run cfgadm -f -c configure"
                      " <ContAT>\n");
            DebugPrintf(SV_LOG_DEBUG, "ContAT: %s, type: %s, state: %s,"
                      " config: %s, condition: %s\n",
                      ContAT.c_str(), type.c_str(), state.c_str(), config.c_str(), condition.c_str());

            std::string uncfgadmcmd = CFGADMCMD;
            uncfgadmcmd += " -c unconfigure -o unusable_FCP_dev ";
            uncfgadmcmd += ContAT;
            std::stringstream results_unconfig;
            if (false == executePipe(uncfgadmcmd, results_unconfig))
            {
                    /* This is error case; but should never occur;
                     * TODO: currently this will be treated as success
                     * change it to return error */
                    DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                     uncfgadmcmd.c_str(), msg.str().c_str());
                    return;
            }

            std::string cfgadmcmd_config = CFGADMCMD;
            cfgadmcmd_config += " -f -c configure ";
            cfgadmcmd_config += ContAT;
            std::stringstream results_config;
            if (false == executePipe(cfgadmcmd_config, results_config))
            {
                    /* This is error case; but should never occur;
                     * TODO: currently this will be treated as success
                     * change it to return error */
                    DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                     cfgadmcmd_config.c_str(), msg.str().c_str());
                    return;
            }
    }

    if (0 == piatluninfo.applianceTargetLUNNumber)
    {
        bool Val = false;
        Val = DummyLunInCfgAdmOld(lpwwn, rpwwn, &piatluninfo.applianceTargetLUNNumber);
        if (Val == true)
        {
              DebugPrintf(SV_LOG_DEBUG, "For %s, found matching AT Lun device for %s."
                                " Cannot find lun/disk name (target number) using emlxadm"
                                " in sol9, sol8\n",
                                msg.str().c_str(), piatluninfo.applianceTargetLUNName.c_str());
              rpwwn.m_ATLunState = LUNSTATE_VALID;
              rpwwn.m_ATLunPath = "/dev/dsk/";
              rpwwn.m_ATLunPath += lpwwn.m_Pwwn.controller;
              rpwwn.m_ATLunPath += "t#d0s2";
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}

void GetATLunPathFromScli(const PIAT_LUN_INFO &piatluninfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn)
{
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    if (bneedtoquit)
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is signalled before getting "
                                  "AT Lun path for %s\n", msg.str().c_str());
        return;
    }

    /* For PI - AT pair do "cfgadm -f -c configure c#:<PWWN> if needed */
    std::string intermpwwn = rpwwn.m_Pwwn.m_Name;
    RemoveChar(intermpwwn, WWN_SEP_SCLI);

    // WWNTYPE pwwnnumber;
    SV_LONGLONG pwwnnumber;
    std::stringstream ss(intermpwwn);
    ss >> std::hex >> pwwnnumber;
    std::stringstream ssnumber;
    ssnumber << std::hex << pwwnnumber; // Get remote-pwwn in lower case format
    std::string strwwn = ssnumber.str();

    std::string cfgadmcmd_al = CFGADMCMD;
    cfgadmcmd_al += " -al | /usr/bin/egrep ";
    std::string Controller_AT = lpwwn.m_Pwwn.controller;
    Controller_AT += CFGADMCONTROLLERATSEP;
    Controller_AT += strwwn;
    cfgadmcmd_al += Controller_AT;

    DebugPrintf(SV_LOG_DEBUG, "Controller, Remote-Pwwn to be used for cfgadm %s\n",
                              Controller_AT.c_str());

    std::stringstream results_al;
    if (false == executePipe(cfgadmcmd_al, results_al))
    {
        /* This is error case; but should never occur;
         * TODO: currently this will be treated as success
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                 cfgadmcmd_al.c_str(), msg.str().c_str());
        return;
    } else if (results_al.str().empty())
    {
        DebugPrintf(SV_LOG_DEBUG,"command %s has returned nothing for %s\n",
                                 cfgadmcmd_al.c_str(), msg.str().c_str());
        return;
    }

    std::string ContAT, type, state, config, condition;
    results_al >> ContAT >> type >> state >> config >> condition;
    if (type != "disk" || state != "connected" ||
            config != "configured" || condition != "unknown")
    {
          DebugPrintf(SV_LOG_DEBUG, "Need to run cfgadm -c unconfigure"
                      " -o unusable_FCP_dev <ContAT>\n");
          DebugPrintf(SV_LOG_DEBUG, "Need to run cfgadm -f -c configure"
                      " <ContAT>\n");
          DebugPrintf(SV_LOG_DEBUG, "ContAT: %s, type: %s, state: %s,"
                      " config: %s, condition: %s\n",
                      ContAT.c_str(), type.c_str(), state.c_str(), config.c_str(), condition.c_str());

          std::string uncfgadmcmd = CFGADMCMD;
          uncfgadmcmd += " -c unconfigure -o unusable_FCP_dev ";
          uncfgadmcmd += ContAT;
          std::stringstream results_unconfig;
          if (false == executePipe(uncfgadmcmd, results_unconfig))
          {
                  /* This is error case; but should never occur;
                   * TODO: currently this will be treated as success
                   * change it to return error */
                  DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                   uncfgadmcmd.c_str(), msg.str().c_str());
                  return;
          }

          std::string cfgadmcmd_config = CFGADMCMD;
          cfgadmcmd_config += " -f -c configure ";
          cfgadmcmd_config += ContAT;
          std::stringstream results_config;
          if (false == executePipe(cfgadmcmd_config, results_config))
          {
                  /* This is error case; but should never occur;
                   * TODO: currently this will be treated as success
                   * change it to return error */
                  DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                   cfgadmcmd_config.c_str(), msg.str().c_str());
                  return;
          }
    }

    if (0 == piatluninfo.applianceTargetLUNNumber)
    {
        bool Val = false;
        Val = DummyLunInCfgAdmOld(lpwwn, rpwwn, &piatluninfo.applianceTargetLUNNumber);
        // Val = IsATDummyLunVisibleInScli(piatluninfo, lpwwn, rpwwn);
        if (Val == true)
        {
              DebugPrintf(SV_LOG_DEBUG, "For %s, found matching AT Lun device for %s."
                                " Cannot find lun/disk name (target number) using scli"
                                " in sol9, sol8\n",
                                msg.str().c_str(), piatluninfo.applianceTargetLUNName.c_str());
              rpwwn.m_ATLunState = LUNSTATE_VALID;
              rpwwn.m_ATLunPath = "/dev/dsk/";
              rpwwn.m_ATLunPath += lpwwn.m_Pwwn.controller;
              rpwwn.m_ATLunPath += "t#d0s2";
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}

bool
IsATDummyLunVisibleInScli(
      const PIAT_LUN_INFO &piatluninfo,
      const LocalPwwnInfo_t &lpwwn,
      const RemotePwwnInfo_t &rpwwn)
{
    bool bisvisible = false;
    std::stringstream msg;
    SV_ULONGLONG lunnumber = piatluninfo.applianceTargetLUNNumber;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    LocalConfigurator theLocalConfigurator;
    std::string sclipath(theLocalConfigurator.getScliPath());
    std::string cmd(sclipath);
    cmd += UNIX_PATH_SEPARATOR;
    cmd += SCLICMDNAME;
    cmd += " ";
    cmd += SCLILUNOPT;
    cmd += " ";
    /* TODO: format of pwwn */
    cmd += lpwwn.m_Pwwn.m_Name;
    cmd += " ";
    cmd += rpwwn.m_Pwwn.m_Name;
    cmd += " ";
    std::stringstream sslunnumber;
    sslunnumber << lunnumber;
    cmd += sslunnumber.str();
    cmd += " | /usr/bin/egrep ";
    cmd += piatluninfo.applianceTargetLUNName;

    std::stringstream results;
    if (false == executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s for %s\n", cmd.c_str(),
                                  msg.str().c_str());
        return bisvisible;
    }

    if (results.str().empty())
    {
        DebugPrintf(SV_LOG_WARNING, "command %s for %s returned no output\n", cmd.c_str(),
                                    msg.str().c_str());
    } else
    {
        DebugPrintf(SV_LOG_ERROR, "For %s, lun number " ULLSPEC
                                  ", AT Lun device (%s) is visible in scli -l\n",
                                  msg.str().c_str(),
                                  piatluninfo.applianceTargetLUNNumber,
                                piatluninfo.applianceTargetLUNName.c_str());
        bisvisible = true;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
    return bisvisible;
}

/* This is for Sol 9 and Sol 8 */
bool
DummyLunInCfgAdmOld(const LocalPwwnInfo_t &lpwwn,
                    const RemotePwwnInfo_t &rpwwn,
                    const SV_ULONGLONG *patlunnumber)
{
    /* bash-2.05# cfgadm -al -o show_FCP_dev c2::210000e08b9d27e1,0
     * Ap_Id                          Type         Receptacle   Occupant     Condition
     * c2::210000e08b9d27e1,0         disk         connected    configured   unknown
     */

    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", Lun Number: " << *patlunnumber;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    std::string intermpwwn = rpwwn.m_Pwwn.m_Name;
    RemoveChar(intermpwwn, WWN_SEP_SCLI);
    // WWNTYPE pwwnnumber;
    SV_LONGLONG pwwnnumber;
    std::stringstream ss(intermpwwn);
    ss >> std::hex >> pwwnnumber;
    std::stringstream ssnumber;
    ssnumber << std::hex << pwwnnumber; // Get remote-pwwn in lower case format
    std::string strwwn = ssnumber.str();

    std::string state;
    std::string controller;
    controller += lpwwn.m_Pwwn.controller;
    std::string controlleratlun = controller;
    controlleratlun += CFGADMCONTROLLERATSEP;
    controlleratlun += strwwn;
    controlleratlun += CFGADMCONTROLLERATLUNSEP;
    std::stringstream sslunnumber;
    sslunnumber << *patlunnumber;
    controlleratlun += sslunnumber.str();

    std::string cfgadmcmd = CFGADMCMD;
    std::string args = FormCfgAdmArgs("aloz", SHOWFCPDEVARG, controlleratlun.c_str());
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm show scsi lun command for %s\n",
                                  msg.str().c_str());
    }
    else
    {
        cfgadmcmd += args;
        cfgadmcmd += " | /usr/bin/egrep \'disk\' | /usr/bin/egrep \'connected\' | /usr/bin/egrep \'configured\' | /usr/bin/egrep \'unknown\'";
    }

    std::stringstream results;
    if (false == executePipe(cfgadmcmd, results))
    {
        /* This is error case; but should never occur;
         * TODO: currently this will be treated as success
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                 cfgadmcmd.c_str(), msg.str().c_str());
        return false;
    }
    else if (results.str().empty())
    {
        DebugPrintf(SV_LOG_DEBUG,"command %s has returned nothing for %s\n",
                                 cfgadmcmd.c_str(), msg.str().c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s - DummyLunZ\n",
                             FUNCTION_NAME, msg.str().c_str());
    return true;
}

void GetATLunPathFromFcInfo(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            const bool &bshouldscan,
                            Channels_t &channelnumbers,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn)
{
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    if (!bneedtoquit)
    {
        /* This also has to match the product and vendor */
        UpdateFcInfoATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, rpwwn);
        DebugPrintf(SV_LOG_DEBUG, "For %s, the at lun state is %s\n", 
                                  msg.str().c_str(), 
                                  LunStateStr[rpwwn.m_ATLunState]);
        if (LUNSTATE_VALID == rpwwn.m_ATLunState)
        {
            DebugPrintf(SV_LOG_DEBUG, "for %s, the at lun is visible without any scan\n", msg.str().c_str());
        }
        else if (bshouldscan)
        {
            /* TODO: should this be error because on solaris every thing is supposed
             * to be automatic? currently recording as error; */
            DebugPrintf(SV_LOG_ERROR, "for %s, the at lun is not visible. hence scanning\n", msg.str().c_str());
            ScanFcInfoATLun(piatluninfo, lpwwn, qf, rpwwn);
            bneedtoquit = (qf && qf(0));
            if (!bneedtoquit)
            {
                if (LUNSTATE_VALID == rpwwn.m_ATLunState)
                {
                    DebugPrintf(SV_LOG_DEBUG, "for %s, AT Lun is visible after fcinfo scan\n", 
                                              msg.str().c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "for %s, the AT Lun is not visible after fcinfo scan\n",
                                              msg.str().c_str());
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "quit is signalled while doing fcinfo scan "
                                          "AT Lun path for %s\n", msg.str().c_str());
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is signalled before getting "
                                  "AT Lun path for %s\n", msg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}

void GetATLunPathFromIScsiAdm(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            const bool &bshouldscan,
                            Channels_t &channelnumbers,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn)
{
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID
        << ", AT Lun Number: " << piatluninfo.applianceTargetLUNNumber
        << ", AT Lun Name: " << piatluninfo.applianceTargetLUNName;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    rpwwn.m_ATLunState = LUNSTATE_NOTVISIBLE;

    if (!bneedtoquit)
    {
        std::string cmd("/usr/sbin/iscsiadm list target -S ");
        cmd += rpwwn.m_Pwwn.m_Name;
        cmd += " | /usr/bin/egrep \'LUN: | Vendor: | Product: | OS Device Name:\'";
        // cmd += " | /usr/bin/awk -F: \'{print $2}\'";
        // cmd += " | /usr/bin/sed \'s/ //g\'";

        std::stringstream results;

        if (executePipe(cmd, results))
        {
            while (!results.eof())
            {
                std::string keyword, line1, line2, line3, line4;
                results >> keyword >> line1;
                results >> keyword >> line2;
                results >> keyword >> line3;
                results >> keyword >> keyword >> keyword >> line4;
                DebugPrintf(SV_LOG_DEBUG," line1 %s line2 %s line3 %s line4 %s\n", line1.c_str(), line2.c_str(), line3.c_str(), line4.c_str());

                if (!line3.empty() && (line3 == piatluninfo.applianceTargetLUNName))
                {
                    std::stringstream lunnumber;
                    lunnumber << piatluninfo.applianceTargetLUNNumber;
                    if (line1 == lunnumber.str())
                    {
                        std::string rdsk = "/rdsk/";
                        std::string disk;
                        disk = line4;
                        DebugPrintf(SV_LOG_DEBUG," rdsk %s disk %s\n", rdsk.c_str(), disk.c_str());
                        disk.replace(disk.find(rdsk), rdsk.length(), "/dsk/");
                        DebugPrintf(SV_LOG_DEBUG," disk %s rdisk %s\n", rdsk.c_str(), disk.c_str());
                        rpwwn.m_ATLunPath = disk;
                        rpwwn.m_ATLunState = LUNSTATE_VALID;
                    }
                    else
                        rpwwn.m_ATLunState = LUNSTATE_NEEDSCLEANUP;
                }
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is signalled before getting "
                                  "AT Lun path for %s\n", msg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
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
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());

    if (LUNSTATE_NOTVISIBLE == rpwwn.m_ATLunState)
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, AT Lun is not visible. "
                                  "Hence considering deletion successful, " 
                                  "but still running deletion commands\n",
                                  msg.str().c_str());
    }
    else
    {
        /* recording as error since this has to be automatic */
        DebugPrintf(SV_LOG_ERROR, "For %s, AT Lun is visible."
                                  "Hence deleting it\n", msg.str().c_str());
    }

    if (!bneedtoquit)
    {
        /* even if lun not visible, cleanup but dont check */
        DebugPrintf(SV_LOG_DEBUG, "For %s, cleaning up AT Lun device\n", 
                                  msg.str().c_str());
        bool bcleaned = IsATLunCleanedUpFromFcInfo(piatluninfo, lpwwn, qf, rpwwn);
        bneedtoquit = (qf && qf(0));

        if (!bneedtoquit)
        {
            if (bcleaned)
            {
                DebugPrintf(SV_LOG_DEBUG, "for %s, AT Lun cleanup succeeded\n", msg.str().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "for %s, AT Lun cleanup failed\n", msg.str().c_str());
            }
            /* TODO: if already lun was not visible, 
             * then, after cleanup, do we need to
             * check if becomes visible ? currently 
             * not checking this */
            if (LUNSTATE_NOTVISIBLE != rpwwn.m_ATLunState) // Sunil: After cleanup did u update this?
            {
                UpdateFcInfoATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, rpwwn);
                if (LUNSTATE_NOTVISIBLE != rpwwn.m_ATLunState)
                {
                    DebugPrintf(SV_LOG_ERROR, "For %s, AT Lun is visible even after cleanup.\n",
                                              msg.str().c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_DEBUG, "For %s, AT Lun is not visible after cleanup. "
                                              "Hence considering deletion successful.\n",
                                              msg.str().c_str());
                }
            }
        }
    }

    if (bneedtoquit)
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is signalled while doing fcinfo scan "
                                  "AT Lun path for %s\n", msg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());
}


void DeleteATLunFromScli(const PIAT_LUN_INFO &piatluninfo, 
                         const LocalPwwnInfo_t &lpwwn, 
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    if (bneedtoquit)
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is signalled before getting "
                                  "AT Lun path for %s\n", msg.str().c_str());
        return;
    }

    /* For PI - AT pair do "cfgadm -c unconfigure -o unusable_FCP_dev c#:<PWWN> */
    std::string intermpwwn = rpwwn.m_Pwwn.m_Name;
    RemoveChar(intermpwwn, WWN_SEP_SCLI);

    // WWNTYPE pwwnnumber;
    SV_LONGLONG pwwnnumber;
    std::stringstream ss(intermpwwn);
    ss >> std::hex >> pwwnnumber;
    std::stringstream ssnumber;
    ssnumber << std::hex << pwwnnumber; // Get remote pwwn in lower case format
    std::string strwwn = ssnumber.str();

    std::string cfgadmcmd_al = CFGADMCMD;
    cfgadmcmd_al += " -al | /usr/bin/egrep ";
    std::string Controller_AT = lpwwn.m_Pwwn.controller;
    Controller_AT += CFGADMCONTROLLERATSEP;
    Controller_AT += strwwn;
    cfgadmcmd_al += Controller_AT;

    DebugPrintf(SV_LOG_DEBUG, "Controller, Remote-Pwwn to be used for cfgadm %s\n",
                                Controller_AT.c_str());

    std::stringstream results_al;
    if (false == executePipe(cfgadmcmd_al, results_al))
    {
        /* This is error case; but should never occur;
         * TODO: currently this will be treated as success
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                 cfgadmcmd_al.c_str(), msg.str().c_str());
        return;
    } else if (results_al.str().empty())
    {
        DebugPrintf(SV_LOG_DEBUG,"command %s has returned nothing for %s\n",
                                 cfgadmcmd_al.c_str(), msg.str().c_str());
        return;
    }

    std::string ContAT, type, state, config, condition;
    results_al >> ContAT >> type >> state >> config >> condition;
    DebugPrintf(SV_LOG_DEBUG, "Run cfgadm -c unconfigure"
                " -o unusable_FCP_dev <ContAT>\n");
    DebugPrintf(SV_LOG_DEBUG, "ContAT: %s, type: %s, state: %s,"
                " config: %s, condition: %s\n",
                ContAT.c_str(), type.c_str(), state.c_str(), config.c_str(), condition.c_str());

    std::string uncfgadmcmd = CFGADMCMD;
    uncfgadmcmd += " -c unconfigure -o unusable_FCP_dev ";
    uncfgadmcmd += ContAT;
    std::stringstream results_unconfig;
    if (false == executePipe(uncfgadmcmd, results_unconfig))
    {
        /* This is error case; but should never occur;
         * TODO: currently this will be treated as success
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                 uncfgadmcmd.c_str(), msg.str().c_str());
        return;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());

    return;
}


void DeleteATLunFromLpfc(const PIAT_LUN_INFO &piatluninfo, 
                         const LocalPwwnInfo_t &lpwwn, 
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    if (bneedtoquit)
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is signalled before getting "
                                  "AT Lun path for %s\n", msg.str().c_str());
        return;
    }

    /* For PI - AT pair do "cfgadm -c unconfigure -o unusable_FCP_dev c#:<PWWN> */
    std::string intermpwwn = rpwwn.m_Pwwn.m_Name;
    RemoveChar(intermpwwn, WWN_SEP_SCLI);

    // WWNTYPE pwwnnumber;
    SV_LONGLONG pwwnnumber;
    std::stringstream ss(intermpwwn);
    ss >> std::hex >> pwwnnumber;
    std::stringstream ssnumber;
    ssnumber << std::hex << pwwnnumber; // Get remote pwwn in lower case format
    std::string strwwn = ssnumber.str();

    std::string cfgadmcmd_al = CFGADMCMD;
    cfgadmcmd_al += " -al | /usr/bin/egrep ";
    std::string Controller_AT = lpwwn.m_Pwwn.controller;
    Controller_AT += CFGADMCONTROLLERATSEP;
    Controller_AT += strwwn;
    cfgadmcmd_al += Controller_AT;

    DebugPrintf(SV_LOG_DEBUG, "Controller, Remote-Pwwn to be used for cfgadm %s\n",
                                Controller_AT.c_str());

    std::stringstream results_al;
    if (false == executePipe(cfgadmcmd_al, results_al))
    {
        /* This is error case; but should never occur;
         * TODO: currently this will be treated as success
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                 cfgadmcmd_al.c_str(), msg.str().c_str());
        return;
    } else if (results_al.str().empty())
    {
        DebugPrintf(SV_LOG_DEBUG,"command %s has returned nothing for %s\n",
                                 cfgadmcmd_al.c_str(), msg.str().c_str());
        return;
    }

    std::string ContAT, type, state, config, condition;
    results_al >> ContAT >> type >> state >> config >> condition;
    DebugPrintf(SV_LOG_DEBUG, "Run cfgadm -c unconfigure"
                " -o unusable_FCP_dev <ContAT>\n");
    DebugPrintf(SV_LOG_DEBUG, "ContAT: %s, type: %s, state: %s,"
                " config: %s, condition: %s\n",
                ContAT.c_str(), type.c_str(), state.c_str(), config.c_str(), condition.c_str());

    std::string uncfgadmcmd = CFGADMCMD;
    uncfgadmcmd += " -c unconfigure -o unusable_FCP_dev ";
    uncfgadmcmd += ContAT;
    std::stringstream results_unconfig;
    if (false == executePipe(uncfgadmcmd, results_unconfig))
    {
        /* This is error case; but should never occur;
         * TODO: currently this will be treated as success
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                 uncfgadmcmd.c_str(), msg.str().c_str());
        return;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());

    return;
}

void DeleteATLunFromIScsiAdm(const PIAT_LUN_INFO &piatluninfo, 
                           const LocalPwwnInfo_t &lpwwn, 
                           Channels_t &channels,
                           QuitFunction_t &qf,
                           RemotePwwnInfo_t &rpwwn)
{
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());

    if (LUNSTATE_NOTVISIBLE == rpwwn.m_ATLunState)
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, AT Lun is not visible. "
                                  "Hence considering deletion successful, " 
                                  "but still running deletion commands\n",
                                  msg.str().c_str());
    }
    else
    {
        /* recording as error since this has to be automatic */
        DebugPrintf(SV_LOG_ERROR, "For %s, AT Lun is visible."
                                  "Hence deleting it\n", msg.str().c_str());
        /* TODO */
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());
}

void UpdateFcInfoATLunState(const PIAT_LUN_INFO &piatluninfo,
                            const std::string &vendor, 
                            const LocalPwwnInfo_t &lpwwn,
                            RemotePwwnInfo_t &rpwwn)
{
    bool IsDummyLun = false;
    E_LUNMATCHSTATE lunmatchstate = LUN_NOVENDORDETAILS;
    std::stringstream msg;
    msg << "source LUN ID : " << piatluninfo.sourceLUNID << ','
        << " PIWWN : " << lpwwn.m_Pwwn.m_Name << ','
        << " ATWWN : " << rpwwn.m_Pwwn.m_Name << ','
        << " LUN Number : " << piatluninfo.applianceTargetLUNNumber << '\n';
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());

    rpwwn.m_ATLunState = LUNSTATE_NOTVISIBLE;
    std::string dir = OSNAMESPACEFORDEVICES;
    std::string onlydisk = GetFcInfoOnlyDiskName(dir, lpwwn, rpwwn, 
                                                &piatluninfo.applianceTargetLUNNumber);
    /* no need of sleep on
     * quit event since blocking; 
     * works while deleion too since disk 
     * should not be visible */
    bool brunneddevfsadm = RunATLunDisocovery();
    if (false == brunneddevfsadm)
    {
        DebugPrintf(SV_LOG_ERROR, "failed to run devfsadm for %s when " 
                                  " getting AT Lun name from fcinfo\n", msg.str().c_str());
    }

    /* This can be p0, s2, s[0-16], s7, d<n> etc ... */
    std::string disk = FindDiskEntryFromDiskName(piatluninfo, dir, onlydisk); 
    if (!disk.empty())
    {
        if (piatluninfo.applianceTargetLUNNumber == 0)
        {
            IsDummyLun = DummyLunInCfgAdm(lpwwn, rpwwn, &piatluninfo.applianceTargetLUNNumber);
            if (IsDummyLun == true)
            {
                lunmatchstate = LUN_NAME_MATCHED;
            }
        }
        else
        {
                lunmatchstate = GetLunMatchStateForDisk(disk, vendor,
                                                                piatluninfo.applianceTargetLUNName,
                                                                piatluninfo.applianceTargetLUNNumber);
        }

        if (LUN_NAME_MATCHED == lunmatchstate)
        {
            DebugPrintf(SV_LOG_DEBUG, "For %s, found matching AT Lun device %s\n", 
                                      msg.str().c_str(), disk.c_str());
            rpwwn.m_ATLunState = LUNSTATE_VALID;
            rpwwn.m_ATLunPath = disk;
        }
        else
        {
            /* This has to be error because FindDiskEntryFromDiskName opens, 
             * then only disk can be non empty here */
            DebugPrintf(SV_LOG_ERROR, "The AT Lun only disk name %s state is %s for %s\n",
                                      onlydisk.c_str(), LunMatchStateStr[lunmatchstate], 
                                      msg.str().c_str());
        }
    }
    else
    {
        /* This has to be warning because before even 
         * we come here, the lun should have been created.
         * but what when we are checking for deletion being
         * successful ? Hence recording it as debug */
        DebugPrintf(SV_LOG_DEBUG, "The AT Lun only disk name %s is not created for "
                                    " %s\n", onlydisk.c_str(), msg.str().c_str());
    }
 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}


std::string FindDiskEntryFromDiskName(const PIAT_LUN_INFO &piatluninfo, const std::string &dir, const std::string &onlydiskname)
{
    std::string disk;
    const std::string FULLDISKSLC = "s2";

    std::string s2fulldiskname = BaseName(onlydiskname);
    s2fulldiskname += FULLDISKSLC;
    DiskPartitions_t diskandpartition;
    /* TODO: remove doing this every time since this opens efi library */
    EfiInfo_t efiinfo;
    struct stat64 dVolStat;

    if (piatluninfo.applianceTargetLUNNumber == 0)
    {
        disk = onlydiskname;
        disk += FULLDISKSLC;
        return disk;
    }
    
    GetDiskAndItsPartitions(dir.c_str(),
                            s2fulldiskname,
                            E_S2ISFULL,
                            diskandpartition,
                            efiinfo.GetEfiReadAddress(),
                            efiinfo.GetEfiFreeAddress());

    ConstDiskPartitionIter fulldiskiter;
    fulldiskiter = find_if(diskandpartition.begin(), diskandpartition.end(), DPEqDeviceType(piatluninfo.sourceType));
    if ((diskandpartition.end() != fulldiskiter) && (!stat64((*fulldiskiter).m_name.c_str(), &dVolStat))
        && (dVolStat.st_rdev))
    {
        disk = (*fulldiskiter).m_name;
    }
    
    return disk;
}


bool IsATLunCleanedUpFromFcInfo(const PIAT_LUN_INFO &piatluninfo,
                                const LocalPwwnInfo_t &lpwwn,
                                QuitFunction_t &qf,
                                const RemotePwwnInfo_t &rpwwn) 
{
    bool bcleaned = false;
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());
    bool bneedtoquit = (qf && qf(0));

    for (int f = 0; (f < NELEMS(FcInfoClean)) && !bcleaned && !bneedtoquit; f++)
    {
        for (int i = 0; i < NELEMS(cfgadmopts); i++)
        {
            bool bcleanedfromfcinfo = FcInfoClean[f](piatluninfo, lpwwn, qf, rpwwn, cfgadmopts[i]);
            bneedtoquit = (qf && qf(0));
            if (false == bneedtoquit)
            {
                if (bcleanedfromfcinfo)
                {
                    DebugPrintf(SV_LOG_DEBUG, "For %s, AT Lun is cleaned for cfgadm option %s\n", 
                                              msg.str().c_str(), cfgadmopts[i]);
                    bcleaned = true;
                    break;
                }
                else
                {
                    DebugPrintf(SV_LOG_WARNING, "For %s, AT Lun is not cleaned for cfgadm option %s\n", 
                                                msg.str().c_str(), cfgadmopts[i]);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "quit is signalled in process of calling fcinfo cleanup for %s\n",
                                          msg.str().c_str());
                break;
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());
    return bcleaned;
}


bool IsFcInfoLunCleaned(const PIAT_LUN_INFO &piatluninfo,
                        const LocalPwwnInfo_t &lpwwn,
                        QuitFunction_t &qf,
                        const RemotePwwnInfo_t &rpwwn, 
                        const char *opts)
{
    bool bcleaned = false;
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());
    const std::string UNUSABLE = "unusable";
    const std::string FAILING = "failing";

    std::string controller;
    controller += CONTROLLERCHAR;
    std::stringstream controllernumber;
    controllernumber << lpwwn.m_Pwwn.m_Number;
    controller += controllernumber.str();
    std::string controllerat = controller;
    controllerat += CFGADMCONTROLLERATSEP;
    controllerat += rpwwn.m_Pwwn.m_Name;
    std::string cfgadmcmd = CFGADMCMD;
    /* cfgadm -c unconfigure -o unusable_SCSI_LUN $controller_AT */
    std::string uncfgcmd = cfgadmcmd;
    std::string cfgcmd = cfgadmcmd;
    bool buseforce = false;
    bool buseonlyc = false;

    for (const char *p = opts; *p ; p++)
    {
        switch (*p)
        {
            case 'f':
                buseforce = true;
                break;
            case 'c':
                buseonlyc = true;
                break;
            default:
                DebugPrintf(SV_LOG_ERROR, "For %s, got wrong options to "
                                          "cfgadm\n", msg.str().c_str());
                break;
        }
    }

    std::string args = FormCfgAdmArgs("coz", UNCONFIGUREARG, UNUSABLESCSILUNARG, controllerat.c_str());
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "For %s, forming unconfigure command failed\n",
                                  msg.str().c_str());
    }
    else
    {
        uncfgcmd += args;
    }
    
    std::string force = buseforce ? "f" : "";
    const char *c = buseonlyc ? controller.c_str() : controllerat.c_str();
    std::string fmt = force;
    fmt += "cz";
    args = FormCfgAdmArgs(fmt.c_str(), CONFIGUREARG, c);
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm configure command for %s\n",
                                  msg.str().c_str());
    }
    else
    {
        cfgcmd += args;
    }
    
    std::string lunstate = GetLunStateInCfgAdm(lpwwn, rpwwn, 
                                               &piatluninfo.applianceTargetLUNNumber);
    DebugPrintf(SV_LOG_DEBUG, "For %s, found AT Lun state as %s\n", msg.str().c_str(),
                              lunstate.c_str());
    if (lunstate.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, AT Lun is not seen in cfgadm\n",
                                  msg.str().c_str());
        bcleaned = true;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For %s, AT Lun is seen in cfgadm before cleanup\n",
                                  msg.str().c_str());
        bool bneedsdevfsadm = false;
        bool bneedsuncfg = false;
        bool bneedscfg = false;
        bool bneedsluxadmoffline = false;

        /* TODO: here need to check if no 
         *       sleep is required ? The 
         *       scripts do not have any */
        if (UNUSABLE == lunstate)
        {
            bneedsuncfg = true;
        }
        else if (FAILING == lunstate)
        {
            bneedsuncfg = bneedsdevfsadm = true;
            bneedsluxadmoffline = !rpwwn.m_ATLunPath.empty();
        }
        else
        {
            bneedscfg = bneedsuncfg = bneedsdevfsadm = true;
            bneedsluxadmoffline = !rpwwn.m_ATLunPath.empty();
        }

        if (bneedscfg)
        {
            std::stringstream results;
            bool bexecdcfg = executePipe(cfgcmd, results);
            if (false == bexecdcfg)
            {
                DebugPrintf(SV_LOG_ERROR, "For %s, failed to execute command %s while at lun cleaning up\n", 
                                          msg.str().c_str(), cfgcmd.c_str());
            } 
        }

        if (bneedsluxadmoffline)
        {
            std::stringstream results;
            std::string luxadmcmd(LUXADMCMD);
            luxadmcmd += " ";
            luxadmcmd += LUXADMCMDEXPERTOPT;
            luxadmcmd += " ";
            luxadmcmd += OFFLINEARG;
            luxadmcmd += " ";
            luxadmcmd += rpwwn.m_ATLunPath;
            bool bexecdluxadm = executePipe(luxadmcmd, results);
            if (false == bexecdluxadm)
            {
                DebugPrintf(SV_LOG_ERROR, "For %s, failed to execute command %s while at lun cleaning up\n", 
                                          msg.str().c_str(), luxadmcmd.c_str());
            } 
        }
        
        if (bneedsdevfsadm)
        {
            bool brunneddevfsadm = RunATLunDisocovery();
            if (false == brunneddevfsadm)
            {
                DebugPrintf(SV_LOG_ERROR, "For %s, failed to execute devfsadm while at lun cleaning up\n", 
                                          msg.str().c_str());
            }
        }
  
        if (bneedsuncfg)
        {
            std::stringstream results;
            bool bexecduncfg = executePipe(uncfgcmd, results);
            if (false == bexecduncfg)
            {
                DebugPrintf(SV_LOG_ERROR, "For %s, failed to execute command %s while at lun cleaning up\n", 
                                          msg.str().c_str(), uncfgcmd.c_str());
            } 
        }

        lunstate = GetLunStateInCfgAdm(lpwwn, rpwwn, 
                                       &piatluninfo.applianceTargetLUNNumber);
        if (lunstate.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "For %s, AT Lun is not seen in cfgadm after cleanup\n",
                                      msg.str().c_str());
            bcleaned = true;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "For %s, AT Lun is seen in cfgadm after cleanup\n",
                                      msg.str().c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());
    return bcleaned;
}


void GetATLunPathsFromCumulation(const PIAT_LUN_INFO &piatluninfo, 
                                 const bool &bshouldscan, 
                                 QuitFunction_t &qf, 
                                 LocalPwwnInfos_t &lpwwns,
                                 std::set<std::string> &atlundirectpaths)
{
    std::stringstream msg;
    msg << "source LUN ID: " << piatluninfo.sourceLUNID
        << ", AT Lun Name: " << piatluninfo.applianceTargetLUNName
        << ", AT Lun Number: " << piatluninfo.applianceTargetLUNNumber;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    SV_ULONGLONG cumulativecount = 0;
    bool bneedtoquit = (qf && qf(0));

    /* TODO: if fcinfo scanning fails, then also we land 
     * here but there is no harm at all since it searches
     * for matching lun id and returns success if yes;
     * which means luns are scanned regardless of state in 
     * cfgadm; the check for source not to be fcinfo can
     * be added but not for now; also this comes in deletion 
     * path but is harmless */

    LocalPwwnInfos_t::iterator liter = lpwwns.begin();
    for ( /* empty */ ; (liter != lpwwns.end()) && !bneedtoquit; liter++)
    {
        const LocalPwwnInfo_t &lpwwn = liter->second;
        if (PWWNTYPEFC == lpwwn.m_Pwwn.m_Type)
        {
            SV_ULONGLONG count = GetATCountFromPI[lpwwn.m_PwwnSrc](piatluninfo, lpwwn, qf);
            cumulativecount += count;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "PI pwwn type %s is not currently handled for "
                                      "scanning; PI wwn %s, source LUN ID %s\n", 
                                      PwwnTypeStr[lpwwn.m_Pwwn.m_Type], 
                                      lpwwn.m_Pwwn.m_Name.c_str(),
                                      piatluninfo.sourceLUNID.c_str());
        }
        bneedtoquit = (qf && qf(0));
    }
    
    /* TODO: handle quit events properly across all */
    /* TODO: should remove check for cumulativecount;
     *       always check for device in "/dev/dsk" from
     *       addition and deletion */
    /* cumulativecount is meant for only knowing 
     * how many paths should have been there vs 
     * actually present to make decision making */

    DebugPrintf(SV_LOG_DEBUG, "For %s, got cumulative count of AT Lun devices as " ULLSPEC
                              "\n", msg.str().c_str(), cumulativecount);
    if (!bneedtoquit)
    {
        GetATLunDevices(piatluninfo, atlundirectpaths);
        SV_ULONGLONG atlundevicescount = atlundirectpaths.size();
        DebugPrintf(SV_LOG_DEBUG, "Number of AT Lun path detected in system " 
                                  "from cumulation is " ULLSPEC " for %s\n", 
                                  atlundevicescount, msg.str().c_str());

        if (bshouldscan && (atlundevicescount != cumulativecount))
        {
            DebugPrintf(SV_LOG_ERROR, "For %s, number of AT luns should have been "
                                      "present is " ULLSPEC ", actually found is "
                                      ULLSPEC "\n", msg.str().c_str(), 
                                      cumulativecount, atlundevicescount);
        }

        /* TODO: here we need to decide the
         * policy of how many number of 
         * atlundevices should be present to
         * proceed forward ? */
        if (atlundevicescount)
        {
            DebugPrintf(bshouldscan ? SV_LOG_DEBUG : SV_LOG_ERROR, "For %s, found AT Lun device%s", 
                                       msg.str().c_str(), (atlundevicescount > 1) ? "s" : "");
        }
        else
        {
            DebugPrintf(bshouldscan ? SV_LOG_ERROR : SV_LOG_DEBUG, "For %s, no AT Lun devices can be found\n", msg.str().c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is requested while getting AT lun names for "
                                  "%s\n", msg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());
}


void GetATLunDevices(const PIAT_LUN_INFO &piatluninfo, std::set<std::string> &atlundevices)
{
    std::stringstream msg;
    msg << "source LUN ID: " << piatluninfo.sourceLUNID
        << ", AT Lun Name: " << piatluninfo.applianceTargetLUNName
        << ", AT Lun Number: " << piatluninfo.applianceTargetLUNNumber;

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    bool brunneddevfsadm = RunATLunDisocovery();
    if (false == brunneddevfsadm)
    {
        DebugPrintf(SV_LOG_ERROR, "failed to run devfsadm for %s, " 
                                  "when getting AT Lun devices from device namespace\n", 
                                  msg.str().c_str());
    }

    EfiInfo_t efiinfo;
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    
    dentresult = NULL;
    dirp = opendir(OSNAMESPACEFORDEVICES);
    boost::shared_ptr<void> dirpGuard (static_cast<void*>(0), boost::bind(closedir, dirp));
    if (dirp)
    {
        dentp = (struct dirent *)calloc(direntsize, 1);
        if (dentp)
        {
            while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
            {
                E_DISKTYPE edsktyp = E_S2ISFULL;
                if (strcmp(dentp->d_name, ".") &&
                    strcmp(dentp->d_name, "..") &&
                    (DoesLunNumberMatch(dentp->d_name, &piatluninfo.applianceTargetLUNNumber, &edsktyp)))
                {
                    DiskPartitions_t diskandpartition;    
                    GetDiskAndItsPartitions(
                                            OSNAMESPACEFORDEVICES,
                                            dentp->d_name, 
                                            edsktyp, 
                                            diskandpartition,
                                            efiinfo.GetEfiReadAddress(),
                                            efiinfo.GetEfiFreeAddress()
                                           );
                    struct stat64 dVolStat;
                    ConstDiskPartitionIter fulldiskiter;
                    fulldiskiter = find_if(diskandpartition.begin(), diskandpartition.end(), DPEqDeviceType(piatluninfo.sourceType));
                    if ((diskandpartition.end() != fulldiskiter) && (!stat64((*fulldiskiter).m_name.c_str(), &dVolStat))
                        && dVolStat.st_rdev)
                    {
                        E_LUNMATCHSTATE lunmatchstate = GetLunMatchStateForDisk((*fulldiskiter).m_name, INMATLUNVENDOR, 
                                                                                piatluninfo.applianceTargetLUNName,
                                                                                piatluninfo.applianceTargetLUNNumber);
                        if (LUN_NAME_MATCHED == lunmatchstate)
                        {
                            DebugPrintf(SV_LOG_DEBUG, "For %s, found AT Lun name %s\n", 
                                                      msg.str().c_str(), (*fulldiskiter).m_name.c_str());
                            atlundevices.insert((*fulldiskiter).m_name);
                        }
                    }
                } /* end of skipping . and .. */
                memset(dentp, 0, direntsize);
             } /* end of while readdir_r */
            free(dentp);
        } /* endif of if (dentp) */
        else
        {
            DebugPrintf(SV_LOG_ERROR, "failed to allocate memory to open %s for %s\n", OSNAMESPACEFORDEVICES, msg.str().c_str());
        }
        //closedir(dirp); 
    } /* end of if (dirp) */
    else
    {
        DebugPrintf(SV_LOG_ERROR, "failed to open directory %s for %s\n", OSNAMESPACEFORDEVICES, msg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());
}


SV_ULONGLONG GetATCountFromUnknownSrcPI(const PIAT_LUN_INFO &piatluninfo, 
                                        const LocalPwwnInfo_t &lpwwn, 
                                        QuitFunction_t &qf)
{
    return 0;
}

SV_ULONGLONG GetATCountFromFcHostPI(const PIAT_LUN_INFO &piatluninfo, 
                                    const LocalPwwnInfo_t &lpwwn, 
                                    QuitFunction_t &qf)
{
    return 0;
}


SV_ULONGLONG GetATCountFromScsiHostPI(const PIAT_LUN_INFO &piatluninfo, 
                                      const LocalPwwnInfo_t &lpwwn, 
                                      QuitFunction_t &qf)
{
    return 0;
}


SV_ULONGLONG GetATCountFromQlaPI(const PIAT_LUN_INFO &piatluninfo, 
                                 const LocalPwwnInfo_t &lpwwn, 
                                 QuitFunction_t &qf)
{
    return 0;
}


SV_ULONGLONG GetATCountFromFcInfoPI(const PIAT_LUN_INFO &piatluninfo, 
                                    const LocalPwwnInfo_t &lpwwn, 
                                    QuitFunction_t &qf)
{
    return 0;
}

SV_ULONGLONG GetATCountFromiScsiPI(const PIAT_LUN_INFO &piatluninfo, 
                                    const LocalPwwnInfo_t &lpwwn, 
                                    QuitFunction_t &qf)
{
    return 0;
}


SV_ULONGLONG GetATCountFromScliPI(const PIAT_LUN_INFO &piatluninfo, 
                                  const LocalPwwnInfo_t &lpwwn, 
                                  QuitFunction_t &qf)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with source LUN ID: %s, PI: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str(),
                             lpwwn.m_Pwwn.m_Name.c_str());
    SV_ULONGLONG count = 0;
    bool bneedtoquit = (qf && qf(0));
    
    const RemotePwwnInfos_t &rpwwns = lpwwn.m_RemotePwwnInfos;
    RemotePwwnInfos_t::const_iterator riter = rpwwns.begin();
    for ( /* empty */; (riter != rpwwns.end()) && !bneedtoquit; riter++)
    {
        const RemotePwwnInfo_t &rpwwn = riter->second;
        if (PWWNTYPEFC != rpwwn.m_Pwwn.m_Type)
        {
            DebugPrintf(SV_LOG_ERROR, "AT pwwn type %s is not currently handled for "
                                      "scanning; PI wwn %s, AT wwn %s, source LUN ID %s\n", 
                                      PwwnTypeStr[rpwwn.m_Pwwn.m_Type], 
                                      lpwwn.m_Pwwn.m_Name.c_str(),
                                      rpwwn.m_Pwwn.m_Name.c_str(),
                                      piatluninfo.sourceLUNID.c_str());
        }
        else if (rpwwn.m_IsSentFromCX)
        {
            /* commented for now since scli -l is not required.
            bool bseesat = IsATVisibleInScli(piatluninfo, lpwwn, rpwwn, INMATLUNVENDOR);
            if (bseesat)
            {
            */
                count++;
            /*
            }
            */
        }
        bneedtoquit = (qf && qf(0));
    }

    /* TODO: since already scanning is done 
     * before doing -t; we do not need this 
     * keep this commented for now
    /* TODO: should we be scanning only if
     * bshouldscan is true ? 
    if (false == bneedtoquit)
    {
        bool bscannedpi = ScliScan(lpwwn);
        if (false == bscannedpi)
        {
            // TODO: what should be done here ? 
            //  count = 0; 
        }
    }
    */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s, PI: %s," 
                             "with number of AT lun devices seen: " ULLSPEC "\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str(), 
                             lpwwn.m_Pwwn.m_Name.c_str(), count);
    return count;
}


SV_ULONGLONG GetATCountFromLpfcPI(const PIAT_LUN_INFO &piatluninfo, 
                                  const LocalPwwnInfo_t &lpwwn, 
                                  QuitFunction_t &qf)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with source LUN ID: %s, PI: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str(),
                             lpwwn.m_Pwwn.m_Name.c_str());
    SV_ULONGLONG count = 0;
    bool bneedtoquit = (qf && qf(0));
    
    const RemotePwwnInfos_t &rpwwns = lpwwn.m_RemotePwwnInfos;
    RemotePwwnInfos_t::const_iterator riter = rpwwns.begin();
    for ( /* empty */; (riter != rpwwns.end()) && !bneedtoquit; riter++)
    {
        const RemotePwwnInfo_t &rpwwn = riter->second;
        if (PWWNTYPEFC != rpwwn.m_Pwwn.m_Type)
        {
            DebugPrintf(SV_LOG_ERROR, "AT pwwn type %s is not currently handled for "
                                      "scanning; PI wwn %s, AT wwn %s, source LUN ID %s\n", 
                                      PwwnTypeStr[rpwwn.m_Pwwn.m_Type], 
                                      lpwwn.m_Pwwn.m_Name.c_str(),
                                      rpwwn.m_Pwwn.m_Name.c_str(),
                                      piatluninfo.sourceLUNID.c_str());
        }
        else if (rpwwn.m_IsSentFromCX)
        {
            count++;
        }
        bneedtoquit = (qf && qf(0));
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s, PI: %s," 
                             "with number of AT lun devices seen: " ULLSPEC "\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str(), 
                             lpwwn.m_Pwwn.m_Name.c_str(), count);
    return count;
}


bool IsATVisibleInScli(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                       const RemotePwwnInfo_t &rpwwn, const std::string &vendor)
{
    bool bisvisible = false;
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    LocalConfigurator theLocalConfigurator;
    std::string sclipath(theLocalConfigurator.getScliPath());
    std::string cmd(sclipath);
    cmd += UNIX_PATH_SEPARATOR;
    cmd += SCLICMDNAME;
    cmd += " ";
    cmd += SCLILUNOPT;
    cmd += " ";
    /* TODO: format of pwwn */
    cmd += lpwwn.m_Pwwn.m_Name;
    cmd += " ";
    cmd += rpwwn.m_Pwwn.m_Name;
    cmd += " | /usr/bin/egrep \'(^Product Vendor|^Product ID|^LUN)\'";
    
    std::stringstream results;
    if (false == executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s for %s\n", cmd.c_str(), 
                                  msg.str().c_str());
        return bisvisible;
    }

    if (results.str().empty())
    {
        DebugPrintf(SV_LOG_WARNING, "command %s for %s returned no output\n", cmd.c_str(), 
                                    msg.str().c_str());
        return bisvisible;
    }

    const std::string PRODUCTVENDORTOKEN = "Product Vendor";

    std::string::size_type beginIdx = 0;
    //int i = 0;

    /*
    root@solaris154:/>: scli -l 21-00-00-E0-8B-19-E0-2A 21-00-00-E0-8B-11-F3-30
    Target WWPN 21-00-00-E0-8B-11-F3-30 PortID 01-09-00
    ------------------------------------------------------------
    Product Vendor                    : InMageDR (This will be InMage)
    Product ID                        : DUMMY_LUN_ZERO (This will be inmage000000010)
    Product Revision                  : 0001
    LUN                               : 0
    Size                              : 16444.28 GB
    Type                              : SBC-2 Direct access block device
                                       (e.g., magnetic disk)
    WWULN                             : 49-6E-4D-61-67-65-44-52-44-55-4D-4D-20-20-20-20
                                       20-20-20-20-20-20-20-20-31-31-34-35
    ------------------------------------------------------------
    Product Vendor                    : SCST_FIO
    Product ID                        : vlun2
    Product Revision                  :  096
    LUN                               : 100
    Size                              : 390.00 MB
    Type                              : SBC-2 Direct access block device
                                       (e.g., magnetic disk)
    WWULN                             : 53-43-53-54-5F-46-49-4F-76-6C-75-6E-32-20-20-20
                                       20-20-20-20-20-20-20-20-34-33-34-31-32
    ------------------------------------------------------------
    */
    
    do 
    {
        std::string::size_type idx = results.str().find(PRODUCTVENDORTOKEN, beginIdx);
        /* exit condition */
        if (std::string::npos == idx)
        {
            break;
        }

        if (idx < (results.str().length() - 1))
        {
            std::string::size_type incridx = idx + 1;
            idx = results.str().find(PRODUCTVENDORTOKEN, incridx);
        }
        
        if (std::string::npos == idx)
        {
            idx = results.str().length() + beginIdx;
        }

        std::stringstream lunstream(std::string(results.str().substr(beginIdx, idx - beginIdx)));
        beginIdx = idx;
        
        std::string hostportline;
        std::getline(lunstream, hostportline);
        if (hostportline.empty())
        {
            break;
        }
        
        while (!lunstream.eof())
        {
            std::string producttok, vendortok, colontok, gotvendor;
            lunstream >> producttok >> vendortok >> colontok >> vendortok;
            if (producttok.empty())
            {
                break;
            }
            if (gotvendor == vendor)
            {
                std::string idtok, gotid;
                lunstream >> producttok >> idtok >> colontok >> gotid;
                if (idtok.empty())
                {
                    break;
                }
                if (gotid == piatluninfo.applianceTargetLUNName)
                {
                    std::string luntok;
                    SV_ULONGLONG gotlunnumber = 0;
                    lunstream >> luntok >> colontok >> gotlunnumber;
                    if (luntok.empty())
                    {
                        break;
                    }
                    /* TODO: can it ever be 0 ? */
                    if (gotlunnumber == piatluninfo.applianceTargetLUNNumber)
                    {
                        DebugPrintf(SV_LOG_DEBUG, "For %s, lun number " ULLSPEC
                                                  ", AT Lun device is visible in scli -l\n",
                                                  msg.str().c_str(), 
                                                  piatluninfo.applianceTargetLUNNumber);
                        bisvisible = true;
                        break;
                    }
                }
            }
        }

        //++i;
    } while (!results.eof());

    if (false == bisvisible)
    {
        DebugPrintf(SV_LOG_ERROR, "For %s, lun number " ULLSPEC
                                  ", AT Lun device is not visible in scli -l\n",
                                  msg.str().c_str(), 
                                  piatluninfo.applianceTargetLUNNumber);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
    return bisvisible;
}


/* TODO: Not used for now; but if in future 
 * it is used, then should wait for 3 seconds
 * after scan */
bool ScliScan(const LocalPwwnInfo_t &lpwwn)
{
    LocalConfigurator theLocalConfigurator;
    std::string sclipath(theLocalConfigurator.getScliPath());
    std::string cmd(sclipath);
    cmd += UNIX_PATH_SEPARATOR;
    cmd += SCLICMDNAME;
    cmd += " ";
    cmd += SCLISCANOPT;
    cmd += " ";
    cmd += lpwwn.m_Pwwn.m_Name;
    cmd += " ";
    cmd += SCLISCANOPTSUFFIX;

    std::stringstream results;
    bool bscanned = executePipe(cmd, results);

    /* TODO: after scan sleep for 3 seconds; */
    /* TODO: then run devfsadm */

    return bscanned;
}


bool DeleteATLunsFromCumulation(const PIAT_LUN_INFO &piatluninfo, 
                                QuitFunction_t &qf, 
                                LocalPwwnInfos_t &lpwwns)
{
    std::set<std::string> atlundirectpaths;
    std::stringstream msg;
    msg << "source LUN ID: " << piatluninfo.sourceLUNID
        << ", AT Lun Name: " << piatluninfo.applianceTargetLUNName
        << ", AT Lun Number: " << piatluninfo.applianceTargetLUNNumber;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    /* TODO: if fcinfo deletion fails, then also we land 
     * here but there is no harm at all since it searches
     * for matching lun id and returns success if count is 0 
     * which means luns are deleted regardless of state in 
     * cfgadm; the check for source not to be fcinfo can
     * be added but not for now */

    /* No need for any scli stuff since 
     * scanning is done to get remote ports */
    GetATLunDevices(piatluninfo, atlundirectpaths);
    SV_ULONGLONG atlundevicescount = atlundirectpaths.size();
    DebugPrintf(SV_LOG_DEBUG, "Number of AT Lun path detected in system " 
                              "from cumulation is " ULLSPEC " for %s\n", 
                              atlundevicescount, msg.str().c_str());
  
    if (atlundevicescount)
    {
        DebugPrintf(SV_LOG_ERROR, "For %s, number of AT Luns visible are "
                                  ULLSPEC ". deletion of AT Luns failed\n", 
                                  msg.str().c_str(), atlundevicescount);
                          
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, number of AT Luns visible are "
                                  "zero. deletion of AT Luns succeeded\n", 
                                  msg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with details: %s\n",
                             FUNCTION_NAME, msg.str().c_str());
    return (0 == atlundevicescount);
}


/* TODO: there is no sleep required after
 *       devfsadm since this is blocking 
 *       command */
bool RunDevfsAdm(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string devfsadmcmd(DEVFSADMCMD);
    std::stringstream devfsadmresults;
    bool brunned = executePipe(devfsadmcmd, devfsadmresults);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return brunned;
}


std::string FormCfgAdmArgs(const char *opts, ...)
{
    std::string args;
    va_list ap;
    const char *p = 0;
    const char *optarg = 0;
    bool bshouldbreak = false;

    va_start(ap, opts);
    for (p = opts; *p ; p++)
    {
        switch (*p)
        {
            case 'c':
            case 'o':
                /* fallthrough */
                optarg = va_arg(ap, const char *);
                args += " ";
                args += "-";
                args += *p;
                args += " ";
                args += optarg;
                break;
            case 'a':
            case 'l':
            case 'f':
                /* fallthrough */
                args += " ";
                args += "-";
                args += *p;
                break;
            case 'z':
                /* From man page of cfgadm,
                 * z is unused option so far,
                 * hence used for appending args;
                 * also this is the last some
                 * thing analogous to append */
                optarg = va_arg(ap, const char *);
                args += " ";
                args += optarg;
                break;
            default:
                /* this is an error */
                args.clear();
                bshouldbreak = true;
                break;
        }
        if (bshouldbreak)
        {
            break;
        }
    }

    va_end(ap);
    return args;
}


void FcInfoLunNormalScan(const PIAT_LUN_INFO &piatluninfo, 
                         const LocalPwwnInfo_t &lpwwn,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn, 
                         const char *opts)
{
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID 
        << ", options: " << opts;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());
    std::string controller;
    controller += CONTROLLERCHAR;
    std::stringstream controllernumber;
    controllernumber << lpwwn.m_Pwwn.m_Number;
    controller += controllernumber.str();
    std::string controllerat = controller;
    controllerat += CFGADMCONTROLLERATSEP;
    controllerat += rpwwn.m_Pwwn.m_Name; // Sunil: c2::<target-pwwn>
    std::string cfgadmcmd = CFGADMCMD;
    bool buseforce = false;
    bool buseonlyc = false;

    for (const char *p = opts; *p ; p++)
    {
        switch (*p)
        {
            case 'f':
                buseforce = true;
                break;
            case 'c':
                buseonlyc = true;
                break;
            default:
                DebugPrintf(SV_LOG_ERROR, "For %s, got wrong options to "
                                          "cfgadm\n", msg.str().c_str());
                break;
        }
    }

    std::vector<std::string> cfgadmcmds;
    std::string uncfgunusablecmd;
    std::string args = FormCfgAdmArgs("coz", UNCONFIGUREARG, UNUSABLESCSILUNARG, controllerat.c_str());
    /* TODO: finally remove this checking of 
     * args.empty() */
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm unconfigure unusable "
                                  "scsi lun command for %s\n",
                                  msg.str().c_str());
    }
    else
    {
        uncfgunusablecmd = cfgadmcmd;
        uncfgunusablecmd += args;
        cfgadmcmds.push_back(uncfgunusablecmd);
    }

    /* only for configure, unconfigure commands, 
     * use -f and only controller */
    std::string force = buseforce ? "f" : "";
    const char *c = buseonlyc ? controller.c_str() : controllerat.c_str();
    std::string cfgcmd;
    std::string fmt = force;
    fmt += "cz";
    args = FormCfgAdmArgs(fmt.c_str(), CONFIGUREARG, c);
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm configure command for %s\n",
                                  msg.str().c_str());
    }
    else
    {
        cfgcmd = cfgadmcmd;
        cfgcmd += args;
        cfgadmcmds.push_back(cfgcmd);
    }

    bool bneedtoquit = (qf && qf(0));
    std::vector<std::string>::const_iterator citer = cfgadmcmds.begin();
    for ( /* empty */ ; citer != cfgadmcmds.end(); citer++)
    {
        const std::string &cmd = *citer;
        std::stringstream results;
        bool bexeccmd = executePipe(cmd, results);
        /* TODO: for now NSECS_TO_WAIT_AFTER_SCAN is common for 
         * linux and solaris; need to test and update accordingly */
        (qf && qf(NSECS_TO_WAIT_AFTER_SCAN));
        bneedtoquit = (qf && qf(0));
        if (!bneedtoquit)
        {
            if (false == bexeccmd)
            {
                DebugPrintf(SV_LOG_ERROR, "failed to do fcinfo scan command %s for %s\n", 
                                           cmd.c_str(), msg.str().c_str());
            }
            UpdateFcInfoATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, rpwwn);
            if (LUNSTATE_VALID == rpwwn.m_ATLunState)
            {
                DebugPrintf(SV_LOG_DEBUG, "for %s, the at lun is visible after fcinfo scan command %s\n",
                                          msg.str().c_str(), cmd.c_str());
                break;
            }
            else
            {
                /* recording as error since this is abnormal */
                DebugPrintf(SV_LOG_ERROR, "For %s, the at lun state is %s after fcinfo scan command %s\n",
                                          msg.str().c_str(), LunStateStr[rpwwn.m_ATLunState], cmd.c_str());
            }
        } 
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "quit is signalled in process of fcinfo scanning for %s\n",
                                      msg.str().c_str());
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());
}


bool UnconfigureAndConfigure(const std::string &uncfgcmd, 
                             const std::string &cfgcmd, 
                             QuitFunction_t &qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with unconfigure command: %s, "
                              "configure command: %s\n", FUNCTION_NAME, 
                              uncfgcmd.c_str(), cfgcmd.c_str()); 
    bool bexecd = false;
    
    std::stringstream uncfgresults;
    bexecd = executePipe(uncfgcmd, uncfgresults);
    /* using NSECS_TO_WAIT_AFTER_SCANDELETE since this is
     * called from both scan and delete */
    (qf && qf(NSECS_TO_WAIT_AFTER_SCANDELETE));
    bool bneedtoquit = (qf  && qf(0));

    if (false == bneedtoquit)
    {
        if (bexecd)
        {
            std::stringstream cfgresults; 
            bexecd = executePipe(cfgcmd, cfgresults);
            /* TODO: check whether sleep needed here since
             * as per poc script it is not there */
            (qf && qf(NSECS_TO_WAIT_AFTER_SCANDELETE));
            bneedtoquit = (qf  && qf(0));
            if (false == bneedtoquit)
            {
                if (false == bexecd)
                {
                    DebugPrintf(SV_LOG_ERROR, "failed to execute command %s\n", 
                                              cfgcmd.c_str());
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "failed to execute command %s\n", 
                                      uncfgcmd.c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with unconfigure command: %s, "
                              "configure command: %s\n", FUNCTION_NAME, 
                              uncfgcmd.c_str(), cfgcmd.c_str()); 
    return bexecd;
}


void ScanFcInfoATLun(const PIAT_LUN_INFO &piatluninfo, 
                     const LocalPwwnInfo_t &lpwwn,
                     QuitFunction_t &qf,
                     RemotePwwnInfo_t &rpwwn)
{
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());
    bool bneedtoquit = (qf && qf(0));
     
    for (int f = 0; (f < NELEMS(FcInfoScan)) && (LUNSTATE_VALID != rpwwn.m_ATLunState) && !bneedtoquit; f++)
    {
        for (int i = 0; i < NELEMS(cfgadmopts); i++)
        {
            FcInfoScan[f](piatluninfo, lpwwn, qf, rpwwn, cfgadmopts[i]);
            bneedtoquit = (qf && qf(0));
            if (false == bneedtoquit)
            {
                if (LUNSTATE_VALID == rpwwn.m_ATLunState)
                {
                    DebugPrintf(SV_LOG_DEBUG, "For %s, AT Lun is seen for cfgadm option %s\n", 
                                              msg.str().c_str(), cfgadmopts[i]);
                    break;
                }
                else
                {
                    /* recording as warning since this is abnormal; and also message is not helpful */
                    DebugPrintf(SV_LOG_WARNING, "For %s, AT Lun is not seen for cfgadm option %s\n", 
                                                msg.str().c_str(), cfgadmopts[i]);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "quit is signalled in process of calling fcinfo scanning for %s\n",
                                          msg.str().c_str());
                break;
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}

bool
DummyLunInCfgAdm(const LocalPwwnInfo_t &lpwwn,
                                const RemotePwwnInfo_t &rpwwn,
                                const SV_ULONGLONG *patlunnumber)
{
    /* -bash-3.00# cfgadm -al -o show_SCSI_LUN c4::2103001b32787057,0
     * Ap_Id                          Type         Receptacle   Occupant     Condition
     * c4::2103001b32787057,0         disk         connected    configured   unknown
     * -bash-3.00#
    */
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", Lun Number: " << *patlunnumber;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());
    std::string state;
    std::string controller;
    controller += CONTROLLERCHAR;
    std::stringstream controllernumber;
    controllernumber << lpwwn.m_Pwwn.m_Number;
    controller += controllernumber.str();
    std::string controlleratlun = controller;
    controlleratlun += CFGADMCONTROLLERATSEP;
    controlleratlun += rpwwn.m_Pwwn.m_Name;
    controlleratlun += CFGADMCONTROLLERATLUNSEP;
    std::stringstream sslunnumber;
    sslunnumber << *patlunnumber;
    controlleratlun += sslunnumber.str();

    std::string cfgadmcmd = CFGADMCMD;
    std::string args = FormCfgAdmArgs("aloz", SHOWSCSILUNARG, controlleratlun.c_str());
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm show scsi lun command for %s\n",
                                  msg.str().c_str());
    }
    else
    {
        cfgadmcmd += args;
        cfgadmcmd += " | /usr/bin/egrep \'disk\' | /usr/bin/egrep \'connected\' | /usr/bin/egrep \'configured\' | /usr/bin/egrep \'unknown\'";
    }

    std::stringstream results;
    if (false == executePipe(cfgadmcmd, results))
    {
        /* This is error case; but should never occur;
         * TODO: currently this will be treated as success
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n",
                                 cfgadmcmd.c_str(), msg.str().c_str());
        return false;
    }
    else if (results.str().empty())
    {
        DebugPrintf(SV_LOG_DEBUG,"command %s has returned nothing for %s\n",
                                 cfgadmcmd.c_str(), msg.str().c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s - DummyLunZ\n",
                             FUNCTION_NAME, msg.str().c_str());
    return true;
}



std::string GetLunStateInCfgAdm(const LocalPwwnInfo_t &lpwwn,
                                const RemotePwwnInfo_t &rpwwn, 
                                const SV_ULONGLONG *patlunnumber)
{
    /* -bash-3.00# cfgadm -al -o show_SCSI_LUN c4::2103001b32787057,0
     * Ap_Id                          Type         Receptacle   Occupant     Condition
     * c4::2103001b32787057,0         disk         connected    configured   unknown
     * -bash-3.00#
    */
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", Lun Number: " << *patlunnumber;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());
    std::string state;
    std::string controller;
    controller += CONTROLLERCHAR;
    std::stringstream controllernumber;
    controllernumber << lpwwn.m_Pwwn.m_Number;
    controller += controllernumber.str();
    std::string controlleratlun = controller;
    controlleratlun += CFGADMCONTROLLERATSEP;
    controlleratlun += rpwwn.m_Pwwn.m_Name;
    controlleratlun += CFGADMCONTROLLERATLUNSEP;
    std::stringstream sslunnumber;
    sslunnumber << *patlunnumber;
    controlleratlun += sslunnumber.str();
     
    std::string cfgadmcmd = CFGADMCMD;
    std::string args = FormCfgAdmArgs("aloz", SHOWSCSILUNARG, controlleratlun.c_str());
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm show scsi lun command for %s\n",
                                  msg.str().c_str());
    }
    else
    {
        cfgadmcmd += args;
    }
     
    std::stringstream results;
    if (false == executePipe(cfgadmcmd, results)) 
    {
        /* This is error case; but should never occur; 
         * TODO: currently this will be treated as success 
         * change it to return error */
        DebugPrintf(SV_LOG_ERROR,"Unable to run command %s for %s\n", 
                                 cfgadmcmd.c_str(), msg.str().c_str());
    }
    else if (results.str().empty())
    {
        /* This is success case */
        DebugPrintf(SV_LOG_DEBUG,"command %s has returned nothing for %s\n", 
                                 cfgadmcmd.c_str(), msg.str().c_str());
    }
    else
    {
        std::string headingline;
        std::getline(results, headingline);
        if (!headingline.empty())
        {
            std::stringstream ssheader(headingline);
            const std::string CONDITION = "Condition";
            int conditionpos = -1;
            for (int i = 0; !ssheader.eof(); i++)
            {
                std::string token;
                ssheader >> token;
                if (CONDITION == token)
                {
                    conditionpos = i;
                    break;
                }
            }
            if ((conditionpos >= 0) && (!results.eof()))
            {
                DebugPrintf(SV_LOG_DEBUG, "For %s, condition position is %d\n",
                            msg.str().c_str(), conditionpos);
                std::string statusline;
                std::getline(results, statusline);
                if (!statusline.empty())
                {
                    std::stringstream ssstatusline(statusline);
                    for (int i = 0; i < conditionpos; i++)
                    {
                        std::string skip;
                        ssstatusline >> skip;
                    }
                    ssstatusline >> state;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "For %s, command %s did not give AT Lun state\n",
                                          msg.str().c_str(), cfgadmcmd.c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "For %s, command %s gave empty output\n",
                                      msg.str().c_str(), cfgadmcmd.c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s with at lun state %s\n",
                             FUNCTION_NAME, msg.str().c_str(), state.c_str());
    return state;
}


std::string GetFcInfoOnlyDiskName(const std::string &dir, 
                                  const LocalPwwnInfo_t &lpwwn,
                                  const RemotePwwnInfo_t &rpwwn,
                                  const SV_ULONGLONG *patlunnumber)
{
    std::string onlydisk = dir;
    onlydisk += UNIX_PATH_SEPARATOR;
    onlydisk += CONTROLLERCHAR;
    std::stringstream sscontrollernumber;
    sscontrollernumber << lpwwn.m_Pwwn.m_Number;
    onlydisk += sscontrollernumber.str();
    onlydisk += SOL_TARGETCHAR;
    /* TODO: what about iscsi ? */
    /* TODO: format of pwwn ? */
    std::string target = GetUpperCase(rpwwn.m_Pwwn.m_Name);
    onlydisk += target;
    onlydisk += SOL_LUNCHAR;
    std::stringstream sslunnumber;
    sslunnumber << *patlunnumber;
    onlydisk += sslunnumber.str();

    return onlydisk;
}


/* TODO: Check this might be working only for
 * controller while testing ? */
void FcInfoLunRetryScan(const PIAT_LUN_INFO &piatluninfo, 
                        const LocalPwwnInfo_t &lpwwn,
                        QuitFunction_t &qf,
                        RemotePwwnInfo_t &rpwwn, 
                        const char *opts)
{
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID 
        << ", options: " << opts;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());
    std::string controller;
    controller += CONTROLLERCHAR;
    std::stringstream controllernumber;
    controllernumber << lpwwn.m_Pwwn.m_Number;
    controller += controllernumber.str();
    std::string controllerat = controller;
    controllerat += CFGADMCONTROLLERATSEP;
    controllerat += rpwwn.m_Pwwn.m_Name;
    std::string cfgadmcmd = CFGADMCMD;
    bool buseforce = false;
    bool buseonlyc = false;

    for (const char *p = opts; *p ; p++)
    {
        switch (*p)
        {
            case 'f':
                buseforce = true;
                break;
            case 'c':
                buseonlyc = true;
                break;
            default:
                DebugPrintf(SV_LOG_ERROR, "For %s, got wrong options to "
                                          "cfgadm\n", msg.str().c_str());
                break;
        }
    }

    std::string force = buseforce ? "f" : "";
    std::string fmt = force;
    fmt += "cz";
    const char *c = buseonlyc ? controller.c_str() : controllerat.c_str();
    std::string cfgcmd;
    std::string args = FormCfgAdmArgs(fmt.c_str(), CONFIGUREARG, c);
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm configure command "
                                  "for %s\n", msg.str().c_str());
    }
    else
    {
        cfgcmd = cfgadmcmd;
        cfgcmd += args;
    }

    std::string uncfgcmd;
    args = FormCfgAdmArgs(fmt.c_str(), UNCONFIGUREARG, c);
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm unconfigure command for %s\n", 
                                  msg.str().c_str());
    }
    else
    {
        uncfgcmd = cfgadmcmd;
        uncfgcmd += args;
    }

    bool bneedtoquit = (qf && qf(0));
    if (false == bneedtoquit)
    {
        DebugPrintf(SV_LOG_WARNING, "For %s, did not get AT Lun from "
                                    "fcinfo scan commands. Trying "
                                    "unconfigure and configure\n", 
                                    msg.str().c_str());
        bool bexecd = UnconfigureAndConfigure(uncfgcmd, cfgcmd, qf);
        bneedtoquit = (qf && qf(0));
        if (false == bneedtoquit)
        {
            if (false == bexecd)
            {
                DebugPrintf(SV_LOG_ERROR, "For %s, failed to do unconfigure and "
                                          "configure\n", msg.str().c_str());
            }
            UpdateFcInfoATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, rpwwn);
            if (LUNSTATE_VALID == rpwwn.m_ATLunState)
            {
                DebugPrintf(SV_LOG_DEBUG, "for %s, the at lun is visible after unconfigure and configure\n", 
                                          msg.str().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "for %s, the at lun state is %s after unconfigure command %s, "
                                          "and configure command %s\n", msg.str().c_str(), 
                                          LunStateStr[rpwwn.m_ATLunState], uncfgcmd.c_str(), cfgcmd.c_str());
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());
}


/* TODO: Check this might be working only for
 * controller while testing ? */
bool IsFcInfoLunCleanedOnRetry(const PIAT_LUN_INFO &piatluninfo, 
                               const LocalPwwnInfo_t &lpwwn,
                               QuitFunction_t &qf,
                               const RemotePwwnInfo_t &rpwwn, 
                               const char *opts)
{
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID 
        << ", options: " << opts;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());
    bool bcleaned = false;
    std::string controller;
    controller += CONTROLLERCHAR;
    std::stringstream controllernumber;
    controllernumber << lpwwn.m_Pwwn.m_Number;
    controller += controllernumber.str();
    std::string controllerat = controller;
    controllerat += CFGADMCONTROLLERATSEP;
    controllerat += rpwwn.m_Pwwn.m_Name;
    std::string cfgadmcmd = CFGADMCMD;
    bool buseforce = false;
    bool buseonlyc = false;

    for (const char *p = opts; *p ; p++)
    {
        switch (*p)
        {
            case 'f':
                buseforce = true;
                break;
            case 'c':
                buseonlyc = true;
                break;
            default:
                DebugPrintf(SV_LOG_ERROR, "For %s, got wrong options to "
                                          "cfgadm\n", msg.str().c_str());
                break;
        }
    }

    std::string force = buseforce ? "f" : "";
    std::string fmt = force;
    fmt += "cz";
    const char *c = buseonlyc ? controller.c_str() : controllerat.c_str();
    std::string cfgcmd;
    std::string args = FormCfgAdmArgs(fmt.c_str(), CONFIGUREARG, c);
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm configure command "
                                  "for %s\n", msg.str().c_str());
    }
    else
    {
        cfgcmd = cfgadmcmd;
        cfgcmd += args;
    }

    std::string uncfgcmd;
    args = FormCfgAdmArgs(fmt.c_str(), UNCONFIGUREARG, c);
    if (args.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to form cfgadm unconfigure command for %s\n", 
                                  msg.str().c_str());
    }
    else
    {
        uncfgcmd = cfgadmcmd;
        uncfgcmd += args;
    }

    bool bneedtoquit = (qf && qf(0));
    if (false == bneedtoquit)
    {
        DebugPrintf(SV_LOG_WARNING, "For %s, AT Lun clean up did not happen from "
                                    "fcinfo scan commands. Trying "
                                    "unconfigure and configure\n", 
                                    msg.str().c_str());
        bool bexecd = UnconfigureAndConfigure(uncfgcmd, cfgcmd, qf);
        bneedtoquit = (qf && qf(0));
        if (false == bneedtoquit)
        {
            if (false == bexecd)
            {
                DebugPrintf(SV_LOG_ERROR, "For %s, failed to do unconfigure and "
                                          "configure\n", msg.str().c_str());
            }
            std::string lunstate = GetLunStateInCfgAdm(lpwwn, rpwwn, 
                                                       &piatluninfo.applianceTargetLUNNumber);
            bcleaned = lunstate.empty();
            if (bcleaned)
            {
                DebugPrintf(SV_LOG_DEBUG, "for %s, the at lun is cleaned after unconfigure and configure\n", 
                                          msg.str().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "for %s, the at lun is not cleaned after unconfigure and configure\n", 
                                          msg.str().c_str());
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());
    return bcleaned;
}

void DeleteATLunFromIScsiHost(const PIAT_LUN_INFO &prismvolumeinfo,
                         const LocalPwwnInfo_t &lpwwn,
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
    return DeleteATLunFromIScsiAdm(prismvolumeinfo, lpwwn, channels, qf, rpwwn);
}

void GetATLunPathFromIScsiHost(const PIAT_LUN_INFO &prismvolumeinfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn)
{
    return GetATLunPathFromIScsiAdm(prismvolumeinfo, lpwwn, bshouldscan, channelnumbers, qf, rpwwn);
}

void GetRemotePwwnsFromIScsiHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    return GetRemotePwwnsFromIScsiAdm(pLocalPwwnInfo, qf);
}

void DiscoverRemoteIScsiTargets(std::list<std::string> networkAddresses)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);

    std::string cmd("/usr/sbin/iscsiadm list discovery-address | /bin/awk -F: \'{print $2}\' | /bin/sed \'/ //g\'");
    std::stringstream results;
    std::list<std::string> ipAddrs;
    
    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string ipAddr;
            results >> ipAddr;
            ipAddrs.push_front(ipAddr);
        }

        bool bFoundIpAddr = false;
        for (std::list<std::string>::iterator it=networkAddresses.begin(); it != networkAddresses.end(); ++it)
        {
            std::string sentIpAddr = *it;

            for (std::list<std::string>::iterator it1=ipAddrs.begin(); it1 != ipAddrs.end(); ++it1)
            {
                std::string ipAddr = *it1;
                if (ipAddr == sentIpAddr)
                {
                    bFoundIpAddr = true;
                    break; 
                }
            }

            if (!bFoundIpAddr)
            {
                DebugPrintf(SV_LOG_DEBUG, "Appliance %s not found in remote node list. Adding to discovery list.\n", 
                                          sentIpAddr.c_str());
                std::string cmd1("/usr/sbin/iscsiadm add discovery-address ");
                cmd1 += sentIpAddr;
            
                if (executePipe(cmd1, results))
                {
                    DebugPrintf(SV_LOG_DEBUG, "Disocvery of %s is complete.\n", sentIpAddr.c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd1.c_str());
                }
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return;
}

void DiscoverRemoteFcTargets()
{
    return;
}

