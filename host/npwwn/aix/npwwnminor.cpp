#include <ace/Synch.h>

#include "npwwn.h"
#include "npwwnminor.h"
#include "logger.h"
#include "portable.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "event.h"
#include "inmstrcmp.h"
#include "scsicmd.h"
#include "executecommand.h"

ACE_Thread_Mutex g_CfgMgrMutex;
ACE_Condition<ACE_Thread_Mutex> g_CfgMgrConditionVariable(g_CfgMgrMutex);
bool g_CfgMgrFlag = true;

void RunATLunDisocovery(std::string fcAdapter)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    g_CfgMgrMutex.acquire();

    if (g_CfgMgrFlag)
    {
        g_CfgMgrFlag = false;
        g_CfgMgrMutex.release();

        DebugPrintf(SV_LOG_DEBUG,"Running AT Lun discovery...\n");
        RunCfgMgr(fcAdapter);

        g_CfgMgrMutex.acquire();
        g_CfgMgrFlag = true;
        g_CfgMgrConditionVariable.broadcast();
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Waiting for AT Lun discovery to complete...\n");
        g_CfgMgrConditionVariable.wait();
    }

    g_CfgMgrMutex.release();

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetLocalPwwnInfos(LocalPwwnInfos_t &lpwwns)
{
    GetLocalFcPwwnInfos(lpwwns);
    GetLocalIScsiPwwnInfos(lpwwns);
}

