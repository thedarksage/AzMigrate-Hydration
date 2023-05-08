#ifndef _NPWWN__H_
#define _NPWWN__H_

#include <string>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include "svtypes.h"
#include "volumegroupsettings.h"
#include "prismsettings.h"
#include "inmstrcmp.h"
#include "npwwnif.h"

class Event;
#define FC_HOST "fc_host"
#define SCSI_HOST "scsi_host"
#define ISCSI_HOST "iscsi_host"
const char * const SYSCLASSSUBDIRS[] = {FC_HOST, SCSI_HOST, ISCSI_HOST};
const char QLAWWNLASTCHAR  = ';';

typedef std::vector<SV_LONGLONG> Channels_t;

typedef enum eLunState
{
    LUNSTATE_VALID,           /* valid; present -> matches lun number, product, lun name -> unhidden */
    LUNSTATE_INTERMEDIATE,    /* hidden; linux only */
    LUNSTATE_NOTVISIBLE,      /* lun not visible; for linux, entry in sys not present */
    LUNSTATE_NEEDSCLEANUP     /* lun needs cleanup */

} E_LUNSTATE;

typedef enum eLocalLunState
{
    NO_MATCH,
    VENDOR_MATCH,
    VENDOR_LUN_MATCH
} E_LOCALLUNSTATE;

const char * const LunStateStr[] = {"valid", "intermediate", "not visible",
                                    "need cleanup"};

/* pwwn sources: commands or files */
typedef enum ePortWwnSrc
{
    PWWNSRCUNKNOWN,    
    SYSCLASSFCHOST,    /* rhel 5 */
    SYSCLASSSCHOST,    /* rhel 4: lpfc */
    PROCQLA,           /* rhel 4: qla */
    FCINFO,            /* solaris 10 fcinfo*/
    SCLI,              /* solaris 8, 9: scli qla utility */
    LPFC,              /* solaris 8, 9: emlxadm emc utility */
    SYSCLASSISCSIHOST, /* rhel iscsi */
    LSCFG = FCINFO,     /* AIX 6 lscfg*/
    ISCSIADM = SYSCLASSISCSIHOST /* solaris 10 iscsiadm */
     
} E_PORTWWNSRC;

const char * const PwwnSrcStr[] = {"unknown", "fc_host", "scsi_host", 
                                   "qla2xxx", "fcinfo", "scli", "lpfc", "iscsi_host"};

const E_PORTWWNSRC PwwnSrc[] = {SYSCLASSFCHOST, SYSCLASSSCHOST, SYSCLASSISCSIHOST}; 


/* pwwn type: fc or iscsi */
typedef enum ePortWwnType
{
    PWWNTYPEUNKNOWN,
    PWWNTYPEFC,        /* fabric hba wwn */
    PWWNTYPEISCSI      /* iscsi wwn */

} E_PORTWWNTYPE;

const char * const IscsiPwwnPrefixes[] = {"iqn.", "eui.", "naa."};
const char * const PwwnTypeStr[] = {"unknown", "fabric", "iscsi"};

/* attributes of pwwn */
struct PwwnInfo
{
    std::string m_Name;              /* wwn */
    SV_LONGLONG m_Number;            /* linux: host number for local pwwn;
                                      *      : target ID for remote pwwn;
                                      * controller number for fcinfo local pwwn; 
                                      * no significance for emlxadm, scli */
    E_PORTWWNTYPE m_Type;            /* specifies fc or iscsi for now */
    std::string m_Nwwn;              /* every pwwn has nwwn */
    std::string m_FormattedNwwn;     /* formatted npwwn */
    std::string controller;          /* Controller name ex: c2, c15 for Sol 9, 8;
                                      * qlogic and emulex */

    PwwnInfo()
    {
        m_Number = -1;
        m_Type = PWWNTYPEUNKNOWN;
    }
};
typedef PwwnInfo PwwnInfo_t;


