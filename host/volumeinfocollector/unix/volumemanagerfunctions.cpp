#include <string>
#include <sstream>
#include <set>
#include <iterator>
#include <algorithm>
#include <map>
#include <vector>
#include <list>
#include "volumemanagerfunctions.h"
#include "volumemanagerfunctionsminor.h"
#include "localconfigurator.h"
#include "voldefs.h"
#include "executecommand.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "utildirbasename.h"
#include "atacmd.h"
#include "atacommandissuer.h"
#include "utilfunctionsmajor.h"


void GetVxDmpNodes(std::set<std::string> &vxdmpnodes)
{
    LocalConfigurator theLocalConfigurator;
    std::string vxdmpcmd(theLocalConfigurator.getVxDmpCmd());
    std::string grepcmd(theLocalConfigurator.getGrepCmd());
    std::string awkcmd(theLocalConfigurator.getAwkCmd());
    std::string cmd(vxdmpcmd);
    const std::string VXDMPARGS = "list dmpnode all";
    const std::string DMPDEVTOK = "dmpdev";
    const std::string DMPDEVVALUE = "-F\'=\' \'{ print $2 }\'";

    /* SAMPLE OUTPUT:
    [root@imits216 ~]# vxdmpadm list dmpnode all | grep "dmpdev"
    dmpdev          = disk_0
    dmpdev          = disk_1
    dmpdev          = disk_2
    dmpdev          = disk_3
    dmpdev          = disk_4
    dmpdev          = disk_5
    dmpdev          = disk_6
    */

    cmd += " ";
    cmd += VXDMPARGS;
    cmd += " ";
    cmd += "|";
    cmd += " ";
    cmd += grepcmd;
    cmd += " ";
    cmd += DMPDEVTOK;
    cmd += " ";
    cmd += "|";
    cmd += " ";
    cmd += awkcmd;
    cmd += " ";
    cmd += DMPDEVVALUE;

    std::stringstream results;

    if (executePipe(cmd, results))
    {
        while (!results.eof())
        {
            std::string vxdmpname;
            results >> vxdmpname;
            if (vxdmpname.empty())
            {
                break;
            }
            vxdmpnodes.insert(vxdmpname);
        }
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to execute vxdmpadm\n");
    }
}


bool IsVxDmpNode(
                 const std::string &node,
                 const std::set<std::string> &vxdmpnodes
                )
{
    return (vxdmpnodes.end() != find(vxdmpnodes.begin(), vxdmpnodes.end(), node));
}


std::string GetPartitionID(const std::string &diskid, const std::string &partitionname)
{
    std::string id;

    if (!diskid.empty())
    {
        std::string partitionnumber = GetPartitionNumber(partitionname);
        if (!partitionnumber.empty())
        {
            id = diskid;
            id += SEPINPARTITIONID;
            id += partitionnumber;
        }
    }

    return id;
}


std::string GetPartitionNumber(const std::string &partitionname)
{
    std::string num;
    size_t len = partitionname.length();
    const char *start = partitionname.c_str();
    const char *p = start + (len - 1);
    const char *numstart = NULL;

    while ((p >= start) && (isdigit(*p)))
    {
        numstart = p;
        p--;
    }
  
    if ((p >= start) && numstart)
    {
        num = numstart;
    }

    if (num.empty())
    {
        num = GetPartitionNumberFromNameText(partitionname);
    }

    return num;
}


