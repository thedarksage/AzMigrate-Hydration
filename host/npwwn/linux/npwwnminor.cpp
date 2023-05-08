#include "npwwn.h"
#include "npwwnminor.h"
#include "logger.h"
#include "portable.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "event.h"
#include "inmstrcmp.h"
#include "scsicmd.h"
#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"
#include "executecommand.h"

void GetLocalPwwnInfos(LocalPwwnInfos_t &lpwwns)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool bgotwwns = true;
    
    for (int i = 0; i < NELEMS(SYSCLASSSUBDIRS); i++)
    {
        /*
        * RHEL 5: For qla and emulex
        * ======
        * [root@imits216 ~]# cat /sys/class/fc_host/host2/port_name
        * 0x10000000c9308487
        * [root@imits216 ~]# cat /sys/class/fc_host/host2/node_name
        * 0x20000000c9308487
        * 
        * RHEL 4: For emulex
        * ======
        * # cat /sys/class/scsi_host/host1/port_name
        * 0x10000000c95e0709
        *
        */
        GetWwnFromSysClass(
                           SYS_CLASSDIR, 
                           i,
                           HOST_DIRENT, 
                           (i == (NELEMS(SYSCLASSSUBDIRS)-1))? ADDRESS : NODE_FILENAME, 
                           (i == (NELEMS(SYSCLASSSUBDIRS)-1))? INITIATOR_NAME : PORT_FILENAME, 
                           lpwwns
                          );
    }

    std::string procscsidir = PROC_SCSIDIR;
    procscsidir += UNIX_PATH_SEPARATOR;
    procscsidir += QLADIR;
    /*
    * [root@imits046 ~]# grep adapter-node /proc/scsi/qla2xxx/1
    * scsi-qla1-adapter-node=200100e08b355e07;
    * [root@imits046 ~]# grep adapter-port /proc/scsi/qla2xxx/1
    * scsi-qla1-adapter-port=210100e08b355e07;
    */
    GetWwnFromProcScsi(procscsidir, NODENAMEKEY, PORTNAMEKEY, lpwwns);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void GetWwnFromSysClass(
                        const std::string &dir,
                        const int &subdirindex,
                        const std::string &hostdirent,
                        const std::string &nodefile,
                        const std::string &portfile,
                        LocalPwwnInfos_t &lpwwns
                       )
{
    std::string subdir = SYSCLASSSUBDIRS[subdirindex];
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with directory = %s, subdirectory = %s\n",
                             FUNCTION_NAME, dir.c_str(), subdir.c_str()); 

    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    std::string dirtopen = dir;
    dirtopen += UNIX_PATH_SEPARATOR;
    dirtopen += subdir;
    
    dentresult = NULL;
    dirp = opendir(dirtopen.c_str());
    boost::shared_ptr<void> dirpGuard (static_cast<void*>(0), boost::bind(closedir, dirp));
    if (dirp)
    {
        /* DebugPrintf(SV_LOG_DEBUG, "opened directory %s successfully\n", dirtopen.c_str()); */
        dentp = (struct dirent *)calloc(direntsize, 1);
        if (dentp)
        {
            while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
            {
                if (strcmp(dentp->d_name, ".") &&
                    strcmp(dentp->d_name, "..") &&
                    (0 == strncmp(dentp->d_name, hostdirent.c_str(), strlen(hostdirent.c_str()))))
                {
                    LocalPwwnInfo_t lpwwn;
                    std::string file = dirtopen;
                    file += UNIX_PATH_SEPARATOR;
                    file += dentp->d_name;
                    file += UNIX_PATH_SEPARATOR;
                    std::string nwwnfile = file;
                    nwwnfile += nodefile;
                    std::string pwwnfile = file;
                    pwwnfile += portfile;
                    GetFirstStringFromFile(pwwnfile, lpwwn.m_Pwwn.m_Name);
                    if (lpwwn.m_Pwwn.m_Name.empty())
                    {
                        DebugPrintf(SV_LOG_DEBUG, "From directory %s file %s, did not get pwwn\n", 
                                                  file.c_str(), pwwnfile.c_str());
                        continue;
                    }
                    GetFirstStringFromFile(nwwnfile, lpwwn.m_Pwwn.m_Nwwn);
                    /* DebugPrintf(SV_LOG_DEBUG, "From file %s, nwwn got is %s\n", nwwnfile.c_str(), lpwwn.m_Pwwn.m_Nwwn.c_str()); */
                    /* Nothing happens if lpwwn.m_Pwwn.m_Type gets overwritten for 
                     * pwwn */
                    /*
                     * TODO - specific check for iscsi
                     */
                    if (((lpwwn.m_Pwwn.m_Name == "<NULL>") || (lpwwn.m_Pwwn.m_Name == "(null)")) && (portfile == INITIATOR_NAME))
                    {
                        std::string cmd = "cat /etc/iscsi/initiatorname.iscsi | /bin/awk -F= '{print $2}'";
                        std::stringstream results;

                        if (executePipe(cmd, results))
                        {
                            while (!results.eof())
                            {
                                std::string initiator_name;
                                results >> initiator_name;
                                if (!initiator_name.empty())
                                {
                                    lpwwn.m_Pwwn.m_Name = initiator_name;
                                }
                            }
                        }
                    }
                    FormatWwn(PwwnSrc[subdirindex], lpwwn.m_Pwwn.m_Nwwn, lpwwn.m_Pwwn.m_FormattedNwwn, &lpwwn.m_Pwwn.m_Type);
                    /* DebugPrintf(SV_LOG_DEBUG, "From file %s, pwwn got is %s\n", pwwnfile.c_str(), lpwwn.m_Pwwn.m_Name.c_str()); */
                    FormatWwn(PwwnSrc[subdirindex], lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);
                    /*
                    DebugPrintf(SV_LOG_DEBUG, "Formatted pwwn is %s, "
                                              "Formatted nwwn is %s\n",
                                              lpwwn.m_FormattedName.c_str(),
                                              lpwwn.m_Pwwn.m_FormattedNwwn.c_str());
                    */
                    /* This pwwn check is enough as no need
                     * to nwwn check */
                    if (!lpwwn.m_FormattedName.empty())
                    {
                        /* fill the pwwn src and the pwwn number and exit */
                        std::string fmt = hostdirent;
                        fmt += LLSPEC;
                        if ((1 == sscanf(dentp->d_name, fmt.c_str(), &lpwwn.m_Pwwn.m_Number)) && 
                            (lpwwn.m_Pwwn.m_Number >= 0))
                        {
                            lpwwn.m_PwwnSrc = PwwnSrc[subdirindex];
                            DebugPrintf(SV_LOG_DEBUG, "The local pwwn %s found with host number " LLSPEC 
                                                      " source %s\n", lpwwn.m_Pwwn.m_Name.c_str(),
                                                      lpwwn.m_Pwwn.m_Number, 
                                                      PwwnSrcStr[lpwwn.m_PwwnSrc]);
                            InsertLocalPwwn(lpwwns, lpwwn);
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "The local pwwn %s found with source %s, "
                                                      "but getting its host number did not succeed\n",
                                                      lpwwn.m_Pwwn.m_Name.c_str(),
                                                      PwwnSrcStr[PwwnSrc[subdirindex]]);
                        }
                    }
                } /* end of skipping . and .. */
                memset(dentp, 0, direntsize);
            } /* end of while readdir_r */
            free(dentp);
        } /* endif of if (dentp) */
        else
        {
            DebugPrintf(SV_LOG_ERROR, "failed to allocate memory for dirent to get wwn port/node name\n");
        }
        //closedir(dirp); 
    } /* end of if (dirp) */
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "unable to open directory %s\n", dirtopen.c_str());
    }
 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}  


void GetWwnFromProcScsi(
                        const std::string &dir,
                        const std::string &nodekey,
                        const std::string &portkey,
                        LocalPwwnInfos_t &lpwwns
                       )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with directory = %s\n",
                             FUNCTION_NAME, dir.c_str()); 
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;

    dentresult = NULL; 
    dirp = opendir(dir.c_str());
    boost::shared_ptr<void> dirpGuard (static_cast<void*>(0), boost::bind(closedir, dirp));
    if (dirp)
    {
        dentp = (struct dirent *)calloc(direntsize, 1);
        if (dentp)
        {
            while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
            {
                if (strcmp(dentp->d_name, ".") &&
                    strcmp(dentp->d_name, ".."))
                {
                    std::string file = dir;
                    file += UNIX_PATH_SEPARATOR;
                    file += dentp->d_name;
                    GetWwnUsingKey(dentp->d_name, file, nodekey, portkey, lpwwns);
                } /* end of skipping . and .. */
                memset(dentp, 0, direntsize);
            } /* end of while readdir_r */
            free(dentp);
        } /* endif of if (dentp) */
        else
        {
            DebugPrintf(SV_LOG_ERROR, "failed to allocate memory for dirent to get wwn port/node name\n");
        }
        //closedir(dirp); 
    } /* end of if (dirp) */
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "unable to open directory %s\n", dir.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}  