/* remote pwwn for a local pwwn */
struct RemotePwwnInfo
{
    std::string m_FormattedName;    /* universal exchange format of wwn; this is the key */
    PwwnInfo_t m_Pwwn;
    bool m_IsSentFromCX;            /* presence of this remote wwn in prism settings */
    SV_LONGLONG m_ChannelNumber;    /* linux: channel from which AT Lun path is visible */
    std::string m_ATLunPath;        /* discovered AT Lun path */
    E_LUNSTATE m_ATLunState;        /* at lun state */
    E_LOCALLUNSTATE m_LocalMatchState;
    SV_LONGLONG m_LocalChannelNumber;
    SV_LONGLONG m_RemoteNoMatch; 

    RemotePwwnInfo()
    {
        m_IsSentFromCX = false;
        m_ChannelNumber = -1;
        m_ATLunState = LUNSTATE_NOTVISIBLE;
    }
};
typedef RemotePwwnInfo RemotePwwnInfo_t;
/* key from formatted wwn to rpwwn; 
 * ignore case comparision for formatted pwwn */
typedef std::map<std::string, RemotePwwnInfo_t, InmLessNoCase> RemotePwwnInfos_t; 
typedef std::pair<const std::string, RemotePwwnInfo_t> RemotePwwnPair_t;
typedef std::vector<RemotePwwnInfo_t> RemotePwwnData_t;
typedef RemotePwwnData_t::const_iterator ConstRemotePwwnDataIter_t;
typedef std::map<std::string, RemotePwwnData_t, InmLessNoCase> MultiRemotePwwnData_t;
typedef MultiRemotePwwnData_t::const_iterator ConstMultiRemotePwwnDataIter_t;

class RemotePwwnInfoComp
{
    public:
        bool operator()(const RemotePwwnInfo_t &rpwwnl, const RemotePwwnInfo_t &rpwwnr)
        {
            return (rpwwnl.m_Pwwn.m_Number < rpwwnr.m_Pwwn.m_Number);
        }
};


/* local pwwn information */
struct LocalPwwnInfo
{
    std::string m_FormattedName;            /* universal exchange format of wwn; this is the key */
    PwwnInfo_t m_Pwwn;
    E_PORTWWNSRC m_PwwnSrc;
    RemotePwwnInfos_t m_RemotePwwnInfos;    /* collection of remote Pwwns seen from this local pwwn */

    LocalPwwnInfo()
    {
        m_PwwnSrc = PWWNSRCUNKNOWN;
    }
};
typedef LocalPwwnInfo LocalPwwnInfo_t;
/* key from formatted wwn to rpwwn; 
 * ignore case comparision for formatted pwwn */
typedef std::multimap<std::string, LocalPwwnInfo_t, InmLessNoCase> LocalPwwnInfos_t;
typedef std::pair<const std::string, LocalPwwnInfo_t> LocalPwwnPair_t;

typedef enum eLunMatchState
{
    LUN_NOVENDORDETAILS,    /* covers failures to find lun, open, inquiry and so on */
    LUN_VENDOR_UNMATCHED,   /* vendor is different from InMage */
    LUN_NAME_UNMATCHED,     /* vendor matches but lun name does not */
    LUN_NAME_MATCHED        /* vendor, lun name matches */

} E_LUNMATCHSTATE;

const char * const LunMatchStateStr[] = {"unable to find vendor", "vendor unmatched", 
                                         "lun name unmatched", "lun name matched"};

void InsertLocalPwwn(LocalPwwnInfos_t &lpwwns, const LocalPwwnInfo_t &lpwwn);
void InsertRemotePwwn(RemotePwwnInfos_t &rpwwns, const RemotePwwnInfo_t &rpwwn);
void InsertRemotePwwnData(MultiRemotePwwnData_t &rpwwns, const RemotePwwnInfo_t &rpwwn);

/* TODO: made a common define since 
 *       if there has to be changes, 
 *       just change the common value; 
 *       On testing, if needed the values 
 *       can be separated */