void GetVxDevToOsName(std::map<std::string, std::string> &vxdevtonativename)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    /* vxdisk -e list */
    const std::string delimiters = "\n";
    const std::string OS_NATIVE_NAME = "OS_NATIVE_NAME";
    /* For linux, works even if -e is given after vxdisk list */
    LocalConfigurator theLocalConfigurator;
    std::string vxdiskcmd(theLocalConfigurator.getVxDiskCmd());
    const std::string VXDISKARGS = "-e list";
    std::string vxdiske = vxdiskcmd;
    vxdiske += " ";
    vxdiske += VXDISKARGS;

    std::stringstream output;
  
    /*
    * [root@imits216 ~]# vxdisk -e list
    * DEVICE       TYPE           DISK        GROUP        STATUS               OS_NATIVE_NAME   ATTR
    * disk_0       auto:cdsdisk   disk_0       oradatadg   online shared        sdl              -
    * disk_1       auto:cdsdisk   -            -           online               sdg              -
    * sda          auto:none      -            -           online invalid       sda              -
    */

    if (!executePipe(vxdiske, output)) 
    {
        /* normally should not come here */
        DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s executePipe failed with errno = %d\n", __LINE__, __FILE__, errno);
        return;
    }

    if (output.str().empty())
    {
        return;
    }
  
    std::string heading;
    std::getline(output, heading);
    std::stringstream headingstream(heading);
    int osnativenamepos = -1;

    for (int i = 0; (!headingstream.eof()) && (-1 == osnativenamepos) ; ++i)
    {
        std::string token;
        headingstream >> token;

        if (token == OS_NATIVE_NAME)
        {
            osnativenamepos = i;
        }
    }

    if (-1 != osnativenamepos)
    {
        while (!output.eof())
        {
            std::string devicestatusline;
            std::getline(output, devicestatusline);
            if (devicestatusline.empty())
            {
                break;
            }
            std::stringstream devicestatuslinestream(devicestatusline);
            std::string enclosurename; 
            devicestatuslinestream >> enclosurename; 
            
            for (int i = 0; (!devicestatuslinestream.eof()) && (i < (osnativenamepos - 1)) ; ++i)
            {
                std::string skip;
                devicestatuslinestream >> skip;
            }

            std::string osname;
            if (!devicestatuslinestream.eof())
            {
                devicestatuslinestream >> osname;
            }

            if (enclosurename.empty() || osname.empty())
            {
                break;
            }
            vxdevtonativename[enclosurename] = osname; 
        }
    }
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


bool IsDriveUnderVxVM(const std::string &status)
{
    /* 
    * Decision made:
    * 1. For a device to be under vxvm control, 
    *    first status flag should be "online" and second 
    *    should not be "invalid". This is because vxvm docs
    *    themselves mention online invalid status as disk not 
    *    under vxvm control.
    * 2. Consider cases like the disk is under LVM, ZFS or ASM,
    *    vxvm is showing in status as LVM, ZFS or ASM. Hence above
    *    algorithm works.
    * 3. Consider a new disk added that does not have label then
    *    vxvm shows status as "nolabel". Hence the algorithm works
    * 4. For a disk that vxvm thinks is corrupted, it shows
    *    status as "error". Hence the algorithm works
    * 5. For shared disks that are under vxvm, status is shown as 
    *    "online shared". Hence the algorithm works. 
    */
    std::stringstream statusstream(status);
    std::string firstflag, secondflag;
    statusstream >> firstflag >> secondflag;
    return ("online" == firstflag) && ("invalid" != secondflag);
}


/**
*
* vol just is the volume name not the path
*
*/

/** 
    *
    * The algorithm is:
    * 1. Form the following command that lists the vsets in all disk groups
    *    vxvset list | awk 'NR != 1' | awk '{ print $1 }'
    * 
    * root@IMITSF1 # vxvset list
    * NAME            GROUP            NVOLS  CONTEXT
    * myvset          IMITSF1              1  -
    * yourvset        IMITSF1              1  -
    * root@IMITSF1 # vxvset list | awk 'NR != 1' | awk '{ print $1 }'
    * myvset
    * yourvset
    * root@IMITSF1 # 
    *
    * 2. Compare the arguement with the above list
    * 
*/
bool IsVxVMVset(const std::string vol, const std::vector<std::string> &vxvmvsets)
{
    bool isvxvmvset = (vxvmvsets.end() != find(vxvmvsets.begin(), vxvmvsets.end(), vol));     
    if (isvxvmvset)
    {
        DebugPrintf(SV_LOG_DEBUG, "@LINE %d in FILE %s, %s is a vxvm vset\n", 
                                  __LINE__,  __FILE__, vol.c_str()); 
        isvxvmvset = true;
    }
    return isvxvmvset;
}