void GetWwnUsingKey(
                    const std::string &dname,
                    const std::string &filename, 
                    const std::string &nodekey, 
                    const std::string &portkey, 
                    LocalPwwnInfos_t &lpwwns
                   )
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with file to get wwn = %s\n",
                             FUNCTION_NAME, filename.c_str()); 
    std::ifstream ifile(filename.c_str());
    if (ifile.is_open())
    {
        if (ifile.good())
        {
            while (!ifile.eof())
            {
                std::string tok;
                ifile >> tok;
                if (tok.empty())
                {
                    break;
                }
                size_t nodekeyidx = tok.find(nodekey);
                 
                if (std::string::npos != nodekeyidx)
                {
                    LocalPwwnInfo_t lpwwn;
                    /* TODO: need to check if nwwn present for iscsi ? */
                    GetWwnFromNVPair(tok, EQUALS, lpwwn.m_Pwwn.m_Nwwn);
                    RemoveLastMatchingChar(lpwwn.m_Pwwn.m_Nwwn, QLAWWNLASTCHAR);
                    /* DebugPrintf(SV_LOG_DEBUG, "From file %s, nwwn got is %s\n", filename.c_str(), lpwwn.m_Pwwn.m_Nwwn.c_str()); */
                    FormatWwn(PROCQLA, lpwwn.m_Pwwn.m_Nwwn, lpwwn.m_Pwwn.m_FormattedNwwn, &lpwwn.m_Pwwn.m_Type);
                    tok.clear();
                    ifile >> tok;
                    size_t portkeyidx = tok.find(portkey);
                    if (std::string::npos != portkeyidx)
                    {
                        /* TODO: here we need to do format conversion of wwn */
                        GetWwnFromNVPair(tok, EQUALS, lpwwn.m_Pwwn.m_Name);
                        RemoveLastMatchingChar(lpwwn.m_Pwwn.m_Name, QLAWWNLASTCHAR);
                        /* DebugPrintf(SV_LOG_DEBUG, "From file %s, pwwn got is %s\n", filename.c_str(), lpwwn.m_Pwwn.m_Name.c_str()); */
                        FormatWwn(PROCQLA, lpwwn.m_Pwwn.m_Name, lpwwn.m_FormattedName, &lpwwn.m_Pwwn.m_Type);
                    }
                    /*
                    DebugPrintf(SV_LOG_DEBUG, "From file %s, formatted pwwn is %s, "
                                              "formatted nwwn is %s\n", filename.c_str(), 
                                              lpwwn.m_FormattedName.c_str(),
                                              lpwwn.m_Pwwn.m_FormattedNwwn.c_str());
                    */
                    if (!lpwwn.m_Pwwn.m_Name.empty() && !lpwwn.m_Pwwn.m_Nwwn.empty() 
                        && !lpwwn.m_FormattedName.empty())
                    {
                        if ((1 == sscanf(dname.c_str(), LLSPEC, &lpwwn.m_Pwwn.m_Number)) && 
                            (lpwwn.m_Pwwn.m_Number >= 0))
                        {
                            lpwwn.m_PwwnSrc = PROCQLA;
                            DebugPrintf(SV_LOG_DEBUG, "The local pwwn %s found with host number " LLSPEC 
                                                      " source %s\n", lpwwn.m_Pwwn.m_Name.c_str(),
                                                      lpwwn.m_Pwwn.m_Number, 
                                                      PwwnSrcStr[lpwwn.m_PwwnSrc]);
                            InsertLocalPwwn(lpwwns, lpwwn);
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "The local pwwn %s found with source %s, "
                                                      "but getting its host number did not succeed\n",
                                                      lpwwn.m_Pwwn.m_Name.c_str(),
                                                      PwwnSrcStr[PROCQLA]);
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "did not properly get nwwn and pwwn from file %s\n", filename.c_str());
                    }
                    break;
                }
            }
        }
        else
        {
            /* TODO: should this be warning */
            DebugPrintf(SV_LOG_ERROR, "failed to open file %s properly to get wwn information\n", filename.c_str());
        }
        ifile.close();
    }
    else
    {
        /* TODO: should this be warning */
        DebugPrintf(SV_LOG_ERROR, "failed to open file %s to get wwn information\n", filename.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void GetRemotePwwnsFromFcInfo(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /*
    * Should not come here
    */
}


void GetRemotePwwnsFromScli(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /*
    * Should not come here
    */
}


void GetRemotePwwnsFromLpfc(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /*
    * Should not come here
    */
}


void GetRemotePwwnsFromFcHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    const std::string FCPTARGETROLE = "FCP Target";
    MultiRemotePwwnData_t mrpwwns;
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    std::string dir = SYS_CLASSDIR;
    dir += UNIX_PATH_SEPARATOR;
    dir += FCRPORTDIR;
    std::stringstream sslpseq;
    sslpseq << pLocalPwwnInfo->m_Pwwn.m_Number;
    std::string rportdirprefix = RPORT;
    rportdirprefix += RPORTLPSEQSEP;
    rportdirprefix += sslpseq.str();
    rportdirprefix += LPSEQRPSEQSEP;



    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
    dentresult = NULL;

    dirp = opendir(dir.c_str());
    boost::shared_ptr<void> dirpGuard (static_cast<void*>(0), boost::bind(closedir, dirp));

    if (dirp)
    {
       dentp = (struct dirent *)calloc(direntsize, 1);

       if (dentp)
       {
           while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
           {
               if (strcmp(dentp->d_name, ".") &&
                   strcmp(dentp->d_name, "..") && 
                   (0 == strncmp(dentp->d_name, rportdirprefix.c_str(), strlen(rportdirprefix.c_str()))))
               {
                   std::string file = dir;
                   file += UNIX_PATH_SEPARATOR;
                   file += dentp->d_name;
                   file += UNIX_PATH_SEPARATOR;
                   std::string portfile = file;
                   portfile += PORT_FILENAME;
                   std::string rport;
                   /* [root@imits216 ~]# cat /sys/class/fc_remote_ports/rport-2\:0-4/port_name
                    * 0x26000001553507c1
                    */
                   /* TODO: here we need to do format conversion of wwn */
                   GetFirstStringFromFile(portfile, rport);
                   
                   if (!rport.empty())
                   {
                       std::string tgtidfile = file;
                       tgtidfile += TARGETIDFILENAME;
                       std::string rolesfile = file;
                       rolesfile += ROLESFILENAME;
                       std::string statefile = file;
                       statefile += FCHOSTSTATEFILE;
                       RemotePwwnInfo_t rpwwn;
                       FormatWwn(SYSCLASSFCHOST, rport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                       rpwwn.m_Pwwn.m_Name = rport;
                       /* 
                       * [root@imits216 ~]# cat /sys/class/fc_remote_ports/rport-2\:0-4/scsi_target_id
                       * 0
                       /* TODO: since there is no preceding 0x above, assuming target ID to decimal */
                       GetFirstInpFromFile(tgtidfile, E_DEC, &rpwwn.m_Pwwn.m_Number);
                       /* TODO: There may be other entries in remote ports
                        * that have target id >= 0; but for now first match
                        * is enough since device will be not visible and we do
                        * full scan and there every thing is recalculated;
                        * But this should be avoided and how ? */
                       std::string role;
                       GetFirstLineFromFile(rolesfile, role);
                       std::string state;
                       GetFirstLineFromFile(statefile, state);

                       if ((rpwwn.m_Pwwn.m_Number >= 0) && !rpwwn.m_FormattedName.empty() && 
                           (role == FCPTARGETROLE) && (state == FCHOSTONLINESTATE))
                       {
                           InsertRemotePwwnData(mrpwwns, rpwwn);
                       }
                   }
               } /* end of skipping . and .. */
               memset(dentp, 0, direntsize);
           } /* end of while readdir_r */
           free(dentp);

           ConstMultiRemotePwwnDataIter_t mriter = mrpwwns.begin();
           for ( /* empty */ ; mriter != mrpwwns.end(); mriter++)
           {
               const RemotePwwnData_t &rdata = mriter->second;
               ConstRemotePwwnDataIter_t riter = std::max_element(rdata.begin(), rdata.end(), RemotePwwnInfoComp());
               if (riter != rdata.end())
               {
                   InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, *riter);
               }
           }
       } /* endif of if (dentp) */
       else
       {
           DebugPrintf(SV_LOG_ERROR, "failed to allocate memory for dirent to get wwn port/node name\n");
       }
       //closedir(dirp); 
    } /* end of if (dirp) */
    else
    {
        DebugPrintf(SV_LOG_ERROR, "failed to open directory %s to get remote pwwns from fc host\n", dir.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
}


void GetRemotePwwnsFromScsiHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    /*
    [root@imits142 bin]# cat /proc/scsi/lpfc/2
    lpfc0t00 DID 010f00 WWPN 26:00:00:01:55:35:07:c1 WWNN 25:00:00:01:55:35:07:c1
    lpfc0t01 DID 010600 WWPN 26:01:00:01:55:35:07:c1 WWNN 25:00:00:01:55:35:07:c1
    [root@imits142 bin]#
    */

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
    std::stringstream sslpseq;
    sslpseq << pLocalPwwnInfo->m_Pwwn.m_Number;
    std::string file = PROCLPFCDIR;
    file += UNIX_PATH_SEPARATOR;
    file += sslpseq.str();

    std::ifstream ifile(file.c_str());
    if (ifile.is_open())
    {
        if (ifile.good())
        {
            while (!ifile.eof())
            {
                std::string lpfc, did, num, wwpnlbl, rport, wwnnlbl, nwwn;
                ifile >> lpfc >> did >> num >> wwpnlbl >> rport >> wwnnlbl >> nwwn;
                if (lpfc.empty())
                {
                    break;
                }
                /* TODO: should we check for all labels ? 
                 * Also this file might contain additional lines; the 
                 * below comparisions should filter them */
                if ((WWPNLABEL == wwpnlbl) && (WWNNLABEL == wwnnlbl) && !rport.empty())
                {
                    RemotePwwnInfo_t rpwwn;
                    FormatWwn(SYSCLASSSCHOST, rport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                    rpwwn.m_Pwwn.m_Name = rport;
                    size_t tpos = lpfc.rfind(TIDCHAR);
                    if ((tpos != std::string::npos) && (tpos < (lpfc.length() - 1)))
                    {
                        tpos++;
                        std::string stid = lpfc.substr(tpos);
                        std::stringstream sstid(stid);
                        sstid >> rpwwn.m_Pwwn.m_Number;
                        if ((rpwwn.m_Pwwn.m_Number >= 0) && !rpwwn.m_FormattedName.empty())
                        {
                            InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, rpwwn);
                        }
                    }
                }
            }
        }
        ifile.close();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
}


void GetRemotePwwnsFromQla(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
    std::stringstream sslpseq;
    sslpseq << pLocalPwwnInfo->m_Pwwn.m_Number;
    std::string filename = PROC_SCSIDIR;
    filename += UNIX_PATH_SEPARATOR;
    filename += QLADIR;
    filename += UNIX_PATH_SEPARATOR;
    filename += sslpseq.str();

    /*
    * SCSI Device Information:
    * scsi-qla1-adapter-node=200100e08b355e07;
    * scsi-qla1-adapter-port=210100e08b355e07;
    * scsi-qla1-target-0=26000001553507c1;
    */

    std::ifstream ifile(filename.c_str());
    if (ifile.is_open())
    {
        if (ifile.good())
        {
            while (!ifile.eof())
            {
                std::string tok;
                ifile >> tok;
                if (tok.empty())
                {
                    break;
                }
                /* TODO: should we also grep for scsi-qla ? */
                size_t tgtkeyidx = tok.find(TGTKEY);
                 
                if (std::string::npos != tgtkeyidx)
                {
                    std::string rport;
                    GetWwnFromNVPair(tok, EQUALS, rport);
                    if (!rport.empty())
                    {
                        /* TODO: should we check mandatory ';' at last 
                         * in which case RemoveLastMatchingChar should
                         * return success (removed last matching char) and failure */
                        RemoveLastMatchingChar(rport, QLAWWNLASTCHAR);
                        RemotePwwnInfo_t rpwwn;
                        FormatWwn(PROCQLA, rport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                        rpwwn.m_Pwwn.m_Name = rport;
                        std::string fmt = TGTKEY;
                        fmt += LLSPEC;
                        if (!rpwwn.m_FormattedName.empty() &&
                            (1 == sscanf(tok.c_str() + tgtkeyidx, fmt.c_str(), &rpwwn.m_Pwwn.m_Number)) && 
                            (rpwwn.m_Pwwn.m_Number >= 0))
                        {
                            InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, rpwwn);
                        }
                    }
                }
            }
        }
        ifile.close();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
}

void GetRemotePwwnsFromIScsiHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());

    MultiRemotePwwnData_t mrpwwns;

    std::stringstream sshostnum;
    sshostnum << pLocalPwwnInfo->m_Pwwn.m_Number;

    /*
     * /sys/devices/platform/host6/
     */
    std::string hostDir = SYS_DEVSDIR;
    hostDir += UNIX_PATH_SEPARATOR;
    hostDir += HOST;
    hostDir += sshostnum.str();

    DIR *hostDirp;
    struct dirent *hostDentp, *hostDentResult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;

    hostDentResult = NULL;

    hostDirp = opendir(hostDir.c_str());
    boost::shared_ptr<void> hostDirpGuard (static_cast<void*>(0), boost::bind(closedir, hostDirp));

    if (hostDirp)
    {
       hostDentp = (struct dirent *)calloc(direntsize, 1);

       if (hostDentp)
       {
           while ((0 == readdir_r(hostDirp, hostDentp, &hostDentResult)) && hostDentResult)
           {
               std::string sessionprefix = SESSION;

               if (strcmp(hostDentp->d_name, ".") &&
                   strcmp(hostDentp->d_name, "..") && 
                   (0 == strncmp(hostDentp->d_name, sessionprefix.c_str(), strlen(sessionprefix.c_str()))))
               {
                   /*
                    * /sys/class/iscsi_session/session3/targetname
                    */
                   std::string targetFile = SYS_CLASSDIR;
                   targetFile += UNIX_PATH_SEPARATOR;
                   targetFile += ISCSI_SESSIONDIR;
                   targetFile += UNIX_PATH_SEPARATOR;
                   targetFile += hostDentp->d_name;
                   targetFile += UNIX_PATH_SEPARATOR;
                   targetFile += TARGET_NAME;

                   DebugPrintf(SV_LOG_DEBUG, "targetname %s \n", targetFile.c_str());

                   std::string rport, ip, target;

                   GetFirstStringFromFile(targetFile, rport);
                   
                   if (!rport.empty())
                   {
                       /* 
                        *  /sys/devices/platform/host6/session3
                        */
                        std::string sessionDir = hostDir;
                        sessionDir += UNIX_PATH_SEPARATOR;
                        sessionDir += hostDentp->d_name;

                        DIR *sessionDirp;
                        DebugPrintf(SV_LOG_DEBUG, "sessionDir %s\n", sessionDir.c_str());

                        sessionDirp = opendir(sessionDir.c_str());
                        boost::shared_ptr<void> sessionDirpGuard (static_cast<void*>(0), boost::bind(closedir, sessionDirp));

                        if (sessionDirp)
                        {
                            struct dirent *sessionDentp, *sessionDentResult;
                            sessionDentp = (struct dirent *)calloc(direntsize, 1);

                            if (sessionDentp)
                            {
                                while ((0 == readdir_r(sessionDirp, sessionDentp, &sessionDentResult)) && sessionDentResult)
                                {
                                    std::string connectionprefix = CONNECTION;
                                    std::string targetprefix = "target";

                                    if (strcmp(sessionDentp->d_name, ".") &&
                                       strcmp(sessionDentp->d_name, "..") && 
                                       (0 == strncmp(sessionDentp->d_name, connectionprefix.c_str(), strlen(connectionprefix.c_str()))))
                                    {
                                        /*
                                         * /sys/class/iscsi_connection/connection1\:0/address
                                         */

                                        std::string addressFile = SYS_CLASSDIR;
                                        addressFile += UNIX_PATH_SEPARATOR;
                                        addressFile += ISCSI_CONNECTIONDIR;
                                        addressFile += UNIX_PATH_SEPARATOR;
                                        addressFile += sessionDentp->d_name;
                                        addressFile += UNIX_PATH_SEPARATOR;
                                        addressFile += ADDRESS;

                                        DebugPrintf(SV_LOG_DEBUG, "addressFile %s\n", addressFile.c_str());
                                        try {

                                            GetFirstStringFromFile(addressFile, ip);
                                        }
                                        catch(...)
                                        {
                                            DebugPrintf(SV_LOG_ERROR, "Found file read failed - %s\n", addressFile.c_str()) ;
                                            continue;  
                                        }
                                    }
                                    else if (strcmp(sessionDentp->d_name, ".") &&
                                       strcmp(sessionDentp->d_name, "..") && 
                                       (0 == strncmp(sessionDentp->d_name, targetprefix.c_str(), strlen(targetprefix.c_str()))))
                                    {
                                        /*
                                         * /sys/devices/platform/host6/session3/target6:0:0
                                         */
                                        target = sessionDentp->d_name;

                                        DebugPrintf(SV_LOG_DEBUG, "target %s\n", target.c_str());

                                    }

                                    memset(sessionDentp, 0, direntsize);
                                }
                                free(sessionDentp);
                            }
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, "session dir %s open failed\n", sessionDir.c_str());
                        }
                    }

                    RemotePwwnInfo_t rpwwn;
                    FormatWwn(SYSCLASSISCSIHOST, rport, rpwwn.m_FormattedName, &rpwwn.m_Pwwn.m_Type);
                    rpwwn.m_Pwwn.m_Name = rport;
                    rpwwn.m_Pwwn.m_Nwwn = ip;

                    DebugPrintf(SV_LOG_DEBUG, "m_FormattedName %s m_Nwwn %s\n", rpwwn.m_FormattedName.c_str(), rpwwn.m_Pwwn.m_Nwwn.c_str());

                    const std::string TOKEN = ":";
                    size_t channelPos = target.find(TOKEN);
                    size_t targetPos = target.find(TOKEN, channelPos+1);

                    sscanf(target.substr(channelPos+1, targetPos - channelPos).c_str(),  LLSPEC ,&rpwwn.m_LocalChannelNumber);
                    sscanf(target.substr(targetPos+1, target.length()).c_str(), LLSPEC , &rpwwn.m_Pwwn.m_Number);

                    DebugPrintf(SV_LOG_DEBUG, "m_LocalChannelNumber " LLSPEC " m_Number"  LLSPEC "\n", rpwwn.m_LocalChannelNumber, rpwwn.m_Pwwn.m_Number);

                    if (!rpwwn.m_FormattedName.empty() && !rpwwn.m_Pwwn.m_Nwwn.empty())
                    {
                        InsertRemotePwwnData(mrpwwns, rpwwn);
                    }
               } /* end of skipping . and .. */
               memset(hostDentp, 0, direntsize);
           } /* end of while readdir_r */
           free(hostDentp);

           ConstMultiRemotePwwnDataIter_t mriter = mrpwwns.begin();
           for ( /* empty */ ; mriter != mrpwwns.end(); mriter++)
           {
               const RemotePwwnData_t &rdata = mriter->second;
               ConstRemotePwwnDataIter_t riter = std::max_element(rdata.begin(), rdata.end(), RemotePwwnInfoComp());
               if (riter != rdata.end())
               {
                   InsertRemotePwwn(pLocalPwwnInfo->m_RemotePwwnInfos, *riter);
               }
           }
       } /* endif of if (dentp) */
       else
       {
           DebugPrintf(SV_LOG_ERROR, "failed to allocate memory for dirent to get wwn port/node name\n");
       }
       //closedir(dirp); 
    } /* end of if (dirp) */
    else
    {
        DebugPrintf(SV_LOG_ERROR, "failed to open directory %s to get remote pwwns from iscsi host\n", hostDir.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s for pwwn %s\n", FUNCTION_NAME, pLocalPwwnInfo->m_Pwwn.m_Name.c_str());
}

void GetChannelNumbers(Channels_t &ChannelNumbers)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string cmd("cat /proc/scsi/scsi | awk \'{ for (i = 1; i < NF; i++) if ($i == \"Channel:\") { j=i+1; printf(\"%s %s\\n\", $i,$j); }}\' | sort -n -k2 -u");

    std::stringstream results;
    /* 
     * [root@imits216 ~]# cat /proc/scsi/scsi | awk '{ for (i = 1; i < NF; i++) if ($i == "Channel:") { j=i+1; printf("%s %s\n", $i,$j); }}' | sort -n -k2 -u
     * Channel: 00
     * Channel: 01
    */

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string channel;
            SV_LONGLONG channelnumber = -1;
            results >> channel >> channelnumber;
            if (channel.empty())
            {
                break;
            }
            if (channelnumber >= 0)
            {
                DebugPrintf(SV_LOG_DEBUG, "collecting channel number " LLSPEC "\n", channelnumber);
                ChannelNumbers.push_back(channelnumber);
            }
        }
    }
    else
    {
        /* This has to be error since this is common command across linuses */
        DebugPrintf(SV_LOG_ERROR, "failed to execute command %s\n", cmd.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


void GetATLunPathFromFcHost(const PIAT_LUN_INFO &piatluninfo,
                            const LocalPwwnInfo_t &lpwwn,
                            const bool &bshouldscan,
                            Channels_t &channelnumbers,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn)
{
    GetATLunPathFromPIATWwn(piatluninfo, lpwwn, bshouldscan, 
                            channelnumbers, qf, rpwwn);
}


void GetATLunPathFromScsiHost(const PIAT_LUN_INFO &piatluninfo,
                              const LocalPwwnInfo_t &lpwwn,
                              const bool &bshouldscan,
                              Channels_t &channelnumbers,
                              QuitFunction_t &qf,
                              RemotePwwnInfo_t &rpwwn)
{
    GetATLunPathFromPIATWwn(piatluninfo, lpwwn, bshouldscan, 
                            channelnumbers, qf, rpwwn);
}


void GetATLunPathFromQla(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn,
                         const bool &bshouldscan,
                         Channels_t &channelnumbers,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
    GetATLunPathFromPIATWwn(piatluninfo, lpwwn, bshouldscan, 
                            channelnumbers, qf, rpwwn);
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

void GetATLunPathFromPIATWwn(const PIAT_LUN_INFO &piatluninfo,
                             const LocalPwwnInfo_t &lpwwn, 
                             const bool &bshouldscan,
                             Channels_t &channelnumbers,
                             QuitFunction_t &qf,
                             RemotePwwnInfo_t &rpwwn)
{
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI " << lpwwn.m_Pwwn.m_Name
        << ", AT " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID " << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    if (channelnumbers.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "There are no channel numbers discovered on system." 
                                  "For source LUN ID %s\n", 
                                  piatluninfo.sourceLUNID.c_str());
    }
    else if (!bneedtoquit)
    {
        /* This also has to match the product and vendor */
        UpdateATLunState(piatluninfo, INMATLUNVENDOR, lpwwn,
                         channelnumbers.size(), &channelnumbers[0], rpwwn);
        // Sunil: Goes over all channels and tries to find the lun, looks-up in
        // /sys/class/scsi_device, /dev, does lun open/scsi-inquiry/proc-lookup,
        // updates lun-state (valid, hidden, needsclenup, notvisible).

        DebugPrintf(SV_LOG_DEBUG, "For %s, the at lun state is %s before cleanup process\n", msg.str().c_str(), 
                                  LunStateStr[rpwwn.m_ATLunState]);

        if (LUNSTATE_VALID == rpwwn.m_ATLunState) {
           // This means lun is visible on remote side. We will check if it is
           // available locally as well.
           DebugPrintf(SV_LOG_DEBUG, "for %s, AT lun is visible without any scan\n", msg.str().c_str());
           DebugPrintf(SV_LOG_DEBUG, "%s: Remote scan channel: %d , Local scan channel: %d\n",
                                         FUNCTION_NAME, rpwwn.m_ChannelNumber, rpwwn.m_LocalChannelNumber); 
           if (rpwwn.m_LocalMatchState == VENDOR_LUN_MATCH) {
               DebugPrintf(SV_LOG_DEBUG, "%s: AT lun %s already present on host (locally and remotely)\n",
                                         FUNCTION_NAME, piatluninfo.applianceTargetLUNName.c_str());
           }
           if (rpwwn.m_LocalMatchState == VENDOR_MATCH) {
               DebugPrintf(SV_LOG_DEBUG, "%s: AT lun %s present remotely but on host has a stale entry. Needs cleanup\n",
                                         FUNCTION_NAME, piatluninfo.applianceTargetLUNName.c_str());
               rpwwn.m_ATLunState = LUNSTATE_NEEDSCLEANUP;
               // Channel Num is set as state is LUNSTATE_VALID
           }
           if (rpwwn.m_LocalMatchState == NO_MATCH) {
               DebugPrintf(SV_LOG_ERROR, "%s: AT lun %s present remotely but not on host.\n",
                                         FUNCTION_NAME, piatluninfo.applianceTargetLUNName.c_str());
           }
        }

        if ((bshouldscan == false) && (rpwwn.m_RemoteNoMatch == 0)) {
            // This is deletion code-path. If lun state is "NEEDSCLEANUP" set due to
            // conflicting entry at remote side, we simply set state to "NOTVISIBLE"
	    // and channel to -1. In actual lun deletion code, we check if
            // rpwwn.m_RemoteNoMatch = 0 (which indicates conflicting entry is present at
            // remote side) and we dont delete this entry. Seting state to "NOTVISIBLE"
            // ensures that this AT lun is not counted for deletion and that delete
            // returns success.
            //
            // Later "Lun addition" will delete this entry.
            DebugPrintf(SV_LOG_DEBUG, "%s: Conflict seen in deletion code path, NEEDSCLEANUP state, LocalMatchState: %d\n",
                                         FUNCTION_NAME, rpwwn.m_LocalMatchState); 
            DebugPrintf(SV_LOG_DEBUG, "%s: Remote scan channel: %d , Local scan channel: %d\n",
                                         FUNCTION_NAME, rpwwn.m_ChannelNumber, rpwwn.m_LocalChannelNumber); 
            rpwwn.m_ATLunState = LUNSTATE_NOTVISIBLE;
            rpwwn.m_ChannelNumber = -1;
        }

        // Sunil: During deletion lun may not be in VALID state as open/scsi-inquiry may fail,
        // but /proc lookup will succeed.
        if (LUNSTATE_VALID == rpwwn.m_ATLunState)
        {
            // Sunil: Lun already present/visible through one of the channels. No need of scan
            DebugPrintf(SV_LOG_DEBUG, "for %s, valid at lun is visible without any scan\n", msg.str().c_str());
        }
        else if (bshouldscan) // Sunil: In deletion code path, this will not be executed
        {
            if ((LUNSTATE_INTERMEDIATE == rpwwn.m_ATLunState) || // Sunil: if lun is hidden or stale from any of the channel, do cleanup
                (LUNSTATE_NEEDSCLEANUP == rpwwn.m_ATLunState))
            {
                DebugPrintf(SV_LOG_ERROR, "For %s, cleanup is needed before scanning since "
                                          "the at lun state is %s\n", msg.str().c_str(), 
                                          LunStateStr[rpwwn.m_ATLunState]);
                DeleteATLunFromPIATWwn(piatluninfo, lpwwn, qf, rpwwn);
            }

            if (LUNSTATE_NOTVISIBLE == rpwwn.m_ATLunState) {
               if (NO_MATCH == rpwwn.m_LocalMatchState) {
                  DebugPrintf(SV_LOG_DEBUG, "%s: AT lun %s not present remotely and locally. Need to rescan.\n",
                                         FUNCTION_NAME, piatluninfo.applianceTargetLUNName.c_str());
               } else {
                  DebugPrintf(SV_LOG_ERROR, "%s: AT lun %s not present remotely but present locally. ERROR!!!\n",
                                         FUNCTION_NAME, piatluninfo.applianceTargetLUNName.c_str());
                  DebugPrintf(SV_LOG_DEBUG, "%s: Remote scan channel: %d , Local scan channel: %d, LocalMatchState: %d\n",
                                         FUNCTION_NAME, rpwwn.m_ChannelNumber, rpwwn.m_LocalChannelNumber, rpwwn.m_LocalMatchState);
               }
            }

            bneedtoquit = (qf && qf(0));
            if (!bneedtoquit)
            {
                /* Needed to mask out remaining channels which are not seeing inmage */
                UpdateATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, 
                                 channelnumbers.size(), &channelnumbers[0], rpwwn);
                if (LUNSTATE_NOTVISIBLE == rpwwn.m_ATLunState) // Sunil: Scan done only if lun is not visible through all channels.
                {
                    DebugPrintf(SV_LOG_DEBUG, "For %s, the at lun is not visible. Hence scanning\n", msg.str().c_str());
                    bool bdrvscan = DriverScan[lpwwn.m_PwwnSrc](lpwwn); // Sunil: ToDo: what does it do, mat be for RHEL 4.x, qlogic???
                    qf && qf(NSECS_TO_WAIT_AFTER_SCAN);
                    bneedtoquit = (qf && qf(0));
                    if (false == bdrvscan)
                    {
                        DebugPrintf(SV_LOG_ERROR, "failed to do driver scan for %s\n", msg.str().c_str());
                    }
                    for (int f = 0; (f < NELEMS(ScanAT)) && !bneedtoquit; f++)
                    {
                        // Sunil: For single device scan, checks all channels
                        // Sunil: For full device scan, after scan, updates only channel# but not target#
                        // Sunil: which could also change.
                        ScanAT[f](piatluninfo, lpwwn, channelnumbers.size(),
                                  &channelnumbers[0], qf, rpwwn);
                        bneedtoquit = (qf && qf(0));
                        if (bneedtoquit)
                        {
                            break;
                        }
                        else 
                        {
                            DebugPrintf(SV_LOG_DEBUG, "After %s, the lun state is %s for %s\n",
                                                      ScanATStr[f], LunStateStr[rpwwn.m_ATLunState], 
                                                      msg.str().c_str());
                            if (LUNSTATE_NOTVISIBLE != rpwwn.m_ATLunState)
                            {
                                break;
                            }
                            else
                            {
                                DebugPrintf(SV_LOG_ERROR, "After %s, the lun state is %s for %s\n",
                                                          ScanATStr[f], LunStateStr[rpwwn.m_ATLunState], 
                                                          msg.str().c_str());
                            }
                        }
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "For %s, not scanning since at lun state is %s\n",
                                              msg.str().c_str(), LunStateStr[rpwwn.m_ATLunState]);
                }
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}


/* This has to match the product and vendor too */
/* TODO: should vendor also come from CX ? */
void UpdateATLunState(const PIAT_LUN_INFO &piatluninfo,
                      const std::string &vendor,
                      const LocalPwwnInfo_t &lpwwn,
                      const Channels_t::size_type nchannels,
                      SV_LONGLONG *pchannels,
                      RemotePwwnInfo_t &rpwwn)
{
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID
        << ", AT LUN name: " << piatluninfo.applianceTargetLUNName
        << ", AT LUN number: " << piatluninfo.applianceTargetLUNNumber;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n", 
                             FUNCTION_NAME, msg.str().c_str());

    rpwwn.m_LocalMatchState = NO_MATCH;
    rpwwn.m_LocalChannelNumber = -1;
    rpwwn.m_RemoteNoMatch = 1;

    for (Channels_t::size_type i = 0; i < nchannels; i++)
    {
        if (pchannels[i] < 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "For %s, the channel number " LLSPEC 
                                      " should not be looked into, since it is negative\n",
                                      msg.str().c_str(), 
                                      pchannels[i]);
            continue;
        }
        UpdateATLunStateFromChannel(piatluninfo, vendor, 
                                    lpwwn, &pchannels[i], rpwwn);
        DebugPrintf(SV_LOG_DEBUG, "For %s, from channel number " LLSPEC 
                                  " lun state is %s\n", msg.str().c_str(), 
                                  pchannels[i], LunStateStr[rpwwn.m_ATLunState]);
        if (LUNSTATE_VALID == rpwwn.m_ATLunState)
        {
            break;
        }
        else if (LUNSTATE_INTERMEDIATE == rpwwn.m_ATLunState)
        {
            break;
        }
        else if (LUNSTATE_NOTVISIBLE == rpwwn.m_ATLunState)
        {
            /* do not do anything here */
        }
        else if (LUNSTATE_NEEDSCLEANUP == rpwwn.m_ATLunState)
        {
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with source LUN ID: %s\n",
                              FUNCTION_NAME,
                              piatluninfo.sourceLUNID.c_str());
}

/*
 * If (LUNSTATE_NOTVISIBLE) keep scanning for other channels
 * If (LUNSTATE_VALID || LUNSTATE_INTERMEDIATE || LUNSTATE_NEEDSCLEANUP) => stop this is the channel we are interested in
 *
 * Remote Check: (using scsi inquiry)
 * ==================================
 * LUN_NAME_MATCHED => vendor / lun-name matched (perfect remote match) => LUNSTATE_INTERMEDIATE or LUNSTATE_VALID
 * LUN_NAME_UNMATCHED => vendor matched / lun-name unmatched (may be cleanup needed) => LUNSTATE_NEEDSCLEANUP
 * LUN_VENDOR_UNMATCHED => vendor did not match (may be dfferent channel, continue to next channel) => LUNSTATE_NOTVISIBLE
 * 
 * Local Check: (using proc)
 * =========================
 * LUN_NOVENDORDETAILS => /dev/sdX open failed OR scsi inquiry failed OR /dev/sdX stat failed OR there is no /dev/sdX for given devt (major, minor)
 * OR devt = 0
 * 	- Get from proc
 * 	- If no vendor for this channel (no entry in proc), continue to other channel => LUNSTATE_NOTVISIBLE
 * 	- Vendor matched (dont check lunname) => LUNSTATE_NEEDSCLEANUP
 * 	- If vendor did not match, continue to other channel => LUNSTATE_NOTVISIBLE
 */

void UpdateATLunStateFromChannel(const PIAT_LUN_INFO &piatluninfo, 
                                 const std::string &vendor,
                                 const LocalPwwnInfo_t &lpwwn,
                                 SV_LONGLONG *pchannel,
                                 RemotePwwnInfo_t &rpwwn)
{
    std::stringstream msg;
    msg << "PI: " << lpwwn.m_Pwwn.m_Name
        << ", AT: " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID: " << piatluninfo.sourceLUNID
        << ", AT LUN name: " << piatluninfo.applianceTargetLUNName
        << ", AT LUN number: " << piatluninfo.applianceTargetLUNNumber;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n", 
                             FUNCTION_NAME, msg.str().c_str());
  
    const char MAJORMINORSEP = ':';
    std::string devtfile = GetDevtFileForLun(lpwwn.m_Pwwn.m_Number,  // Sunil: Tries to find name in /sys/class/scsi_device
                                             *pchannel,
                                             rpwwn.m_Pwwn.m_Number,
                                             piatluninfo.applianceTargetLUNNumber);

    std::string vendornameinproc = GetVendorFromProc(lpwwn.m_Pwwn.m_Number,
					 *pchannel,
					 rpwwn.m_Pwwn.m_Number,
					 piatluninfo.applianceTargetLUNNumber,
                                         piatluninfo.applianceTargetLUNName,
                                         vendor,
                                         rpwwn);

    if (devtfile.empty())
    {
        // Sunil: ToDo: if devtfile is empty does it mean there is no /sys entry? could be.
        // should we look-up in /proc. No /sys/class/scsi_device/H:C:T:L/device/ OR block*/
        // OR dev/, if visible in /proc how to get ATLunPath, may be look at all /dev/ entries.

        DebugPrintf(SV_LOG_DEBUG, "The devtfile is not present for %s\n", msg.str().c_str()); // do scsi-inquiry and match vendor and LunName.
        rpwwn.m_ChannelNumber = -1;
        rpwwn.m_ATLunState = LUNSTATE_NOTVISIBLE;
    }
    else
    {
        bool bgetvendorfromproc = false;
        DebugPrintf(SV_LOG_DEBUG, "devtfile is %s for %s\n", devtfile.c_str(), msg.str().c_str());
        std::string sdevt;
        GetFirstStringFromFile(devtfile, sdevt);
        dev_t devt = MakeDevt(sdevt, MAJORMINORSEP);
        if (0 == devt)
        {
            DebugPrintf(SV_LOG_ERROR, "From devtfile %s, devt is zero\n", devtfile.c_str());
            bgetvendorfromproc = true;
        }
        else
        {
            //ACE_OS::sleep(30);
            std::string atlunpath;
            E_LUNMATCHSTATE elunmatchstate = GetLunMatchState(devt, OSNAMESPACEFORDEVICES, 
                                                              piatluninfo.applianceTargetLUNName, 
                                                              vendor,
                                                              piatluninfo.applianceTargetLUNNumber,
                                                              atlunpath);
            DebugPrintf(SV_LOG_DEBUG, "From devtfile %s, lun match state is %s\n", devtfile.c_str(),
                                      LunMatchStateStr[elunmatchstate]);
            if (LUN_NOVENDORDETAILS == elunmatchstate)
            {
                DebugPrintf(SV_LOG_ERROR, "From devtfile %s, vendor details could not be found\n", devtfile.c_str());
                bgetvendorfromproc = true;
            }
            else if (LUN_VENDOR_UNMATCHED == elunmatchstate)
            {
                DebugPrintf(SV_LOG_DEBUG, "From devtfile %s, vendor is not %s\n", devtfile.c_str(), vendor.c_str());
                rpwwn.m_ChannelNumber = *pchannel = -1;
                rpwwn.m_ATLunState = LUNSTATE_NOTVISIBLE;
            }
            else if (LUN_NAME_UNMATCHED == elunmatchstate)
            {
                DebugPrintf(SV_LOG_ERROR, "From devtfile %s, vendor is %s, but lun name did not match. " 
                                          " lun needs cleanup in this state\n", 
                                          devtfile.c_str(), vendor.c_str());
                rpwwn.m_ATLunState = LUNSTATE_NEEDSCLEANUP;
                rpwwn.m_ChannelNumber = *pchannel;
                rpwwn.m_RemoteNoMatch = 0;
            }
            else if (LUN_NAME_MATCHED == elunmatchstate)
            {
                /* lun can be valid or intermediate */ 
                rpwwn.m_ChannelNumber = *pchannel;
                rpwwn.m_ATLunPath = atlunpath;
                UpdateLunFormationState(piatluninfo, vendor, lpwwn, rpwwn);
            }
        }
         
        if (bgetvendorfromproc)
        {
            std::string vendorinproc = GetVendorFromProc(lpwwn.m_Pwwn.m_Number, // Sunil: ToDo: Get LunName (Model:) also from /proc
                                                         *pchannel,
                                                         rpwwn.m_Pwwn.m_Number,
                                                         piatluninfo.applianceTargetLUNNumber,
                                                         piatluninfo.applianceTargetLUNName,
                                                         vendor,
                                                         rpwwn);
            DebugPrintf(SV_LOG_DEBUG, "For %s, the vendor from proc is %s\n", msg.str().c_str(), 
                                      vendorinproc.c_str());
            if (vendorinproc.empty())
            {
                DebugPrintf(SV_LOG_ERROR, "For %s, vendor in proc is empty. No action in this case\n", msg.str().c_str());
                rpwwn.m_ChannelNumber = *pchannel = -1;
                rpwwn.m_ATLunState = LUNSTATE_NOTVISIBLE;
            }
            else if (vendorinproc == vendor)
            {
                // Sunil: ToDo: If LunName does not match given one, then ATLunState = LUNSTATE_NEEDSCLEANUP,
                // else if dev_t is valid and /dev file not hidden ATLunState = LUNSTATE_VALID (note:
                // open/scsi-inquiry may have failed) else if dev_t valid and /dev is hidden
                // ATLunState = LUNSTATE_INTERMEDIATE, else ATLunState = LUNSTATE_NEEDSCLEANUP
                DebugPrintf(SV_LOG_ERROR, "For %s, vendor is %s from proc."
                                          " lun needs cleanup in this state\n", 
                                          msg.str().c_str(), vendor.c_str());
                rpwwn.m_ATLunState = LUNSTATE_NEEDSCLEANUP;
                rpwwn.m_ChannelNumber = *pchannel;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "For %s, vendor is not %s from proc. No action in this case\n",
                                          msg.str().c_str(), vendor.c_str());
                rpwwn.m_ChannelNumber = *pchannel =  -1;
                rpwwn.m_ATLunState = LUNSTATE_NOTVISIBLE;
            }
        }
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
    DeleteATForPIAT(piatluninfo, lpwwn, channels, qf, rpwwn);
}


void DeleteATLunFromScsiHost(const PIAT_LUN_INFO &piatluninfo, 
                             const LocalPwwnInfo_t &lpwwn, 
                             Channels_t &channels,
                             QuitFunction_t &qf,
                             RemotePwwnInfo_t &rpwwn)
{
    DeleteATForPIAT(piatluninfo, lpwwn, channels, qf, rpwwn);
}


void DeleteATLunFromQla(const PIAT_LUN_INFO &piatluninfo, 
                        const LocalPwwnInfo_t &lpwwn, 
                        Channels_t &channels,
                        QuitFunction_t &qf,
                        RemotePwwnInfo_t &rpwwn)
{
    DeleteATForPIAT(piatluninfo, lpwwn, channels, qf, rpwwn);
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


void DeleteATForPIAT(const PIAT_LUN_INFO &piatluninfo, 
                     const LocalPwwnInfo_t &lpwwn, 
                     Channels_t &channels,
                     QuitFunction_t &qf,
                     RemotePwwnInfo_t &rpwwn)
{
    DeleteATLunFromPIATWwn(piatluninfo, lpwwn, qf, rpwwn);
    if (!channels.empty())
    {
        /* to get status from all channel numbers */
        UpdateATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, 
                         channels.size(), &channels[0], rpwwn);
    }
}


void DeleteATLunFromPIATWwn(const PIAT_LUN_INFO &piatluninfo, 
                            const LocalPwwnInfo_t &lpwwn,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn)
{
    bool bneedtoquit = (qf && qf(0));
    std::stringstream msg;
    msg << "PI " << lpwwn.m_Pwwn.m_Name
        << ", AT " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID" << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s\n",
                              FUNCTION_NAME, msg.str().c_str());

    // Sunil: Should not we check that LunState is not NOT_VISIBLE before
    // deleting may be before coming into this function as it is also 
    // called from LunAdd path. Or does this check mean that lun is not NOT_VISIBLE else
    // channel# would have been -1.

    if (rpwwn.m_ChannelNumber < 0) {
        if (rpwwn.m_ATLunState == LUNSTATE_NOTVISIBLE) {
            DebugPrintf(SV_LOG_DEBUG, "%s: Lun state is not visible (remotely), channel: %d\n",
                                  FUNCTION_NAME, rpwwn.m_ChannelNumber);
        }
        if ((rpwwn.m_RemoteNoMatch == 1) && (rpwwn.m_LocalMatchState == VENDOR_LUN_MATCH)) {
            DebugPrintf(SV_LOG_DEBUG, "%s: Lun state is visible (locally), channel: %d, need to cleanup\n",
                                  FUNCTION_NAME, rpwwn.m_LocalChannelNumber);
            rpwwn.m_ChannelNumber = rpwwn.m_LocalChannelNumber;
            // Leaving state as LUNSTATE_NOTVISIBLE
        } else {
            if (rpwwn.m_RemoteNoMatch != 1) {
                DebugPrintf(SV_LOG_DEBUG, "%s: Lun with identical vendor but not lun-name visible (remotely). Can not do cleanup\n",
                                  FUNCTION_NAME);
            }
            if (rpwwn.m_LocalMatchState != VENDOR_LUN_MATCH) {
                DebugPrintf(SV_LOG_DEBUG, "%s: Lun with matching vendor and lun-name not visible (locally). Can not do cleanup\n",
                                  FUNCTION_NAME);
            }
            DebugPrintf(SV_LOG_DEBUG, "%s: Remote scan channel: %d , Local scan channel: %d\n",
                                         FUNCTION_NAME, rpwwn.m_ChannelNumber, rpwwn.m_LocalChannelNumber); 
        }
    }

    if (rpwwn.m_ChannelNumber >= 0) 
    {
        std::vector<std::string> cmds;
        std::string removesinglecmd = DeleteSingleDeviceCmd(lpwwn, rpwwn, &piatluninfo.applianceTargetLUNNumber);
        if (!removesinglecmd.empty())
        {
            cmds.push_back(removesinglecmd);
        }
        std::string sysdeletecmd = SysDeleteCmd(lpwwn, rpwwn, &piatluninfo.applianceTargetLUNNumber);
        if (!sysdeletecmd.empty())
        {
            cmds.push_back(sysdeletecmd);
        }

        std::vector<std::string>::const_iterator citer = cmds.begin();
        for ( /* empty */ ; (citer != cmds.end()) && !bneedtoquit; citer++)
        {
            const std::string &cmd = *citer;
            std::stringstream results;
            bool bexeccmd = executePipe(cmd, results);
            if (false == bexeccmd)
            {
                DebugPrintf(SV_LOG_ERROR, "failed to run delete command %s for %s\n", 
                                           cmd.c_str(), msg.str().c_str());
            }
            qf && qf(NSECS_TO_WAIT_AFTER_DELETE);
            bneedtoquit = (qf && qf(0));
            if (bneedtoquit)
            {
                break;
            }
            else 
            {
                // Sunil: Wait for 3/5 seconds, dont check state immediately
                if (rpwwn.m_ChannelNumber < 0 && rpwwn.m_LocalMatchState == VENDOR_LUN_MATCH) {
                    // Lun is NOTVISIBLE remotely but VISIBLE Locally
                    rpwwn.m_ChannelNumber = rpwwn.m_LocalChannelNumber;
                }
                UpdateATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, 1, 
                                 &rpwwn.m_ChannelNumber, rpwwn);
                DebugPrintf(SV_LOG_DEBUG, "For %s, the at lun state is %s after delete command %s, LocalMatchState: %d\n", 
                                          msg.str().c_str(), LunStateStr[rpwwn.m_ATLunState], 
                                          cmd.c_str(), rpwwn.m_LocalMatchState);
                if (LUNSTATE_NOTVISIBLE == rpwwn.m_ATLunState && NO_MATCH == rpwwn.m_LocalMatchState)
                {
                    break;
                }
                else
                {
                    std::stringstream sswaitnsecs;
                    sswaitnsecs << NSECS_TO_WAIT_FOR_ATLUN_DELETE; // Sunil: ToDo: 3 seconds are enough
                    DebugPrintf(SV_LOG_ERROR, "After delete command %s, the at lun has not been deleted."
                                              "waiting for %s seconds for it to delete\n",
                                              cmd.c_str(), sswaitnsecs.str().c_str());
                    qf && qf(NSECS_TO_WAIT_FOR_ATLUN_DELETE);
                    bneedtoquit = (qf && qf(0));
                    if (bneedtoquit)
                    {
                        break;
                    }
                    else
                    {
                         if (rpwwn.m_ChannelNumber < 0 && rpwwn.m_LocalMatchState == VENDOR_LUN_MATCH) {
                             // Lun is NOTVISIBLE remotely but VISIBLE Locally
                             rpwwn.m_ChannelNumber = rpwwn.m_LocalChannelNumber;
                         }
                         UpdateATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, 1, 
                                          &rpwwn.m_ChannelNumber, rpwwn);
                         DebugPrintf(SV_LOG_DEBUG, "For %s, after waiting for at lun to get deleted, its state is %s, LocalMatchState: %d\n", 
                                                   msg.str().c_str(), LunStateStr[rpwwn.m_ATLunState], rpwwn.m_LocalMatchState); 
                         if (LUNSTATE_NOTVISIBLE == rpwwn.m_ATLunState && NO_MATCH == rpwwn.m_LocalMatchState)
                         {
                             break;
                         }
                         else
                         {
                             DebugPrintf(SV_LOG_ERROR, "After delete command %s, the at lun has not been deleted,"
                                                       "even after waiting for %s seconds\n", 
                                                       cmd.c_str(), sswaitnsecs.str().c_str());
                         }
                    }
                }
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, deletion is being done when channel number is not filled for AT Lun\n",
                                  msg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with source LUN ID: %s\n",
                              FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}


std::string DeleteSingleDeviceCmd(const LocalPwwnInfo_t &lpwwn, 
                                  const RemotePwwnInfo_t &rpwwn,
                                  const SV_ULONGLONG *patlunnumber)
{
    std::stringstream sshostnum;
    std::stringstream sschannelnum;
    std::stringstream sstargetnum;
    std::stringstream sslunnumber;

    std::string cmd = "echo \'";
    cmd += SINGLEDEVICEREMCMD;
    cmd += " ";
    sshostnum << lpwwn.m_Pwwn.m_Number;
    cmd += sshostnum.str();
    cmd += " ";
    sschannelnum << rpwwn.m_ChannelNumber;
    cmd += sschannelnum.str();
    cmd += " ";
    sstargetnum << rpwwn.m_Pwwn.m_Number;
    cmd += sstargetnum.str();
    cmd += " ";
    sslunnumber << *patlunnumber;
    cmd += sslunnumber.str();
    cmd += "\' >/proc/scsi/scsi";

    return cmd;
}


std::string SysDeleteCmd(const LocalPwwnInfo_t &lpwwn, 
                         const RemotePwwnInfo_t &rpwwn,
                         const SV_ULONGLONG *patlunnumber)
{
    bool bdeleted = false;
    std::stringstream sspiat;
    const char HOSTCHANNELSEP = ':';

    std::string cmd = "find";
    cmd += " ";
    cmd += SYS;
    cmd += " ";
    cmd += "-name";
    cmd += " ";
    cmd += "\'";
    cmd += SYSDELETEFILE;
    cmd += "\'";
    cmd += " | ";
    cmd += "grep";
    cmd += " "; 
    cmd += "\'";

    sspiat << UNIX_PATH_SEPARATOR;
    sspiat << lpwwn.m_Pwwn.m_Number;
    sspiat << HOSTCHANNELSEP;
    sspiat << rpwwn.m_ChannelNumber;
    sspiat << HOSTCHANNELSEP;
    sspiat << rpwwn.m_Pwwn.m_Number;
    sspiat << HOSTCHANNELSEP;
    sspiat << *patlunnumber;
    sspiat << UNIX_PATH_SEPARATOR;

    cmd += sspiat.str();
    cmd += "\'";

    std::string echocmd;
    std::stringstream results;
    /* TODO: should we use InmCommand here ? */
    /* TODO: delete file should be taken only once */
    bool bexecd = executePipe(cmd, results);
    if (bexecd && !results.eof())
    {
        echocmd = "echo 1 >";
        std::string deletefile; 
        results >> deletefile;
        echocmd += deletefile;
    }

    return echocmd;
}


bool DriverScanFromQla(const LocalPwwnInfo_t &lpwwn)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with PIWWN = %s\n", 
                             FUNCTION_NAME, lpwwn.m_Pwwn.m_Name.c_str());
    bool bdrvscanned = false;
    std::stringstream sshostnum;

    std::string cmd = "echo \'";
    cmd += QLADRVSCANCMD;
    cmd += "\'";
    cmd += " >";
    std::string file = QLADRVSCANDIR;
    sshostnum << lpwwn.m_Pwwn.m_Number;
    file += sshostnum.str();
    cmd += file;

    std::stringstream results;
    /* TODO: should we use InmCommand here ? */
    bdrvscanned = executePipe(cmd, results);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return bdrvscanned;
}