#define NSECS_TO_WAIT_AFTER_SCANDELETE 3
#define NSECS_TO_WAIT_AFTER_SCAN NSECS_TO_WAIT_AFTER_SCANDELETE
#define NSECS_TO_WAIT_AFTER_DELETE NSECS_TO_WAIT_AFTER_SCANDELETE

/* This calls all the wwn collection methods, 
 * which stops once match for local ports are
 * established and then collect all remote pwwns.
 * This has to be done to avoid cases where the 
 * pwwn numbers can change ... across reboots or
 * in some case may be without reboots */
void GetRemotePwwns(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);

void GetRemotePwwnsFromUnknownSrc(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);

void GetRemotePwwnsFromFcHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);

void GetRemotePwwnsFromScsiHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);

void GetRemotePwwnsFromQla(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);

void GetRemotePwwnsFromFcInfo(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);

void GetRemotePwwnsFromScli(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);

void GetRemotePwwnsFromLpfc(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);

void GetRemotePwwnsFromIScsiHost(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);

typedef void (*GetRemotePwwns_t)(LocalPwwnInfo_t *pLocalPwwnInfo, QuitFunction_t &qf);
const GetRemotePwwns_t GetRemotePwwnsFuns[] = {
                                               GetRemotePwwnsFromUnknownSrc, GetRemotePwwnsFromFcHost, GetRemotePwwnsFromScsiHost, 
                                               GetRemotePwwnsFromQla, GetRemotePwwnsFromFcInfo, GetRemotePwwnsFromScli, 
                                               GetRemotePwwnsFromLpfc, GetRemotePwwnsFromIScsiHost
                                              };

void DiscoverRemoteFcTargets();

void DiscoverRemoteIScsiTargets(std::list<std::string> networkAddresses);

void GetATLunPathFromUnknownSrc(const PIAT_LUN_INFO &prismvolumeinfo,
                                const LocalPwwnInfo_t &lpwwn,
                                const bool &bshouldscan,
                                Channels_t &channelnumbers,
                                QuitFunction_t &qf,
                                RemotePwwnInfo_t &rpwwn);

void GetATLunPathFromFcHost(const PIAT_LUN_INFO &prismvolumeinfo,
                            const LocalPwwnInfo_t &lpwwn,
                            const bool &bshouldscan,
                            Channels_t &channelnumbers,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn);

void GetATLunPathFromScsiHost(const PIAT_LUN_INFO &prismvolumeinfo,
                              const LocalPwwnInfo_t &lpwwn,
                              const bool &bshouldscan,
                              Channels_t &channelnumbers,
                              QuitFunction_t &qf,
                              RemotePwwnInfo_t &rpwwn);

void GetATLunPathFromQla(const PIAT_LUN_INFO &prismvolumeinfo,
                         const LocalPwwnInfo_t &lpwwn,
                         const bool &bshouldscan,
                         Channels_t &channelnumbers,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn);

void GetATLunPathFromFcInfo(const PIAT_LUN_INFO &prismvolumeinfo,
                            const LocalPwwnInfo_t &lpwwn,
                            const bool &bshouldscan,
                            Channels_t &channelnumbers,
                            QuitFunction_t &qf,
                            RemotePwwnInfo_t &rpwwn);

void GetATLunPathFromScli(const PIAT_LUN_INFO &prismvolumeinfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn);

void GetATLunPathFromLpfc(const PIAT_LUN_INFO &prismvolumeinfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn);

void GetATLunPathFromIScsiHost(const PIAT_LUN_INFO &prismvolumeinfo,
                          const LocalPwwnInfo_t &lpwwn,
                          const bool &bshouldscan,
                          Channels_t &channelnumbers,
                          QuitFunction_t &qf,
                          RemotePwwnInfo_t &rpwwn);