void GetVxVMVSets(std::set<dev_t> &vxvmvsets)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    LocalConfigurator theLocalConfigurator;
    std::string vxvsetcmd(theLocalConfigurator.getVxVsetCmd());
    std::string awkcmd(theLocalConfigurator.getAwkCmd());
    const std::string VXVSETARGS = "list";
    const std::string SKIPFIRSTLINE = "\'NR != 1\'";
    const std::string ALLOWVSETANDGRP = "\'{ print $1 \" \" $2 }\'";
    std::string listvxvsetscmd = vxvsetcmd;
    listvxvsetscmd += " ";
    listvxvsetscmd += VXVSETARGS;
    listvxvsetscmd += " ";
    listvxvsetscmd += "|";
    listvxvsetscmd += " ";
    listvxvsetscmd += awkcmd;
    listvxvsetscmd += " ";
    listvxvsetscmd += SKIPFIRSTLINE;
    listvxvsetscmd += " ";
    listvxvsetscmd += "|";
    listvxvsetscmd += " ";
    listvxvsetscmd += awkcmd;
    listvxvsetscmd += " ";
    listvxvsetscmd += ALLOWVSETANDGRP;

    std::stringstream vxvsets;

    if (!executePipe(listvxvsetscmd, vxvsets)) {
        DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s executePipe failed with errno = %d\n", __LINE__, __FILE__, errno);
        return;
    }

    /*
    [root@imits216 ~]# vxvset list
    NAME            GROUP            NVOLS  CONTEXT
    */

    while (!vxvsets.eof()) {
        std::string eachvxvset;
        std::string vxvsetbasename;
        std::string groupofvxvset;

        vxvsets >> vxvsetbasename;

        if (vxvsetbasename.empty()) 
        {
            break;
        }
    
        vxvsets >> groupofvxvset;
        
        if (groupofvxvset.empty())
        {
            break;
        }
 
        eachvxvset = VXVMDSKDIR;
        eachvxvset += UNIX_PATH_SEPARATOR;
        eachvxvset += groupofvxvset;
        eachvxvset += UNIX_PATH_SEPARATOR;
        /* To be safe use basename */
        eachvxvset += BaseName(vxvsetbasename);

        struct stat64 vsetStat;
        if ((0 == stat64(eachvxvset.c_str(), &vsetStat)) && vsetStat.st_rdev)
        {
            vxvmvsets.insert(vsetStat.st_rdev);
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


std::string GetNativeDiskDir(void)
{
    return NATIVEDISKDIR;
}


void GetDevtToDeviceName(const std::string &dir, DevtToDeviceName_t &devttoname)
{
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;

    dentresult = NULL; 
    dirp = opendir(dir.c_str());
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
                    std::string device = dir;
                    device += UNIX_PATH_SEPARATOR;
                    device += dentp->d_name;
                    struct stat64 devstat;
                    if ((0 == stat64(device.c_str(), &devstat)) && devstat.st_rdev)
                    {
                        devttoname.insert(std::make_pair(devstat.st_rdev, device));
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
        closedir(dirp); 
    } /* end of if (dirp) */
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "unable to open directory %s\n", dir.c_str());
    }
}


bool IsReadable(const int &fd, const std::string &devicename, 
                const unsigned long long nsecs, const unsigned long long blksize,
                const E_VERIFYSIZEAT everifysizeat)
{
    /* DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with devicename %s\n", FUNCTION_NAME, devicename.c_str()); */
    bool bretval = false;
    off_t lseekrval = -1;
    char *buf = NULL;
    std::stringstream error;
    unsigned long long size = nsecs * blksize;

    off_t savecurrpos = lseek(fd, 0, SEEK_CUR);
    if ((off_t)-1 != savecurrpos)
    {
        off_t seekoff = (E_ATLESSTHANSIZE == everifysizeat) ? (size - blksize) : size;
        lseekrval = lseek(fd, seekoff, SEEK_SET);
   
        if (seekoff == lseekrval)
        {
           buf = (char *)malloc(blksize);
           if (buf)
           {
               ssize_t bytesread;
               bytesread = read(fd, buf, blksize); 
               if ((-1 != bytesread) && (bytesread == blksize))
               {
                   bretval = true;
               }
               else
               {
                   error << "read on device " << devicename << " from " << seekoff; 
                   error << " failed with errno = " << errno << '\n';
                   /* DebugPrintf(SV_LOG_DEBUG, error.str().c_str()); */
               }
               free(buf);    
           }
           else
           {
               error << "malloc failed with errno = " << errno << '\n';
               DebugPrintf(SV_LOG_ERROR, error.str().c_str());
           }
           off_t savebackpos = lseek(fd, savecurrpos, SEEK_SET);
           if (savebackpos != savecurrpos)
           {
               bretval = false;
               error << "lseek on device " << devicename << " to save back original position failed with errno = " << errno << '\n';
               DebugPrintf(SV_LOG_ERROR, error.str().c_str());
           }
        }
        else
        {
            error << "lseek on device " << devicename << " to " << seekoff; 
            error << " failed with errno = " << errno << '\n';
            /* DebugPrintf(SV_LOG_WARNING, error.str().c_str()); */
        }
    }
    else
    {
        error << "lseek on device " << devicename << " to save existing position failed with errno = " << errno << '\n';
        DebugPrintf(SV_LOG_ERROR, error.str().c_str());
    }

    /*
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s. size valid = %s\n",FUNCTION_NAME, 
                bretval?"true":"false");
    */

    return bretval;
}