bool DriverScanFromFcHost(const LocalPwwnInfo_t &lpwwn)
{
    return true;
}


bool DriverScanFromScsiHost(const LocalPwwnInfo_t &lpwwn)
{
    return true;
}

bool DriverScanFromIscsiHost(const LocalPwwnInfo_t &lpwwn)
{
    return true;
}

bool DriverScanFromFcInfo(const LocalPwwnInfo_t &lpwwn)
{
    return false;
}


bool DriverScanFromScli(const LocalPwwnInfo_t &lpwwn)
{
    return false;
}


bool DriverScanFromLpfc(const LocalPwwnInfo_t &lpwwn)
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


bool DeleteATLunsFromCumulation(const PIAT_LUN_INFO &piatluninfo, 
                                QuitFunction_t &qf, 
                                LocalPwwnInfos_t &lpwwns)
{
    return false;
}


bool DriverScanFromUnknownSrc(const LocalPwwnInfo_t &lpwwn)
{
    DebugPrintf(SV_LOG_ERROR, "local pwwn %s, came from unknown source for driver scan\n", 
                              lpwwn.m_Pwwn.m_Name.c_str());
    return false;
}


bool UpdateATNumberFromFcHost(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                              const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                              QuitFunction_t &qf)
{
    bool bupdated = false;

    std::stringstream msg;
    msg << "PI " << lpwwn.m_Pwwn.m_Name
        << ", AT " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID" << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    LocalPwwnInfo_t cpoflpwwn;
    cpoflpwwn.m_Pwwn = lpwwn.m_Pwwn;
    cpoflpwwn.m_FormattedName = lpwwn.m_FormattedName;
    cpoflpwwn.m_PwwnSrc = lpwwn.m_PwwnSrc;
    GetRemotePwwnsFuns[cpoflpwwn.m_PwwnSrc](&cpoflpwwn, qf);
    /* here need not worry about quit */

    RemotePwwnInfos_t::const_iterator nriter = cpoflpwwn.m_RemotePwwnInfos.find(rpwwn.m_FormattedName);
    if (nriter != cpoflpwwn.m_RemotePwwnInfos.end())
    {
        const RemotePwwnInfo_t &newrpwwn = nriter->second;
        DebugPrintf(SV_LOG_DEBUG, "For %s, after full scan, new target id is: " 
                                  LLSPEC ", old target id was: " LLSPEC "\n", 
                                  msg.str().c_str(), newrpwwn.m_Pwwn.m_Number,
                                  rpwwn.m_Pwwn.m_Number);
        /* just assing number; again we do UpdateATLunState
         * so that at lun names, channel numbers are updated */
        rpwwn.m_Pwwn.m_Number = newrpwwn.m_Pwwn.m_Number;
        bupdated = true;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For %s, after full scan, could not find remote pwwn itself\n",
                                  msg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());
    return bupdated;
}