typedef void (*GetATLunPathFromPIAT_t)(const PIAT_LUN_INFO &prismvolumeinfo,
                                       const LocalPwwnInfo_t &lpwwn,
                                       const bool &bshouldscan,
                                       Channels_t &channelnumbers,
                                       QuitFunction_t &qf,
                                       RemotePwwnInfo_t &rpwwn);

const GetATLunPathFromPIAT_t GetATLunPathFromPIAT[] = {
                                                       GetATLunPathFromUnknownSrc, GetATLunPathFromFcHost, GetATLunPathFromScsiHost, 
                                                       GetATLunPathFromQla, GetATLunPathFromFcInfo, GetATLunPathFromScli, 
                                                       GetATLunPathFromLpfc, GetATLunPathFromIScsiHost
                                                      };

void DeleteATLunFromUnknownSrc(const PIAT_LUN_INFO &prismvolumeinfo, 
                               const LocalPwwnInfo_t &lpwwn, 
                               Channels_t &channels,
                               QuitFunction_t &qf,
                               RemotePwwnInfo_t &rpwwn);

void DeleteATLunFromFcHost(const PIAT_LUN_INFO &prismvolumeinfo, 
                           const LocalPwwnInfo_t &lpwwn, 
                           Channels_t &channels,
                           QuitFunction_t &qf,
                           RemotePwwnInfo_t &rpwwn);

void DeleteATLunFromScsiHost(const PIAT_LUN_INFO &prismvolumeinfo, 
                             const LocalPwwnInfo_t &lpwwn, 
                             Channels_t &channels,
                             QuitFunction_t &qf,
                             RemotePwwnInfo_t &rpwwn);

void DeleteATLunFromQla(const PIAT_LUN_INFO &prismvolumeinfo, 
                        const LocalPwwnInfo_t &lpwwn, 
                        Channels_t &channels,
                        QuitFunction_t &qf,
                        RemotePwwnInfo_t &rpwwn);

void DeleteATLunFromFcInfo(const PIAT_LUN_INFO &prismvolumeinfo, 
                           const LocalPwwnInfo_t &lpwwn, 
                           Channels_t &channels,
                           QuitFunction_t &qf,
                           RemotePwwnInfo_t &rpwwn);

void DeleteATLunFromScli(const PIAT_LUN_INFO &prismvolumeinfo, 
                         const LocalPwwnInfo_t &lpwwn, 
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn);

void DeleteATLunFromLpfc(const PIAT_LUN_INFO &prismvolumeinfo, 
                         const LocalPwwnInfo_t &lpwwn, 
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn);

void DeleteATLunFromIScsiHost(const PIAT_LUN_INFO &prismvolumeinfo, 
                         const LocalPwwnInfo_t &lpwwn, 
                         Channels_t &channels,
                         QuitFunction_t &qf,
                         RemotePwwnInfo_t &rpwwn);

typedef void (*DeleteATLunFromPIAT_t)(const PIAT_LUN_INFO &prismvolumeinfo,
                                      const LocalPwwnInfo_t &lpwwn,
                                      Channels_t &channels,
                                      QuitFunction_t &qf,
                                      RemotePwwnInfo_t &rpwwn);

const DeleteATLunFromPIAT_t DeleteATLunFromPIAT[] = {
                                                     DeleteATLunFromUnknownSrc, DeleteATLunFromFcHost, DeleteATLunFromScsiHost, 
                                                     DeleteATLunFromQla, DeleteATLunFromFcInfo, DeleteATLunFromScli, 
                                                     DeleteATLunFromLpfc, DeleteATLunFromIScsiHost
                                                    };

void GetWwnFromNVPair(
                      const std::string &namevaluepair,
                      const std::string &sep,
                      std::string &value
                     );

void InsertNPWwn(const NwwnPwwn_t &npwwn, NwwnPwwns_t &npwwns);

void PrintNPwwn(const NwwnPwwn &npwwn);

void GetChannelNumbers(Channels_t &ChannelNumbers);

