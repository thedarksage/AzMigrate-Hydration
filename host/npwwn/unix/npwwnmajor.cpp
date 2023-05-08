#include "npwwn.h"
#include "npwwnminor.h"
#include "logger.h"
#include "portable.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "scsicommandissuer.h"
#include "utildirbasename.h"
#include <sys/types.h> 
#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"

dev_t MakeDevt(const std::string &sdevt, const char &sep)
{
    dev_t devt = 0;

    std::string::const_iterator begin = sdevt.begin();
    std::string::const_iterator end = sdevt.end();
    std::string::const_iterator delimiter = find(begin, end, sep);
    if (sdevt.end() != delimiter)
    {
        std::string dmmajor(begin, delimiter);
        std::string dmminor(delimiter + 1, end);
        int major = 0;
        int minor = 0;
        std::stringstream strmajor(dmmajor);
        std::stringstream strminor(dmminor);
        strmajor >> major;
        strminor >> minor;
        devt = makedev(major, minor);
    }

    return devt;
}


E_LUNMATCHSTATE GetLunMatchState(dev_t devt, const std::string &devdir, 
                                 const std::string &product, const std::string &vendor, 
                                 const SV_ULONGLONG lunnumber, std::string &atlunpath)
{
    DIR *dirp;
    struct dirent *dentp, *dentresult;
    size_t direntsize = sizeof(struct dirent) + PATH_MAX + 1;
    const char *pdir = devdir.c_str();
    E_LUNMATCHSTATE elunmatchstate = LUN_NOVENDORDETAILS;

    std::stringstream msg;
    msg << "devt = " << devt 
        << ", device directory = " << devdir 
        << ", product = " << product 
        << ", vendor = " << vendor 
        << ", lunnumber = " << lunnumber;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n", FUNCTION_NAME, msg.str().c_str());
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
                   strcmp(dentp->d_name, ".."))
               {
                   std::string dev = devdir;
                   dev += UNIX_PATH_SEPARATOR;
                   dev += dentp->d_name;
                   struct stat64 devstat;
                   if ((0 == stat64(dev.c_str(), &devstat)) && devstat.st_rdev && (devstat.st_rdev == devt))
                   {
                       elunmatchstate = GetLunMatchStateForDisk(dev, vendor, product, lunnumber);
                       if (LUN_NAME_MATCHED == elunmatchstate)
                       {
                           atlunpath = dev;
                       }
                       break;
                   }
               } /* end of skipping . and .. */
               memset(dentp, 0, direntsize);
           } /* end of while readdir_r */
           free(dentp);
       } /* endif of if (dentp) */
       //closedir(dirp); 
    } /* end of if (dirp) */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return elunmatchstate;
}


E_LUNMATCHSTATE GetLunMatchStateForDisk(const std::string &dev, const std::string &vendor, 
                                        const std::string &product, const SV_ULONGLONG lunnumber)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    INQUIRY_DETAILS InqDetails;
    InqDetails.device = dev;
    bool bgotstdinq = false;
    E_LUNMATCHSTATE elunmatchstate = LUN_NOVENDORDETAILS;
    ScsiCommandIssuer sci;

    if (sci.StartSession(dev))
    {
        /* TODO: should change this once we add switch agent code for scsi id */
        bgotstdinq = sci.GetStdInqValues(&InqDetails);
        if (bgotstdinq)
        {
            std::string gotvendor, gotproduct;
            gotvendor = InqDetails.GetVendor();
            gotproduct = InqDetails.GetProduct();
            DebugPrintf(SV_LOG_DEBUG, "for device %s, got vendor %s, got product %s\n", 
                                      dev.c_str(), gotvendor.c_str(), gotproduct.c_str());
            DebugPrintf(SV_LOG_DEBUG, "matching against vendor %s, product %s\n", 
                                      vendor.c_str(), product.c_str());
            /* TODO: issue:
             * 1. While matching vendor and product, 
             *    the vendor that agent is getting from AT Lun 
             *    is "InMage  " (2 spaces at last). 
             *    We are doing string comparision with "InMage" and 
             *    this is failing.
             * what has to be done in this case ?
             * we should get the vendor name also from CX instead of hard coded at agent
             * even then how should the comparision be done ?
             *
             * currently trimming " \t" and then comparing; 
             * not comparing with strncmp with "InMage" 
             * because another organisation may match 
             * differing only in last 2 characters
             * 
            */
            Trim(gotvendor, " \t");
            if (gotvendor == vendor)
            {
                elunmatchstate = (gotproduct == product) ? LUN_NAME_MATCHED : LUN_NAME_UNMATCHED;
            }
            else
            {
                elunmatchstate = LUN_VENDOR_UNMATCHED;
            }
        }
        else
        {
            /* since these will be called from delete also,
             * recording as debug instead of warning; 
             * also this gets called for all matching lun 
             * numbers for lpfc, scli */
            DebugPrintf(SV_LOG_DEBUG, "failed to do standard inquiry on device %s\n", dev.c_str());
        }
        sci.EndSession();
    }
    else
    {
        /* since these will be called from delete also,
         * recording as debug instead of warning;
         * also this gets called for all matching lun 
         * numbers for lpfc, scli */
        DebugPrintf(SV_LOG_DEBUG, "failed to open session with scsi command issuer with error %s\n", sci.GetErrorMessage().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return elunmatchstate;
}


void UpdateLunFormationState(const PIAT_LUN_INFO &piatluninfo,
                             const std::string &vendor,
                             const LocalPwwnInfo_t &lpwwn, 
                             RemotePwwnInfo_t &rpwwn)
{
    std::stringstream msg;
    msg << "PI " << lpwwn.m_Pwwn.m_Name
        << ", AT " << rpwwn.m_Pwwn.m_Name
        << ", source LUN ID" << piatluninfo.sourceLUNID;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s with %s\n",
                             FUNCTION_NAME, msg.str().c_str());

    if (!rpwwn.m_ATLunPath.empty())
    {
        std::string basename = BaseName(rpwwn.m_ATLunPath);
        if (!basename.empty())
        {
            rpwwn.m_ATLunState = (UNIX_HIDDENFILE_STARTER == basename[0]) ? 
                                 LUNSTATE_INTERMEDIATE : LUNSTATE_VALID;
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s with source LUN ID: %s\n",
                             FUNCTION_NAME, piatluninfo.sourceLUNID.c_str());
}