void GetLocalFcPwwnInfos(LocalPwwnInfos_t &lpwwns)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    const std::string cmd("/usr/sbin/lsdev  -Cc adapter | /usr/bin/grep 'fcs' | /usr/bin/awk '{print $1}'");
    std::stringstream results;
    /*
     * Output:
     * fcs0
     * fcs1
     * fcs2
     * ...
     */
    
    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string fcadapter;
            results >> fcadapter;
            if (fcadapter.empty())
            {
                break;
            }

            std::string cmd1("/usr/sbin/lscfg -v -l ");
            cmd1 += fcadapter.c_str(); 
            cmd1 += " | /usr/bin/egrep '(Network Address|Device Specific.\\(Z8\\))' | /usr/bin/awk -F'.' '{print $NF}'";
            std::stringstream results1;
            /*
             * Output:
             * 10000000C928075B
             * 20000000C928075B
             */
    
            if (executePipe(cmd1, results1))
            {
                while (!results1.eof())
                {
                    LocalPwwnInfo_t lpwwn;
                    results1 >> lpwwn.m_Pwwn.m_Name;
                    results1 >> lpwwn.m_Pwwn.m_Nwwn;

                    if ((lpwwn.m_Pwwn.m_Name.length() == 16) && (lpwwn.m_Pwwn.m_Nwwn.length() == 16))
                    {
                        FormatWwn(LSCFG, lpwwn.m_Pwwn.m_Nwwn, lpwwn.m_Pwwn.m_FormattedNwwn, &lpwwn.m_Pwwn.m_Type);
                        FormatWwn(LSCFG, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);

                        if (!lpwwn.m_FormattedName.empty())
                        {
                            const char *p = GetStartOfLastNumber(fcadapter.c_str());
                            if (p && (1 == sscanf(p, LLSPEC, &lpwwn.m_Pwwn.m_Number)) && 
                                     (lpwwn.m_Pwwn.m_Number >= 0))
                            {
                                lpwwn.m_PwwnSrc = LSCFG;
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
                                                          PwwnSrcStr[LSCFG]);
                            }
                        }
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd1.c_str());
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetLocalIScsiPwwnInfos(LocalPwwnInfos_t &lpwwns)
{
    GetLocalIScsiToePwwnInfos(lpwwns);
    GetLocalIScsiSwPwwnInfos(lpwwns);
}

void GetLocalIScsiToePwwnInfos(LocalPwwnInfos_t &lpwwns)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    const std::string cmd("/usr/sbin/lsdev -Cc adapter | /usr/bin/egrep 'iSCSI Adapter' | /usr/bin/awk '{print $1}'");
    std::stringstream results;
    /*
     * Output:
     * ics0
     * ics1
     * ...
     */

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string toeadapter;
            results >> toeadapter;
            if (!toeadapter.empty())
            {
                std::string cmd1("/usr/sbin/lscfg -vl ");
                cmd1 +=  toeadapter.c_str();
                cmd1 += " | /usr/bin/egrep 'Device Specific.\\(Z1\\)'";
                std::stringstream results1;

                if (executePipe(cmd1, results1))
                {
                    while (!results1.eof())
                    {
                        std::string line;
                        results1 >> line;
                        size_t index = line.find("iqn");
                        if (index == std::string::npos)
                        {
                            index = line.find("eui");
                        }
                        if (index == std::string::npos)
                        {
                            index = line.find("naa");
                        }
                        if (index == std::string::npos)
                            continue;
                        std::string iscsiName = line.substr(index);
                        LocalPwwnInfo_t lpwwn;
                        lpwwn.m_Pwwn.m_Name = iscsiName ;
                        FormatWwn(LSCFG, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);

                        if (!lpwwn.m_FormattedName.empty())
                        {
                            const char *p = GetStartOfLastNumber(toeadapter.c_str());
                            if (p && (1 == sscanf(p, LLSPEC, &lpwwn.m_Pwwn.m_Number)) && 
                                     (lpwwn.m_Pwwn.m_Number >= 0))
                            {
                                lpwwn.m_PwwnSrc = LSCFG;
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
                                                          PwwnSrcStr[LSCFG]);
                            }
                        }
                    }
                }
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetLocalIScsiSwPwwnInfos(LocalPwwnInfos_t &lpwwns)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    const std::string cmd("/usr/sbin/lsdev | /usr/bin/egrep 'iSCSI Protocol Device' | /usr/bin/awk '{print $1}'");
    std::stringstream results;
    /*
     * Output:
     * iscsi0
     * iscsi1
     * ...
     */

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string iscsiadapter;
            results >> iscsiadapter;
            if (!iscsiadapter.empty())
            {
                std::string cmd1("/usr/sbin/lsattr -El ");
                cmd1 +=  iscsiadapter.c_str();
                cmd1 += " -a initiator_name";
                cmd1 += " | /usr/bin/awk '{print $2}";
                std::stringstream results1;

                if (executePipe(cmd1, results1))
                {
                    while (!results1.eof())
                    {
                        std::string iscsiName;
                        results1 >> iscsiName;
                        if (iscsiName.empty())
                            continue;

                        LocalPwwnInfo_t lpwwn;
                        lpwwn.m_Pwwn.m_Name = iscsiName ;

                        FormatWwn(LSCFG, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);

                        if (!lpwwn.m_FormattedName.empty())
                        {
                            const char *p = GetStartOfLastNumber(iscsiadapter.c_str());
                            if (p && (1 == sscanf(p, LLSPEC, &lpwwn.m_Pwwn.m_Number)) && 
                                     (lpwwn.m_Pwwn.m_Number >= 0))
                            {
                                lpwwn.m_PwwnSrc = LSCFG;
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
                                                          PwwnSrcStr[LSCFG]);
                            }
                        }
                    }
                }
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetRemotePwwnsFromFcInfo(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string adapter;
    LocalPwwnInfo_t lpwwn = *pLocalPwwnInfo;
    if (lpwwn.m_Pwwn.m_Type == PWWNTYPEFC)
    {
        GetFcAdapterFromPI(lpwwn, qf, adapter);
        GetRemotePwwnsFromFcAdapter(pLocalPwwnInfo, qf, adapter);
    }
    else if (lpwwn.m_Pwwn.m_Type == PWWNTYPEISCSI)
    {
        GetIScsiAdapterFromPI(lpwwn, qf, adapter);
        GetRemotePwwnsFromIScsiAdapter(pLocalPwwnInfo, qf, adapter);
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetFcAdapterFromPI(const LocalPwwnInfo_t &localPwwnInfo, QuitFunction_t &qf, std::string &fcadapter)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    const std::string cmd("/usr/sbin/lsdev  -Cc adapter | /usr/bin/grep 'fcs' | /usr/bin/awk '{print $1}'");
    std::stringstream results;
    /*
     * Output:
     * fcs0
     * fcs1
     * fcs2
     * ...
     */
    
    if (executePipe(cmd, results))
    {
        bool bFoundAdapter =  false;
        while (!results.eof() && !bFoundAdapter)
        {
            results >> fcadapter;
            if (fcadapter.empty())
            {
                break;
            }

            std::string cmd1("/usr/sbin/lscfg -v -l ");
            cmd1 += fcadapter.c_str(); 
            cmd1 += " | /usr/bin/grep 'Network Address' | /usr/bin/awk -F'.' '{print $NF}'";
            std::stringstream results1;
            /*
             * Output:
             * 10000000C928075B
             */
    
            if (executePipe(cmd1, results1))
            {
                while (!results1.eof())
                {
                    LocalPwwnInfo_t lpwwn;
                    results1 >> lpwwn.m_Pwwn.m_Name;

                    if (lpwwn.m_Pwwn.m_Name.length() == 16)
                    {
                        FormatWwn(LSCFG, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);

                        if (!lpwwn.m_FormattedName.empty())
                        {
                            if (localPwwnInfo.m_FormattedName.compare(lpwwn.m_FormattedName.c_str()) == 0)
                            {
                                bFoundAdapter = true;
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd1.c_str());
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
        return;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetIScsiAdapterFromPI(const LocalPwwnInfo_t &localPwwnInfo, QuitFunction_t &qf, std::string &adapter)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    const std::string cmd("/usr/sbin/lsdev  -Cc adapter | /usr/bin/grep 'iSCSI Adapter' | /usr/bin/awk '{print $1}'");
    std::stringstream results;
    bool bFoundAdapter =  false;
    /*
     * Output:
     * ics0
     * ics1
     * ics2
     * ...
     */
    
    if (executePipe(cmd, results))
    {
        while (!results.eof() && !bFoundAdapter)
        {
            results >> adapter;
            if (adapter.empty())
            {
                break;
            }

            std::string cmd1("/usr/sbin/lscfg -v -l ");
            cmd1 += adapter.c_str(); 
            cmd1 += " | /usr/bin/egrep 'Device Specific.\\(Z1\\)'";
            std::stringstream results1;
            /*
             * Output:
             * iqn.1992-08.com.ibm:aix17-196:0839230
             */
    
            if (executePipe(cmd1, results1))
            {
                while (!results1.eof())
                {
                    LocalPwwnInfo_t lpwwn;
                    std::string line;
                    results1 >> line;
                    size_t index = line.find("iqn");
                    if (index == std::string::npos)
                    {
                        index = line.find("eui");
                    }
                    if (index == std::string::npos)
                    {
                        index = line.find("naa");
                    }
                    if (index == std::string::npos)
                        continue;
                    std::string iscsiName = line.substr(index);
                    lpwwn.m_Pwwn.m_Name = iscsiName ;

                    FormatWwn(LSCFG, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);

                    if (!lpwwn.m_FormattedName.empty())
                    {
                        if (localPwwnInfo.m_FormattedName.compare(lpwwn.m_FormattedName.c_str()) == 0)
                        {
                            bFoundAdapter = true;
                            break;
                        }
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd1.c_str());
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    if (!bFoundAdapter)
    {
        const std::string cmd2("/usr/sbin/lsdev | /usr/bin/egrep 'iSCSI Protocol Device' | /usr/bin/awk '{print $1}'");
        std::stringstream results2;
        /*
         * Output:
         * iscsi0
         * iscsi1
         * ...
         */

        if (executePipe(cmd2, results2))
        {
            while (!results2.eof())
            {
                results2 >> adapter;
                if (!adapter.empty())
                {
                    std::string cmd3("/usr/sbin/lsattr -El ");
                    cmd3 +=  adapter.c_str();
                    cmd3 += " -a initiator_name";
                    cmd3 += " | /usr/bin/awk '{print $2}";
                    std::stringstream results3;

                    if (executePipe(cmd3, results3))
                    {
                        while (!results3.eof())
                        {
                            LocalPwwnInfo_t lpwwn;
                            std::string iscsiName;
                            results3 >> iscsiName;
                            if (iscsiName.empty())
                                continue;
                            lpwwn.m_Pwwn.m_Name = iscsiName ;

                            FormatWwn(LSCFG, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);

                            if (!lpwwn.m_FormattedName.empty())
                            {
                                if (localPwwnInfo.m_FormattedName.compare(lpwwn.m_FormattedName.c_str()) == 0)
                                {
                                    bFoundAdapter = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetRemotePwwnsFromFcAdapter(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf, std::string &fcadapter)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string cmd("/usr/sbin/lsdev -p ");
    cmd +=  fcadapter.c_str();
    cmd +=  " | /usr/bin/grep 'FC SCSI' | /usr/bin/awk '{print $1}'";
    std::stringstream results;
    /*
     * Output:
     * fscsi0
     * fscsi1
     * ...
     */

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string fctarg;
            results >> fctarg;
            if (fctarg.empty())
                continue;
            std::string cmd1("/usr/sbin/lsdev -p ");
            cmd1 +=  fctarg.c_str();
            cmd1 +=  " | /usr/bin/grep 'Available' | /usr/bin/grep 'FC SCSI Disk Drive' | /usr/bin/awk '{print $1}'";
            std::stringstream results1;
            /*
             * Output:
             * hdisk0 
             * hdisk1
             * ...
             */

            if (executePipe(cmd1, results1))
            {
                while (!results1.eof())
                {
                    std::string fcdisk;
                    results1 >> fcdisk;

                    if (fcdisk.empty())
                        continue;

                    std::string cmd2 ("/usr/sbin/lsattr -El ");
                    cmd2 +=  fcdisk.c_str();
                    cmd2 += " | /usr/bin/grep ww_name | /usr/bin/awk '{print $2}' | /usr/bin/awk -Fx '{print $2}'";
                    std::stringstream results2;
                    /*
                     * Output:
                     * 10000000C928075B
                     */

                    if (executePipe(cmd2, results2))
                    {
                        while (!results2.eof())
                        {
                            std::string rport;
                            RemotePwwnInfo_t rpwwn;
                            results2 >> rport;
                            if (rport.length() == 16)
                            {
                                FormatWwn(LSCFG, rport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                                if (!rpwwn.m_FormattedName.empty())
                                {
                                    rpwwn.m_Pwwn.m_Name = rport;
                                    InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, rpwwn);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd1.c_str());
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetRemotePwwnsFromIScsiAdapter(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf, std::string &adapter)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string cmd("/usr/sbin/lsdev -p ");
    cmd +=  adapter.c_str();
    cmd +=  " | /usr/bin/grep 'Available' | /usr/bin/grep 'iSCSI Disk Drive' | /usr/bin/awk '{print $1}'";
    std::stringstream results;
    /*
     * Output:
     * hdisk1 
     * hdisk2 
     * ...
     */

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string iscsidisk;
            results >> iscsidisk;

            if (iscsidisk.empty())
                continue;

            std::string cmd1 ("/usr/sbin/lsattr -El ");
            cmd1 +=  iscsidisk.c_str();
            cmd1 +=  " -a target_name";
            cmd1 +=  " | /usr/bin/awk '{print $2}'";
            std::stringstream results1;

            /*
             * Output:
             * iqn.1992-08.com.ibm:storage:disk:0839230
             */

            if (executePipe(cmd1, results1))
            {
                while (!results1.eof())
                {
                    std::string rport;
                    RemotePwwnInfo_t rpwwn;
                    results1 >> rport;
                    FormatWwn(LSCFG, rport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                    if (!rpwwn.m_FormattedName.empty())
                    {
                        rpwwn.m_Pwwn.m_Name = rport;
                        InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, rpwwn);
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd1.c_str());
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void DeleteATLunFromFcInfo(const PIAT_LUN_INFO &piatluninfo,
                           const LocalPwwnInfo_t &lpwwn,
                           Channels_t &channels,
                           QuitFunction_t &qf,
                           RemotePwwnInfo_t &rpwwn)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string fcAdapter;

    if (!rpwwn.m_FormattedName.empty())
    {
        if (lpwwn.m_Pwwn.m_Type == PWWNTYPEFC)
        {
            GetFcAdapterFromPI(lpwwn, qf, fcAdapter);
            DeleteOfflineATLunPathsFromAdapter(piatluninfo, lpwwn, rpwwn, qf, fcAdapter);
        }
        else if (lpwwn.m_Pwwn.m_Type == PWWNTYPEISCSI)
        {
            // GetIScsiAdapterFromPI(lpwwn, qf, adapter);
            // DeleteOfflineIScsiATLunPathsFromAdapter(piatluninfo, lpwwn, rpwwn, qf, fcAdapter);
        }
        // RunCfgMgr(fcAdapter); // Is this enough M$?
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetATLunPathFromFcInfo(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            const bool &bshouldscan,
                            Channels_t &channelnumbers,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string adapter;
    if (lpwwn.m_Pwwn.m_Type == PWWNTYPEFC)
    {
        GetFcAdapterFromPI(lpwwn, qf, adapter);
        GetATLunPathsFromFcAdapter(piatluninfo, lpwwn, rpwwn, qf, adapter, bshouldscan);
    }
    else if (lpwwn.m_Pwwn.m_Type == PWWNTYPEISCSI)
    {
        GetIScsiAdapterFromPI(lpwwn, qf, adapter);
        GetATLunPathsFromIScsiAdapter(piatluninfo, lpwwn, rpwwn, qf, adapter, bshouldscan);
    }

    // RunCfgMgr(adapter);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void RunCfgMgr(const std::string &fcAdapter)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string cmd("/usr/sbin/cfgmgr ");
    if (!fcAdapter.empty())
    {
        cmd += " -l " ;
        cmd += fcAdapter;
    }
    std::stringstream results;

    if (executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully completed %s\n", cmd.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool GetVpdMatchStateForDisk(const std::string disk, const std::string applianceTargetLUNName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    bool match = false;

    std::string cmd("/usr/bin/odmget -q name=");
    cmd += disk.c_str();
    cmd += " CuVPD | grep -w ";
    cmd += applianceTargetLUNName.c_str();
    cmd += " | awk -F* '{print $1}'";
    std::stringstream results;

    if (executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully completed %s with result %s\n", cmd.c_str(), results.str().c_str());

        if ( results.str().size() >= applianceTargetLUNName.size() &&
             (results.str().compare(0, applianceTargetLUNName.size(), applianceTargetLUNName) == 0) )
        {
            match = true;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s with result %s\n", cmd.c_str(), results.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return match;
}

enum INM_VPD_OP {
    INM_VPD_ADD = 0,
    INM_VPD_REMOVE = 1
};

void RunInmVpdScript(const std::string fcdisk, INM_VPD_OP operation)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string cmd("/etc/vxagent/bin/inm_vpd.sh ");
    cmd += (operation == INM_VPD_ADD) ? "add " : "remove ";
    cmd += fcdisk.c_str();
    std::stringstream results;
    if (executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_DEBUG, "Sucessfully ran command %s\n", cmd.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", cmd.c_str(), results.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

void GetATLunPathsFromFcAdapter(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            RemotePwwnInfo_t &rpwwn, 
                            QuitFunction_t &qf, 
                            const std::string &fcadapter,
                            const bool &bshouldscan)
{
    std::stringstream msg;
    msg << "source LUN ID : " << piatluninfo.sourceLUNID << ','
        << " PIWWN : " << lpwwn.m_Pwwn.m_Name << ','
        << " ATWWN : " << rpwwn.m_Pwwn.m_Name << ','
        << " LUN Number : " << piatluninfo.applianceTargetLUNNumber << " ";

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());

    std::stringstream strAtLunNum;

    if (piatluninfo.applianceTargetLUNNumber != 0)
        strAtLunNum << std::hex << piatluninfo.applianceTargetLUNNumber << "000000000000";
    else
        strAtLunNum << std::hex << piatluninfo.applianceTargetLUNNumber;

    std::string cmd("/usr/sbin/lsdev -p ");
    cmd +=  fcadapter.c_str();
    cmd +=  " | /usr/bin/grep 'FC SCSI' | /usr/bin/awk  '{print $1}'";
    std::stringstream results;
    /*
     * Output:
     * fscsi0
     * fscsi1
     * ...
     */

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string fctarg;
            results >> fctarg;

            if (fctarg.empty())
                continue;

            std::string cmd1("/usr/sbin/lsdev -p ");
            cmd1 +=  fctarg.c_str();
            cmd1 +=  " | /usr/bin/grep 'Available' | /usr/bin/grep 'FC SCSI Disk Drive' | /usr/bin/awk '{print $1}'";
            std::stringstream results1;
            /*
             * Output:
             * hdisk0 
             * hdisk1
             * ...
             */

            if (executePipe(cmd1, results1))
            {
                bool bFoundATLun = false;
                while (!results1.eof() && !bFoundATLun)
                {
                    std::string fcdisk;
                    results1 >> fcdisk;

                    if (fcdisk.empty())
                        continue;

                    std::string cmd2 ("/usr/sbin/lsattr -El ");
                    cmd2 +=  fcdisk.c_str();
                    cmd2 += " | /usr/bin/egrep 'ww_name|lun_id' | /usr/bin/awk '{print $2}' | /usr/bin/awk -Fx '{print $2}'";
                    std::stringstream results2;
                    /*
                     * Output:
                     * 1000000000000
                     * 10000000C928075B
                     */

                    if (executePipe(cmd2, results2))
                    {
                        while (!results2.eof())
                        {
                            std::string rport;
                            std::string lun_id;
                            RemotePwwnInfo_t rportinfo;

                            results2 >> lun_id;
                            results2 >> rport;
                            std::string fcdiskPath("/dev/");
                            fcdiskPath += fcdisk.c_str();

                            if (rport.length() == 16)
                            {
                                DebugPrintf(SV_LOG_DEBUG, "rport %s\n", rport.c_str());
                                FormatWwn(LSCFG, rport, rportinfo.m_FormattedName, &rportinfo.m_Pwwn.m_Type);

                                if (!rportinfo.m_FormattedName.empty())
                                {
                                    if ((rportinfo.m_FormattedName.compare(rpwwn.m_FormattedName.c_str()) == 0 ) &&
                                        (lun_id.compare(strAtLunNum.str()) == 0))
                                    {
                                        if (bshouldscan)
                                        {
                                            RunInmVpdScript(fcdisk, INM_VPD_ADD);

                                            E_LUNMATCHSTATE lunmatchstate = GetLunMatchStateForDisk(fcdiskPath, INMATLUNVENDOR,
                                                                        piatluninfo.applianceTargetLUNName,
                                                                        piatluninfo.applianceTargetLUNNumber);

                                            if (LUN_NAME_MATCHED == lunmatchstate)
                                            {
                                                DebugPrintf(SV_LOG_DEBUG, "For %s, found matching AT Lun device %s\n",
                                                                          msg.str().c_str(), fcdiskPath.c_str());


                                                if (GetVpdMatchStateForDisk(fcdisk, piatluninfo.applianceTargetLUNName))
                                                {
                                                    rpwwn.m_ATLunState = LUNSTATE_VALID;
                                                    rpwwn.m_ATLunPath = fcdiskPath;
                                                    bFoundATLun = true;
                                                }
                                                else
                                                {
                                                    DebugPrintf(SV_LOG_ERROR, "For %s, AT Lun device %s not found in CuVPD\n",
                                                                          msg.str().c_str(), fcdiskPath.c_str());

                                                    RunInmVpdScript(fcdisk, INM_VPD_REMOVE);

                                                    std::string cmd3("/usr/sbin/rmdev -l ");
                                                    cmd3 +=  fcdisk.c_str();
                                                    cmd3 +=  " -d ";
                                                    std::stringstream results3;
                                                    if (executePipe(cmd3, results3))
                                                    {
                                                        DebugPrintf(SV_LOG_DEBUG, "Removed stale AT Lun device using %s\n", cmd3.c_str());

                                                    }
                                                    else
                                                    {
                                                        DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", cmd3.c_str(), results3.str().c_str());
                                                    }
                                                }
                               
                                            }
                                            else
                                            {
                                                DebugPrintf(SV_LOG_ERROR, "For %s, AT Lun device %s not matched\n",
                                                                      msg.str().c_str(), fcdiskPath.c_str());

                                                RunInmVpdScript(fcdisk, INM_VPD_REMOVE);

                                                std::string cmd4("/usr/sbin/rmdev -l ");
                                                cmd4 +=  fcdisk.c_str();
                                                cmd4 +=  " -d ";
                                                std::stringstream results4;
                                                if (executePipe(cmd4, results4))
                                                {
                                                    DebugPrintf(SV_LOG_DEBUG, "Removed stale AT Lun device using %s\n", cmd4.c_str());

                                                }
                                                else
                                                {
                                                    DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", cmd4.c_str(), results4.str().c_str());
                                                }
                                            }
                                        }
                                        else
                                        {
                                            rpwwn.m_ATLunState = LUNSTATE_VALID;
                                            rpwwn.m_ATLunPath = fcdiskPath;
                                            bFoundATLun = true;
                                        }
                                    }
                                    else if (rportinfo.m_FormattedName.compare(rpwwn.m_FormattedName.c_str()) == 0 )
                                    {
                                        std::string atlunName;

                                        GetNameFromVpdForDisk(fcdisk, atlunName);

                                        if (atlunName.size() > 0 )
                                        {
                                            E_LUNMATCHSTATE lunmatchstate = GetLunMatchStateForDisk(fcdiskPath, INMATLUNVENDOR,
                                                                             atlunName, 0);

                                            if (LUN_NAME_MATCHED != lunmatchstate)
                                            {
                                                DebugPrintf(SV_LOG_ERROR, "AT Lun device %s and name %s is stale, needs cleanup.\n",
                                                                      fcdiskPath.c_str(), atlunName.c_str());

                                                RunInmVpdScript(fcdisk, INM_VPD_REMOVE);

                                                std::string cmd3("/usr/sbin/rmdev -l ");
                                                cmd3 +=  fcdisk.c_str();
                                                cmd3 +=  " -d ";
                                                std::stringstream results3;
                                                if (executePipe(cmd3, results3))
                                                {
                                                    DebugPrintf(SV_LOG_DEBUG, "Removed stale AT Lun device using %s\n", cmd3.c_str());

                                                }
                                                else
                                                {
                                                    DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", 
                                                                                cmd3.c_str(), results3.str().c_str());
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd2.c_str());
                        break;
                    }
                }

                if (!bFoundATLun && bshouldscan)
                {
                    RunATLunDisocovery(fcadapter);
                }
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd1.c_str());
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void GetATLunPathsFromIScsiAdapter(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            RemotePwwnInfo_t &rpwwn, 
                            QuitFunction_t &qf, 
                            const std::string &adapter,
                            const bool &bshouldscan)
{
    std::stringstream msg;
    msg << "source LUN ID : " << piatluninfo.sourceLUNID << ','
        << " PIWWN : " << lpwwn.m_Pwwn.m_Name << ','
        << " ATWWN : " << rpwwn.m_Pwwn.m_Name << ','
        << " LUN Number : " << piatluninfo.applianceTargetLUNNumber << " ";

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());

    std::stringstream strAtLunNum;

    if (piatluninfo.applianceTargetLUNNumber != 0)
        strAtLunNum << std::hex << piatluninfo.applianceTargetLUNNumber << "000000000000";
    else
        strAtLunNum << std::hex << piatluninfo.applianceTargetLUNNumber;

    std::string cmd("/usr/sbin/lsdev -p ");
    cmd +=  adapter.c_str();
    cmd +=  " | /usr/bin/grep 'iSCSI Disk Drive' | /usr/bin/awk  '{print $1}'";
    std::stringstream results;
    /*
     * Output:
     * hdisk0 
     * hdisk1
     * ...
     */

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            bool bFoundATLun = false;
            std::string disk;
            results >> disk;

            if (disk.empty())
                continue;

            std::string cmd1 ("/usr/sbin/lsattr -El ");
            cmd1 +=  disk.c_str();
            cmd1 += " | /usr/bin/egrep 'target_name' | /usr/bin/awk '{print $2}''";
            std::stringstream results1;
            /*
             * Output:
             * iqn.1992-08.com.ibm:aix17-196:0839230
             */

            std::string cmd2 ("/usr/sbin/lsattr -El ");
            cmd2 +=  disk.c_str();
            cmd2 += " | /usr/bin/egrep 'lun_id' | /usr/bin/awk '{print $2}' | /usr/bin/awk -Fx '{print $2}'";
            std::stringstream results2;
            /*
             * Output:
             * 1000000000000
             */

            if (executePipe(cmd1, results1) && executePipe(cmd2, results2))
            {
                while (!results1.eof() && !results2.eof())
                {
                    std::string rport;
                    std::string lun_id;
                    RemotePwwnInfo_t rportinfo;

                    results1 >> rport;
                    results2 >> lun_id;

                    if (rport.empty() || lun_id.empty())
                        continue;

                    std::string diskPath("/dev/");
                    diskPath += disk.c_str();

                    DebugPrintf(SV_LOG_DEBUG, "rport %s\n", rport.c_str());
                    FormatWwn(LSCFG, rport, rportinfo.m_FormattedName, &rportinfo.m_Pwwn.m_Type);

                    if (!rportinfo.m_FormattedName.empty())
                    {
                        if ((rportinfo.m_FormattedName.compare(rpwwn.m_FormattedName.c_str()) == 0 ) &&
                            (lun_id.compare(strAtLunNum.str()) == 0))
                        {
                            if (bshouldscan)
                            {
                                RunInmVpdScript(disk, INM_VPD_ADD);

                                E_LUNMATCHSTATE lunmatchstate = GetLunMatchStateForDisk(diskPath, INMATLUNVENDOR,
                                                            piatluninfo.applianceTargetLUNName,
                                                            piatluninfo.applianceTargetLUNNumber);

                                if (LUN_NAME_MATCHED == lunmatchstate)
                                {
                                    DebugPrintf(SV_LOG_DEBUG, "For %s, found matching AT Lun device %s\n",
                                                              msg.str().c_str(), diskPath.c_str());


                                    if (GetVpdMatchStateForDisk(disk, piatluninfo.applianceTargetLUNName))
                                    {
                                        rpwwn.m_ATLunState = LUNSTATE_VALID;
                                        rpwwn.m_ATLunPath = diskPath;
                                        bFoundATLun = true;
                                    }
                                    else
                                    {
                                        DebugPrintf(SV_LOG_ERROR, "For %s, AT Lun device %s not found in CuVPD\n",
                                                              msg.str().c_str(), diskPath.c_str());

                                        RunInmVpdScript(disk, INM_VPD_REMOVE);

                                        std::string cmd3("/usr/sbin/rmdev -l ");
                                        cmd3 +=  disk.c_str();
                                        cmd3 +=  " -d ";
                                        std::stringstream results3;
                                        if (executePipe(cmd3, results3))
                                        {
                                            DebugPrintf(SV_LOG_DEBUG, "Removed stale AT Lun device using %s\n", cmd3.c_str());

                                        }
                                        else
                                        {
                                            DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", cmd3.c_str(), results3.str().c_str());
                                        }
                                    }
                   
                                }
                                else
                                {
                                    DebugPrintf(SV_LOG_ERROR, "For %s, AT Lun device %s not matched\n",
                                                          msg.str().c_str(), diskPath.c_str());

                                    RunInmVpdScript(disk, INM_VPD_REMOVE);

                                    std::string cmd4("/usr/sbin/rmdev -l ");
                                    cmd4 +=  disk.c_str();
                                    cmd4 +=  " -d ";
                                    std::stringstream results4;
                                    if (executePipe(cmd4, results4))
                                    {
                                        DebugPrintf(SV_LOG_DEBUG, "Removed stale AT Lun device using %s\n", cmd4.c_str());

                                    }
                                    else
                                    {
                                        DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", cmd4.c_str(), results4.str().c_str());
                                    }
                                }
                            }
                            else
                            {
                                rpwwn.m_ATLunState = LUNSTATE_VALID;
                                rpwwn.m_ATLunPath = diskPath;
                                bFoundATLun = true;
                            }
                        }
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s %s\n", cmd1.c_str(), cmd2.c_str());
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void DeleteOfflineATLunPathsFromAdapter(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            RemotePwwnInfo_t &rpwwn, 
                            QuitFunction_t &qf, 
                            const std::string &fcadapter)
{
    std::stringstream msg;
    msg << "source LUN ID : " << piatluninfo.sourceLUNID << ','
        << " PIWWN : " << lpwwn.m_Pwwn.m_Name << ','
        << " ATWWN : " << rpwwn.m_Pwwn.m_Name << ','
        << "AT LUN Name: " <<  piatluninfo.applianceTargetLUNName << ','
        << " LUN Number : " << piatluninfo.applianceTargetLUNNumber << " ";

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());

    std::stringstream strAtLunNum;

    if (piatluninfo.applianceTargetLUNNumber != 0)
        strAtLunNum << std::hex << piatluninfo.applianceTargetLUNNumber << "000000000000";
    else
        strAtLunNum << std::hex << piatluninfo.applianceTargetLUNNumber;

    DebugPrintf(SV_LOG_DEBUG, "rpwwn %s atLunNum %s\n", rpwwn.m_FormattedName.c_str(), strAtLunNum.str().c_str());


    std::string cmd("/usr/sbin/lsdev -p ");
    cmd +=  fcadapter.c_str();
    cmd +=  " | /usr/bin/grep 'FC SCSI' | /usr/bin/awk '{print $1}'";
    std::stringstream results;
    /*
     * Output:
     * fscsi0
     * fscsi1
     * ...
     */

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string fctarg;
            results >> fctarg;

            if (fctarg.empty())
                continue;
            std::string cmd1("/usr/sbin/lsdev -p ");
            cmd1 +=  fctarg.c_str();
            cmd1 +=  " | /usr/bin/grep 'Available' | /usr/bin/grep 'FC SCSI Disk Drive' | /usr/bin/awk '{print $1}'";
            std::stringstream results1;
            /*
             * Output:
             * hdisk0 
             * hdisk1
             * ...
             */

            if (executePipe(cmd1, results1))
            {
                bool bFoundATLun = false;
                while (!results1.eof() && !bFoundATLun)
                {
                    std::string fcdisk;
                    results1 >> fcdisk;

                    if (fcdisk.empty())
                        continue;

                    std::string cmd2 ("/usr/sbin/lsattr -El ");
                    cmd2 +=  fcdisk.c_str();
                    cmd2 += " | /usr/bin/egrep 'ww_name|lun_id' | /usr/bin/awk '{print $2}' | /usr/bin/awk -Fx '{print $2}'";
                    std::stringstream results2;
                    /*
                     * Output:
                     * 1000000000000
                     * 10000000C928075B
                     */

                    if (executePipe(cmd2, results2))
                    {
                        while (!results2.eof())
                        {
                            std::string rport;
                            std::string lun_id;
                            RemotePwwnInfo_t rportinfo;

                            results2 >> lun_id;
                            results2 >> rport;

                            if (rport.length() == 16)
                            {
                                FormatWwn(LSCFG, rport, rportinfo.m_FormattedName, &rportinfo.m_Pwwn.m_Type);

                                if (!rportinfo.m_FormattedName.empty())
                                {
                                    if ((rportinfo.m_FormattedName.compare(rpwwn.m_FormattedName.c_str()) == 0 ) && 
                                        (lun_id.compare(strAtLunNum.str()) == 0))
                                    {
                                        DebugPrintf(SV_LOG_DEBUG, "Found AT Lun at %s\n", fcdisk.c_str() );
                                        bFoundATLun = true;

                                        RunInmVpdScript(fcdisk, INM_VPD_REMOVE);

                                        std::string cmd3("/usr/sbin/rmdev -l ");
                                        cmd3 +=  fcdisk.c_str();
                                        cmd3 +=  " -d ";
                                        std::stringstream results3;
                                        if (executePipe(cmd3, results3))
                                        {
                                            DebugPrintf(SV_LOG_DEBUG, "Removed AT Lun device using %s\n", cmd3.c_str());
                                        }
                                        else
                                        {
                                            DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", cmd3.c_str(), results3.str().c_str());
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", cmd2.c_str(), results2.str().c_str());
                        break;
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", cmd1.c_str(), results1.str().c_str());
                break;
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to execute command %s. Result %s\n", cmd.c_str(), results.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void DeleteATLunFromFcHost(const PIAT_LUN_INFO &piatluninfo,
                           const LocalPwwnInfo_t &lpwwn,
                           Channels_t &channels,
                           QuitFunction_t &qf,
                           RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
}

void DeleteATLunFromScsiHost(const PIAT_LUN_INFO &piatluninfo,
                             const LocalPwwnInfo_t &lpwwn,
                             Channels_t &channels,
                             QuitFunction_t &qf,
                             RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
}
void DeleteATLunFromQla(const PIAT_LUN_INFO &piatluninfo,
                        const LocalPwwnInfo_t &lpwwn,
                        Channels_t &channels,
                        QuitFunction_t &qf,
                        RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
}

void DeleteATLunFromScli(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn,
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
}
void DeleteATLunFromLpfc(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn,
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
}
void GetATLunPathFromFcHost(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            const bool &bshouldscan,
                            Channels_t &channelnumbers,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
}
void GetATLunPathFromQla(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn,
                         const bool &bshouldscan,
                         Channels_t &channelnumbers,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
}

void GetRemotePwwnsFromFcHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /* No implementation required for AIX */
}

void GetRemotePwwnsFromScsiHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /* No implementation required for AIX */
}

void GetRemotePwwnsFromQla(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
}

void GetChannelNumbers(Channels_t &ChannelNumbers)
{
    /* No implementation required for AIX */
}

void GetRemotePwwnsFromLpfc(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /* No implementation required for AIX */
}
void GetATLunPathsFromCumulation(const PIAT_LUN_INFO &piatluninfo,
                                 const bool &bshouldscan,
                                 QuitFunction_t &qf,
                                 LocalPwwnInfos_t &lpwwns,
                                 std::set<std::string> &atlundirectpaths)
{
}
bool DeleteATLunsFromCumulation(const PIAT_LUN_INFO &piatluninfo,
                                QuitFunction_t &qf,
                                LocalPwwnInfos_t &lpwwns)
{
	return false ;
}

void GetRemotePwwnsFromScli(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /* No implementation required for AIX */
}
void GetATLunPathFromScsiHost(const PIAT_LUN_INFO &piatluninfo,
                              const LocalPwwnInfo_t &lpwwn,
                              const bool &bshouldscan,
                              Channels_t &channelnumbers,
                              QuitFunction_t &qf,
                              RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
}

void GetATLunPathFromScli(const PIAT_LUN_INFO &piatluninfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
}
void GetATLunPathFromLpfc(const PIAT_LUN_INFO &piatluninfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn)
{
    /* No implementation required for AIX */
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


SV_ULONGLONG GetATCountFromScliPI(const PIAT_LUN_INFO &piatluninfo,
                                    const LocalPwwnInfo_t &lpwwn,
                                    QuitFunction_t &qf)
{
    return 0;
}

SV_ULONGLONG GetATCountFromLpfcPI(const PIAT_LUN_INFO &piatluninfo,
                                    const LocalPwwnInfo_t &lpwwn,
                                    QuitFunction_t &qf)
{
    return 0;
}

void FcInfoLunNormalScan(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn,
                         const char *opts)
{
	// nothing to do
}

void FcInfoLunRetryScan(const PIAT_LUN_INFO &piatluninfo,
                        const LocalPwwnInfo_t &lpwwn,
                        QuitFunction_t &qf,
                        RemotePwwnInfo_t &rpwwn,
                        const char *opts)
{
	//nothing to do
}

bool IsFcInfoLunCleaned(const PIAT_LUN_INFO &piatluninfo,
                        const LocalPwwnInfo_t &lpwwn,
                        QuitFunction_t &qf,
                        const RemotePwwnInfo_t &rpwwn,
                        const char *opts)
{
	return true;
}

bool IsFcInfoLunCleanedOnRetry(const PIAT_LUN_INFO &piatluninfo,
                               const LocalPwwnInfo_t &lpwwn,
                               QuitFunction_t &qf,
                               const RemotePwwnInfo_t &rpwwn,
                               const char *opts)
{
	return true;
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
    const std::string cmd("/usr/sbin/lsdev  -Cc adapter | /usr/bin/grep 'fcs' | /usr/bin/awk '{print $1}'");
    std::stringstream results;
    /*
     * Output:
     * fcs0
     * fcs1
     * fcs2
     * ...
     */
    
    if (executePipe(cmd, results))
    {
        bool bFoundAdapter =  false;
        while (!results.eof() && !bFoundAdapter)
        {
			std::string fcadapter;
            results >> fcadapter;
            if (fcadapter.empty())
            {
                continue;
            }

            RunATLunDisocovery(fcadapter);
        }
    }
    return;
}

void GetNameFromVpdForDisk(const std::string disk, std::string &applianceTargetLUNName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    bool match = false;

    std::string cmd("/usr/bin/odmget -q name=");
    cmd += disk.c_str();
    cmd += " CuVPD | egrep 'DUMMY|inmage' | awk -F* '{print $1}'";
    std::stringstream results;

    if (executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully completed %s with result %s\n", cmd.c_str(), results.str().c_str());

        if ( results.str().size() > 0)
        {
            results >> applianceTargetLUNName;

            if (applianceTargetLUNName.compare("DUMMY_LUN_ZERO") == 0)
                applianceTargetLUNName = "DUMMY_LUN_ZERO  ";
        }

        DebugPrintf(SV_LOG_DEBUG, "Got AT Lun name %s for disk %s\n", applianceTargetLUNName.c_str(), disk.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s with result %s\n", cmd.c_str(), results.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

    return;
}