bool UpdateATNumberFromScsiHost(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                                const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                                QuitFunction_t &qf)
{
    return true;
}

bool UpdateATNumberFromIscsiHost(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                                const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                                QuitFunction_t &qf)
{
    return true;
}


bool UpdateATNumberFromQla(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                           const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                           QuitFunction_t &qf)
{
    return true;
}


bool UpdateATNumberFromScli(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                            const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                            QuitFunction_t &qf)
{
    return false;
}


bool UpdateATNumberFromLpfc(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                            const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                            QuitFunction_t &qf)
{
    return false;
}


bool UpdateATNumberFromFcInfo(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                              const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                              QuitFunction_t &qf)
{
    return false;
}


bool UpdateATNumberFromUnknownSrc(const PIAT_LUN_INFO &piatluninfo, const LocalPwwnInfo_t &lpwwn,
                                  const std::vector<SV_LONGLONG> &channelnumbers, RemotePwwnInfo_t &rpwwn, 
                                  QuitFunction_t &qf)
{
    DebugPrintf(SV_LOG_ERROR, "local pwwn %s, remote pwwn %s, came from unknown source for "
                              " source LUN ID %s to get at lun paths\n", lpwwn.m_Pwwn.m_Name.c_str(), rpwwn.m_Pwwn.m_Name.c_str(),
                              piatluninfo.sourceLUNID.c_str());
    return false;
}