bool GetModelVendorFromAtaCmd(const std::string &devicename, std::string &vendor, std::string &model)
{
    AtaCommandIssuer aci;
    if (!aci.StartSession(devicename))
    {
        return false;
    }

    uint8_t resp[LENOFATACMD + LENOFATADATA] = {0};
    bool bgot = false;

    resp[0] = IDENTIFYATACODE;
    resp[3] = 1;
  
    int execstatus = aci.Issue(resp);
    if (0 != execstatus)
    {
        memset(resp, 0, sizeof resp);
        resp[0] = PIDENTIFYATACODE;
        resp[3] = 1;
        execstatus = aci.Issue(resp);
    }
    
    if (0 == execstatus)
    {
        uint16_t *data = (uint16_t *)(resp + LENOFATACMD);
        if (!IsLittleEndian())
        {
            /* TODO: not sure how to support mixed endian machine (NUXI machines) */
            unsigned int len = LENOFATADATA / 2;
            for (int i = 0; i < len; ++i)
                swap16bits(&data[i]);
        }
       
        uint16_t *pmodel = data+27;
        uint8_t *pchar;
        for (int i = 0; i < 20; i++)
        {
            /* TODO: understand this */
            pchar = (uint8_t *)(pmodel+i);
            model.push_back(*(pchar+1));
            model.push_back(*pchar);
        }

        /* sometimes, '\0' are being pushed.
         * this is to avoid nulls */
        if (model.size() && ('\0'==model[0]))
        {
            model.clear();
        }

        /* TODO: we get OUI number from this cmd 
         * but donot know how to get manufacturer
         * name from OUI ? */
        bgot = true;
    }
    aci.EndSession();
     
    return bgot;
}


std::string GetHCTLWithDelimiter(const std::string &hctldelimited, const std::string &delim)
{
    std::string hctl;

    std::vector<std::string> tokens;
    Tokenize(hctldelimited, tokens, delim);
    if (4 == tokens.size())
    {
        hctl += HOSTCHAR;
        hctl += HCTLLABELVALUESEP;
        hctl += tokens[0];
        hctl += HCTLSEP;
     
        hctl += CHANNELCHAR;
        hctl += HCTLLABELVALUESEP;
        hctl += tokens[1];
        hctl += HCTLSEP;
     
        hctl += TARGETCHAR;
        hctl += HCTLLABELVALUESEP;
        hctl += tokens[2];
        hctl += HCTLSEP;

        hctl += LUNCHAR;
        hctl += HCTLLABELVALUESEP;
        hctl += tokens[3];
    }

    return hctl;
}

std::string GetHCTLWithHCTL(const std::string &hctldelimited, const std::string &delim)
{
    std::string hctl;

    std::vector<std::string> tokens;
    Tokenize(hctldelimited, tokens, delim);
    if (4 == tokens.size())
    {
        hctl += HOSTCHAR;
        hctl += tokens[0];
     
        hctl += CHANNELCHAR;
        hctl += tokens[1];
     
        hctl += TARGETCHAR;
        hctl += tokens[2];

        hctl += LUNCHAR;
        hctl += tokens[3];
    }

    return hctl;
}