void GetATLunPaths(const PIAT_LUN_INFO &prismvolumeinfo, 
                   const bool &bshouldscan, 
                   QuitFunction_t &qf,
                   LocalPwwnInfos_t &lpwwns, 
                   ATLunNames_t &atlundirectpaths);

void CopyATLunNames(const LocalPwwnInfos_t &lpwwns, 
                    const ATLunNames_t &atlundirectpaths, 
                    ATLunNames_t &atlunnames);

SV_ULONGLONG GetCountOfATLuns(const LocalPwwnInfos_t &lpwwns, 
                              const ATLunNames_t &atlundirectpaths);

void GetATLunPathsFromPIList(const PIAT_LUN_INFO &prismvolumeinfo, 
                             const bool &bshouldscan,
                             QuitFunction_t &qf,
                             LocalPwwnInfos_t &lpwwns, 
                             std::set<std::string> &atlundirectpaths);

void GetATLunPathsFromPI(const PIAT_LUN_INFO &prismvolumeinfo, 
                         const bool &bshouldscan, 
                         QuitFunction_t &qf, 
                         LocalPwwnInfo_t &lpwwn);

dev_t MakeDevt(const std::string &sdevt, const char &sep);

E_LUNMATCHSTATE GetLunMatchState(dev_t devt, const std::string &devdir, 
                                 const std::string &product, const std::string &vendor, 
                                 const SV_ULONGLONG lunnumber, std::string &atlunpath);

E_LUNMATCHSTATE GetLunMatchStateForDisk(const std::string &dev, const std::string &vendor, 
                                        const std::string &product, const SV_ULONGLONG lunnumber);

void UpdateLunFormationState(const PIAT_LUN_INFO &prismvolumeinfo,
                             const std::string &vendor,
                             const LocalPwwnInfo_t &lpwwn, 
                             RemotePwwnInfo_t &rpwwn);

void DeleteATLuns(const PIAT_LUN_INFO &prismvolumeinfo,
                  LocalPwwnInfos_t &lpwwns, 
                  QuitFunction_t &qf);

void DeleteATLunsFromPI(const PIAT_LUN_INFO &prismvolumeinfo, 
                        LocalPwwnInfo_t &lpwwn, 
                        QuitFunction_t &qf);

void GetATLunPathsFromCumulation(const PIAT_LUN_INFO &prismvolumeinfo, 
                                 const bool &bshouldscan,
                                 QuitFunction_t &qf, 
                                 LocalPwwnInfos_t &lpwwns, 
                                 std::set<std::string> &atlundirectpaths);

bool DeleteATLunsFromCumulation(const PIAT_LUN_INFO &prismvolumeinfo, 
                                QuitFunction_t &qf, 
                                LocalPwwnInfos_t &lpwwns);

void PrintLocalPwwn(const LocalPwwnPair_t &lpwwnpair);
void PrintRemotePwwn(const RemotePwwnPair_t &rpwwnpair);
void PrintPwwn(const PwwnInfo_t &pwwn);

void FormatWwn(const E_PORTWWNSRC epwwnsrc, const std::string &unformattedwwn, std::string &formattedwwn, 
               E_PORTWWNTYPE *pepwwntype);

const char FORMATTED_WWN_SEP = ':';
#define CHARSTOTRIMFROMWWN "\"\';"
bool IsIscsiPwwn(const std::string &pwwn);

template<class WWNTYPE>
void FormatUniversalWwn(const WWNTYPE *pwwnnumber, std::string &formattedwwn);
template<class WWNTYPE>
void FormUniversalWwn(const std::string &unformattedwwn, WWNTYPE *pwwnnumber);
template<class WWNTYPE>
bool IsWwnFormatted(const std::string &wwn);