std::string GetDevtFileForLun(const SV_LONGLONG host,
                              const SV_LONGLONG channel,
                              const SV_LONGLONG target,
                              const SV_ULONGLONG lun)
{
    const std::string DEVTFILEPREFIX = "/sys/class/scsi_device/";
    const std::string DEVTFILESUFFIX = "/device/";
    const std::string DIRPATTERN = "block";
    const std::string NDIRPATTERN = "sd";
    const std::string DEVTFILE = "dev";
    const char DEVTFILESEP = ':';
    std::stringstream sshostnum;
    std::stringstream sschannelnum;
    std::stringstream sstargetnum;
    std::stringstream sslunnumber;
    sshostnum << host;
    sschannelnum << channel;
    sstargetnum << target;
    sslunnumber << lun;
    std::string dir = DEVTFILEPREFIX;
    dir += sshostnum.str();
    dir += DEVTFILESEP;
    dir += sschannelnum.str();
    dir += DEVTFILESEP;
    dir += sstargetnum.str();
    dir += DEVTFILESEP;
    dir += sslunnumber.str();
    dir += DEVTFILESUFFIX;

    std::string file;
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    const char *pdir = dir.c_str();
    dentresult = NULL;
    dirp = opendir(pdir);
    boost::shared_ptr<void> dirpGuard (static_cast<void*>(0), boost::bind(closedir, dirp));
    if (dirp)
    {
       dentp = (struct dirent *)calloc(direntsize, 1);
       if (dentp)
       {
           while ((0 == readdir_r(dirp, dentp, &dentresult)) && dentresult)
           {
               if (strcmp(dentp->d_name, ".") &&
                   strcmp(dentp->d_name, "..") && 
                   (0 == strncmp(dentp->d_name, DIRPATTERN.c_str(), strlen(DIRPATTERN.c_str()))))
               {
                   std::string devtfile = dir;
                   std::string ndevtfile = dir;
                   devtfile += dentp->d_name;
                   ndevtfile += dentp->d_name;
                   devtfile += UNIX_PATH_SEPARATOR;
                   ndevtfile += UNIX_PATH_SEPARATOR;
                   devtfile += DEVTFILE;
                   struct stat64 devtstat;
                   /* TODO: should there be stat ? */
                   if (0 == stat64(devtfile.c_str(), &devtstat))
                   {
                       /*
                        * On OS < RHEL 6.1, "dev" file is at:
                        * /sys/class/scsi_device/1\:0\:0\:0/device/block\:sdb/dev
                        */
                       file = devtfile;
                   } else
                   {
                       /*
                        * On OS = RHEL 6.1, "dev" file is at:
                        * /sys/class/scsi_device/4\:0\:0\:0/device/block/sdb/dev
                        */
                       const char *npdir = ndevtfile.c_str();
                       DIR *ndirp;
                       struct dirent *ndentp, *ndentresult;
                       size_t ndirentsize = sizeof(struct dirent) + PATH_MAX + 1;
                       ndentresult = NULL;
                       ndirp = opendir(npdir);
                       boost::shared_ptr<void> npdirGuard (static_cast<void*>(0), boost::bind(closedir, ndirp));
                       if (ndirp)
                       {
                           ndentp = (struct dirent *)calloc(ndirentsize, 1);
                           if (ndentp)
                           {
                               while ((0 == readdir_r(ndirp, ndentp, &ndentresult)) && ndentresult)
                               {
                                   if (strcmp(ndentp->d_name, ".") &&
                                       strcmp(ndentp->d_name, "..") && 
                                       (0 == strncmp(ndentp->d_name, NDIRPATTERN.c_str(), strlen(NDIRPATTERN.c_str()))))
                                      {
                                          std::string cdevtfile = ndevtfile;
                                          cdevtfile += ndentp->d_name;
                                          cdevtfile += UNIX_PATH_SEPARATOR;
                                          cdevtfile += DEVTFILE;
                                          struct stat64 ndevtstat;
                                          /* TODO: should there be stat ? */
                                          if (0 == stat64(cdevtfile.c_str(), &ndevtstat))
                                          {
                                              file = cdevtfile;
                                          }
                                          break;
                                      }
                               }
                           }
                           // closedir(ndirp); 
                       } /* end of "if (ndirp)" */
                   } /* end of "if (0 == stat64(devtfile.c_str(), &devtstat))" */
                   break;
               } /* end of skipping . and .. */
               memset(dentp, 0, direntsize);
           } /* end of while readdir_r */
           free(dentp);
       } /* endif of if (dentp) */
       //closedir(dirp); 
    } /* end of if (dirp) */

    return file;
}


std::string GetVendorFromProc(const SV_LONGLONG host,
                              const SV_LONGLONG channel,
                              const SV_LONGLONG target,
                              const SV_ULONGLONG lun,
                              const std::string &lunName,
                              const std::string &vendorName,
                              RemotePwwnInfo_t &rpwwn)
{
    /* 
    * Host: scsi2 Channel: 00 Id: 03 Lun: 36
    *   Vendor: Promise  Model: VTrak E310f      Rev: 0322
    *   Type:   Direct-Access                    ANSI SCSI revision: 04
    * Host: scsi2 Channel: 00 Id: 03 Lun: 37
    *   Vendor: Promise  Model: VTrak E310f      Rev: 0322
    *   Type:   Direct-Access                    ANSI SCSI revision: 04
    */
    std::string lunModel;
    std::string vendor;
    std::ifstream ifile("/proc/scsi/scsi");
    std::stringstream results;

    if (ifile.is_open())
    {
        if (ifile.good())
        {
            results << ifile.rdbuf();
        }
        ifile.close();
    }

    if (results.str().empty())
    {
        return vendor;
    }
    const std::string HOSTTOKEN = "Host:";

    size_t currentStartPos = 0;
    size_t offset = 0;
    while ((std::string::npos != currentStartPos) && ((currentStartPos + offset) < results.str().length()))
    {
        size_t nextStartPos = results.str().find(HOSTTOKEN, currentStartPos + offset);
        if (0 == offset)
        {
            offset = HOSTTOKEN.size();
        }
        else
        {
            size_t hoststreamlen = (std::string::npos == nextStartPos) ? std::string::npos : (nextStartPos - currentStartPos);
            std::string hostinfo = results.str().substr(currentStartPos, hoststreamlen);
            if (IsMatchingVendorFound(hostinfo, host, channel, target, lun, vendor, lunModel))
            {
               if (lunModel.find("DUMMY_LUN_ZERO") != std::string::npos)
                    lunModel += "  ";

               DebugPrintf(SV_LOG_DEBUG,"%s: lunName %s lunModel %s.\n", FUNCTION_NAME, lunName.c_str(), lunModel.c_str());

                if (rpwwn.m_LocalMatchState != VENDOR_LUN_MATCH) {
                   if ((vendor == vendorName) && (lunModel == lunName)) {
                       rpwwn.m_LocalMatchState = VENDOR_LUN_MATCH;
                       rpwwn.m_LocalChannelNumber = channel;
                       DebugPrintf(SV_LOG_DEBUG,"%s: Found matching vendor %s and lun-name %s in proc, (H:%d C:%d T:%d L:%d)\n",
                                    FUNCTION_NAME, vendor.c_str(), lunModel.c_str(), host, channel, target, lun);
                   } else {
                       if (vendor == vendorName) {
                           rpwwn.m_LocalMatchState = VENDOR_MATCH;
                           rpwwn.m_LocalChannelNumber = channel;
                           DebugPrintf(SV_LOG_DEBUG,"%s: Found matching vendor %s in proc, (H:%d C:%d T:%d L:%d)\n",
                       		     FUNCTION_NAME, vendor.c_str(), host, channel, target, lun);
                       }
                   }
                }
                break;
            }
        }
        currentStartPos = nextStartPos;
    }
    
    return vendor;
}