template<class WWNTYPE>
bool IsWwnFormatted(const std::string &wwn)
{
    bool bisformatted = false;
    /* DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with wwn: %s\n", FUNCTION_NAME, wwn.c_str()); */

    const std::string sep(1, FORMATTED_WWN_SEP);
    std::vector<std::string> wwnbytes;
    Tokenize(wwn, wwnbytes, sep);
    size_t ntoks = wwnbytes.size();
    /*
    std::stringstream ssntoks;
    ssntoks << ntoks;
    DebugPrintf(SV_LOG_DEBUG, "For wwn %s, number of bytes is %s\n", 
                              wwn.c_str(), ssntoks.str().c_str());
    */

    if (sizeof (WWNTYPE) == ntoks)
    {
        bisformatted = (wwnbytes.end() == find_if(wwnbytes.begin(), 
                                                  wwnbytes.end(),
                                                  IsNotxString));
        if (bisformatted)
        {
            std::string wwncopy = wwn;
            WWNTYPE wwnnumber = 0;
            RemoveChar(wwncopy, FORMATTED_WWN_SEP);
            std::stringstream sswwnnumber(wwncopy);
            sswwnnumber >> std::hex >> wwnnumber;
            if (wwnnumber)
            {
                /*
                std::stringstream ssnumber;
                ssnumber << wwnnumber;
                DebugPrintf(SV_LOG_DEBUG, "from wwn %s, the wwn number is %s. "
                                          "Considering this as formatted\n",
                                          wwn.c_str(), ssnumber.str().c_str());
                */
            }
            else
            {
                bisformatted = false;
                DebugPrintf(SV_LOG_WARNING, "could not convert wwn %s to number\n", wwn.c_str());
            }
        } 
        else
        {
            DebugPrintf(SV_LOG_WARNING, "bytes in wwn %s are not in hexadecimal format\n", wwn.c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "could not tokenize %s with delimiter %s\n", 
                                  wwn.c_str(), sep.c_str());
    }

    /* DebugPrintf(SV_LOG_DEBUG, "EXITED %s with wwn: %s\n", FUNCTION_NAME, wwn.c_str()); */
    return bisformatted;
}


template<class WWNTYPE>
void FormUniversalWwn(const std::string &unformattedwwn, WWNTYPE *pwwnnumber)
{
    /*
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with unformattedwwn %s\n", FUNCTION_NAME, 
                              unformattedwwn.c_str());
    */

    std::stringstream ss(unformattedwwn);
    ss >> std::hex >> (*pwwnnumber);

    /*
    std::stringstream ssnumber;
    ssnumber << (*pwwnnumber);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with unformattedwwn %s, "
                              "wwn number %s\n", FUNCTION_NAME, 
                              unformattedwwn.c_str(), ssnumber.str().c_str());
    */
}


template<class WWNTYPE>
void FormatUniversalWwn(const WWNTYPE *pwwnnumber, std::string &formattedwwn)
{
    std::stringstream ss;
    ss << std::hex; /* TODO: use std::uppercase iomanip to print wwn in upper case hex */
    ss << std::setfill('0') << std::setw(2 * sizeof (*pwwnnumber));
    ss << (*pwwnnumber);

    const std::string &strwwn = ss.str();
    /* need not worry since size of strwwn is even, 
     * we will not cross rend; since multipled by 2 above */
    for (size_t i = 0; i < strwwn.size(); /* empty */ )
    {
        formattedwwn.push_back(strwwn[i++]);
        formattedwwn.push_back(strwwn[i++]);
        if (i < strwwn.size())
        {
            formattedwwn.push_back(FORMATTED_WWN_SEP);
        }
    }
}

typedef SV_ULONGLONG WWNFC_t;

void GetLocalPwwnInfos(LocalPwwnInfos_t &lpwwns);
void CopyLocalPwwnInfos(const LocalPwwnInfos_t &lpwwns, NwwnPwwns_t &npwwns);
bool AreLpwwnsCumulative(const LocalPwwnInfos_t &lpwwns);
SV_ULONGLONG GetCountOfIndividualATLuns(const LocalPwwnInfos_t &lpwwns);

#endif /* _NPWWN__H_ */