bool IsMatchingVendorFound(const std::string &hostinfo,
                           const SV_LONGLONG host,
                           const SV_LONGLONG channel,
                           const SV_LONGLONG target,
                           const SV_ULONGLONG lun,
                           std::string &vendor,
                           std::string &lunModel)
{
    std::stringstream ss(hostinfo);
    SV_LONGLONG gothost = 0;
    SV_LONGLONG gotchannel = 0;
    SV_LONGLONG gottarget = 0;
    SV_ULONGLONG gotlun = 0;
    bool bisfound = false;

    std::string hosttok, channeltok, idtok, luntok;
    std::string hoststr; 
    ss >> hosttok >> hoststr
       >> channeltok >> gotchannel
       >> idtok >> gottarget
       >> luntok >> gotlun;
    const char *p = GetStartOfLastNumber(hoststr.c_str()); 
    if (p && (1 == sscanf(p, LLSPEC, &gothost)))
    {
        if ((gothost == host) && 
            (gotchannel == channel) &&
            (gottarget == target) &&
            (gotlun == lun))
        {
            bisfound = true;
            std::string vendortok, modeltok;
            ss >> vendortok >> vendor;
            ss >> modeltok >> lunModel;
        }
    }
     
    return bisfound;
}


std::string FormFullScanCmd(const LocalPwwnInfo_t &lpwwn)
{
    std::string cmd = "echo \'- - -\' >/sys/class/scsi_host/host";
    std::stringstream sshostnum;
    sshostnum << lpwwn.m_Pwwn.m_Number;
    cmd += sshostnum.str();
    cmd += UNIX_PATH_SEPARATOR;
    cmd += "scan";

    return cmd;
}


std::string FormSingleScanCmd(const SV_LONGLONG host,
                              const SV_LONGLONG channel,
                              const SV_LONGLONG target,
                              const SV_ULONGLONG lun)
{
    std::stringstream sshostnum;
    std::stringstream sschannelnum;
    std::stringstream sstargetnum;
    std::stringstream sslunnumber;

    std::string cmd = "echo \'";
    cmd += SINGLEDEVICESCANCMD;
    cmd += " ";
    sshostnum << host;
    cmd += sshostnum.str();
    cmd += " ";
    sschannelnum << channel;
    cmd += sschannelnum.str();
    cmd += " ";
    sstargetnum << target;
    cmd += sstargetnum.str();
    cmd += " ";
    sslunnumber << lun;
    cmd += sslunnumber.str();
    cmd += "\' >/proc/scsi/scsi";

    return cmd;
}


void SingleDeviceScan(const PIAT_LUN_INFO &piatluninfo,
                      const LocalPwwnInfo_t &lpwwn, 
                      const Channels_t::size_type nchannels,
                      SV_LONGLONG *pchannels,
                      QuitFunction_t &qf,
                      RemotePwwnInfo_t &rpwwn)
{
    std::stringstream msg;
    msg << "PI " << lpwwn.m_Pwwn.m_Name
        << ", AT " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID" << piatluninfo.sourceLUNID;
    bool bneedtoquit = (qf && qf(0));
    for (Channels_t::size_type i = 0; (i < nchannels) && !bneedtoquit; i++)
    {
        if (pchannels[i] < 0)
        {
            continue;
        }
        std::string cmd = FormSingleScanCmd(lpwwn.m_Pwwn.m_Number, pchannels[i],
                                            rpwwn.m_Pwwn.m_Number, piatluninfo.applianceTargetLUNNumber);
        std::stringstream results;
        bool bexeccmd = executePipe(cmd, results);
        if (false == bexeccmd)
        {
            DebugPrintf(SV_LOG_ERROR, "failed to run scan command %s for %s\n", 
                                      cmd.c_str(), msg.str().c_str());
        }
        qf && qf(NSECS_TO_WAIT_AFTER_SCAN);
        bneedtoquit = (qf && qf(0));
        if (bneedtoquit)
        {
            break;
        }
        else 
        {
		    // Sunil: ToDo: Dont check immediately, wait for 3/5 seconds
            UpdateATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, 1, 
                             &pchannels[i], rpwwn);
            DebugPrintf(SV_LOG_DEBUG, "For %s, the at lun state is %s after scan command %s\n", 
                                      msg.str().c_str(), LunStateStr[rpwwn.m_ATLunState], 
                                      cmd.c_str());
            if ((LUNSTATE_VALID == rpwwn.m_ATLunState) ||
                (LUNSTATE_NEEDSCLEANUP == rpwwn.m_ATLunState))
            {
                break;
            }
            else if ((LUNSTATE_NOTVISIBLE == rpwwn.m_ATLunState) ||
                     (LUNSTATE_INTERMEDIATE == rpwwn.m_ATLunState))
            {
                std::stringstream ssnsecstowait;
                ssnsecstowait << NSECS_TO_WAIT_FOR_VALID_ATLUN; // Sunil: 60 seconds ok for intermediate
                DebugPrintf(SV_LOG_DEBUG, "waiting for %s seconds for valid AT Lun\n",
                                          ssnsecstowait.str().c_str());
                qf && qf(NSECS_TO_WAIT_FOR_VALID_ATLUN);
                bneedtoquit = (qf && qf(0));
                if (bneedtoquit)
                {
                    break;
                }
                else
                {
                     UpdateATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, 1, 
                                      &pchannels[i], rpwwn);
                     DebugPrintf(SV_LOG_DEBUG, "After command %s, at lun state is %s, "
                                               "after waiting for it to be valid\n",
                                               cmd.c_str(), LunStateStr[rpwwn.m_ATLunState]);
                     if (LUNSTATE_NOTVISIBLE != rpwwn.m_ATLunState)
                     {
                         break;
                     }
                }
            }
        }
    }
}


void FullDeviceScan(const PIAT_LUN_INFO &piatluninfo,
                    const LocalPwwnInfo_t &lpwwn, 
                    const Channels_t::size_type nchannels,
                    SV_LONGLONG *pchannels,
                    QuitFunction_t &qf,
                    RemotePwwnInfo_t &rpwwn)
{
    std::stringstream msg;
    msg << "PI " << lpwwn.m_Pwwn.m_Name
        << ", AT " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID" << piatluninfo.sourceLUNID;

    std::string cmd = FormFullScanCmd(lpwwn);
    std::stringstream results;
    bool bexeccmd = executePipe(cmd, results);
    if (false == bexeccmd)
    {
        DebugPrintf(SV_LOG_ERROR, "failed to run scan command %s for %s\n", 
                                  cmd.c_str(), msg.str().c_str());
    }
    qf && qf(NSECS_TO_WAIT_AFTER_SCAN); // Sunil: ToDo: 3 sec. fine
    bool bneedtoquit = (qf && qf(0));
    bool bupdatedtargetid = false;
    Channels_t channels;
    if (!bneedtoquit)
    {
        GetChannelNumbers(channels);
        if (!channels.empty())
        {
            bupdatedtargetid = UpdateATNumberFromPIAT[lpwwn.m_PwwnSrc](piatluninfo,
                                                                      lpwwn,
                                                                      channels,
                                                                      rpwwn, qf);
            if (!bupdatedtargetid)
            {
                DebugPrintf(SV_LOG_ERROR, "for %s, after full scan, could not find the target ID\n",
                                          msg.str().c_str());
            }
        }
    }

    bneedtoquit = (qf && qf(0));
    if (!bneedtoquit && bupdatedtargetid)
    {
		// Sunil: Target number also could change.
        if (!channels.empty())
        {
            UpdateATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, channels.size(), 
                             &channels[0], rpwwn);
            DebugPrintf(SV_LOG_DEBUG, "For %s, the at lun state is %s after scan command %s\n", 
                                      msg.str().c_str(), LunStateStr[rpwwn.m_ATLunState], 
                                      cmd.c_str());
            if ((LUNSTATE_INTERMEDIATE == rpwwn.m_ATLunState) || 
                (LUNSTATE_NOTVISIBLE == rpwwn.m_ATLunState))
            {
                std::stringstream ssnsecstowait;
                ssnsecstowait << NSECS_TO_WAIT_FOR_VALID_ATLUN;
                DebugPrintf(SV_LOG_ERROR, "After command %s, at lun state is %s."
                                          " Waiting for %s seconds for valid AT Lun\n",
                                          cmd.c_str(), LunStateStr[rpwwn.m_ATLunState],
                                          ssnsecstowait.str().c_str());
                qf && qf(NSECS_TO_WAIT_FOR_VALID_ATLUN); // Sunil: 60 sec. for intermediate is fine
                bneedtoquit = (qf && qf(0));
                if (!bneedtoquit)
                {
                     if (!channels.empty())
                     {
                         UpdateATLunState(piatluninfo, INMATLUNVENDOR, lpwwn, channels.size(), 
                                          &channels[0], rpwwn);
                         DebugPrintf(SV_LOG_ERROR, "After command %s, at lun state is %s."
                                                   " after waiting for %s seconds for valid AT Lun\n",
                                                   cmd.c_str(), LunStateStr[rpwwn.m_ATLunState],
                                                   ssnsecstowait.str().c_str());
                     }
                }
            }
        }
    }
}

void DeleteATLunFromIScsiHost(const PIAT_LUN_INFO &piatluninfo,
                         const LocalPwwnInfo_t &lpwwn,
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn)
{
    DeleteATForPIAT(piatluninfo, lpwwn, channels, qf, rpwwn);
}

void GetATLunPathFromIScsiHost(const PIAT_LUN_INFO &piatluninfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn)
{
    GetATLunPathFromPIATWwn(piatluninfo, lpwwn, bshouldscan, 
                            channelnumbers, qf, rpwwn);
}

void DiscoverRemoteIScsiTargets(std::list<std::string> networkAddresses)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);
    std::stringstream results;

    for (std::list<std::string>::iterator it=networkAddresses.begin(); it != networkAddresses.end(); ++it)
    {
        std::string sentIpAddr = *it;

        std::string cmd1("/sbin/iscsiadm -m discoverydb -t sendtargets -p ");
        cmd1 += sentIpAddr;
        cmd1 += " --discover";
    
        if (executePipe(cmd1, results))
        {
            DebugPrintf(SV_LOG_DEBUG, "Adding %s to discoverydb complete.\n", sentIpAddr.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd1.c_str());
        }

        DebugPrintf(SV_LOG_DEBUG, "Issuing iscsi login to %s.\n", sentIpAddr.c_str());
        std::string cmd2("/sbin/iscsiadm -m node -p ");
        cmd2 += sentIpAddr;
        cmd2 += " -l";

        if (executePipe(cmd2, results))
        {
            DebugPrintf(SV_LOG_DEBUG, "login to %s complete.\n", sentIpAddr.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Failed to execute command %s\n", cmd2.c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return;
}

void DiscoverRemoteFcTargets()
{
    return;
}

