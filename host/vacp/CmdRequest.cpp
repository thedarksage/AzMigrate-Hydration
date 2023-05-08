#include "stdafx.h"

using namespace std;

#include "util.h"

#include "CmdRequest.h"
#include "VssRequestor.h"

#include "VssRequestor.h"
#include "VssWriter.h"
#include "StreamEngine/InvoltTypes.h"
#include "StreamEngine/StreamRecords.h"
#include "StreamEngine/StreamEngine.h"
#include "vacp.h"
#include "VacpConf.h"
#include "Communicator.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "VacpErrorCodes.h"
#include "TagTelemetryKeys.h"

#include "..\common\win32\Failoverclusterinfocollector.h"


using namespace TagTelemetry;
using namespace TagTelemetry::NsVacpOutputKeys;

extern bool		gbRemoteSend;
extern char		gszFileSystemTag[1024];
extern bool		gbIssueTag;
extern bool		gbVerify;
extern string volumePathNames[MAX_PATH];
const string gstrInMageVssProviderGuid = string("3EB85257-E0B0-4788-A080-D39AA6A515AF");
extern bool    gbUseInMageVssProvider;
extern bool gbIncludeAllApplications;
extern bool gbEnumSW;
extern bool gbSystemLevelTag;

//Tag Life Time related extern variables
extern unsigned long long    gulLTMins;//LT:LifeTime in minutes
extern unsigned long long    gulLTHours;//60 minutes
extern unsigned long long    gulLTDays;//24 hours
extern unsigned long long    gulLTWeeks;//7 days
extern unsigned long long    gulLTMonths;//30 days
extern unsigned long long    gulLTYears ;//365 days

extern unsigned long long gullLifeTime;
extern bool    gbTagLifeTime;
extern bool    gbLTForEver;
extern bool    gbSkipUnProtected;
extern std::vector<Writer2Application_t> vDynamicVssWriter2Apps;

extern std::string g_strRecvdTagGuid;
extern bool g_bDistributedVacp;
extern bool g_bMasterNode;
extern bool g_bThisIsMasterNode;
extern std::string g_strMasterHostName;
extern bool g_bDistMasterFailedButHasToFacilitate;

extern bool gbUseDiskFilter;
extern bool gbBaselineTag;
extern std::string gStrBaselineId;

extern std::string gStrTagGuid;

extern DRIVER_VERSION g_inmFltDriverVersion;
extern FailMsgList g_VacpFailMsgsList;

extern void inm_printf(const char * format, ...);


HRESULT ACSRequestGenerator::GenerateACSRequest(CLIRequest_t &CLIRequest, ACSRequest_t &Request, TAG_TYPE TagType)
{
   HRESULT hr = S_OK;
   DWORD volumeBitMask = 0;
   std::string errmsg;
   do
   {
       XDebugPrintf("\nGenerateACSRequest() Enter\n");

       if (IS_APP_TAG(TagType))
       {
           hr = ValidateCLIRequest(CLIRequest);

           if (hr != S_OK)
               break;
       }

       hr = FillACSRequest(CLIRequest, Request, TagType);
       if (hr != S_OK)
       {
           std::stringstream ss;
           ss << "GenerateACSRequest: FillACSRequest() Failed. Error: ";
           ss << GetVacpLastError(TagType).m_errMsg;
           AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, ss.str().c_str(), GetVacpLastError(TagType).m_errorCode, TagType);
           break;
       }

        if(gbSkipUnProtected && !IS_CRASH_TAG(TagType))
        {
            if (gbUseDiskFilter)
            {
                if (DISKFLT_MV3_TAG_PRECHECK_SUPPORT != (g_inmFltDriverVersion.ulDrMinorVersion3 & DISKFLT_MV3_TAG_PRECHECK_SUPPORT))
                {
                    //Identify if there are any protected disks 
                    std::set<std::string>::iterator iterDisk = Request.vDisks.begin();
                    for (; iterDisk != Request.vDisks.end(); iterDisk++)
                    {
                        if (IsThisDiskProtected(*iterDisk, Request.m_diskGuidMap))
                        {
                            Request.vProtectedDisks.insert((*iterDisk));
                        }
                    }

                    if (0 == Request.vProtectedDisks.size())
                    {
                        std::string errmsg = "There are no Protected Disks[VACP_E_NO_PROTECTED_DISKS]";
                        inm_printf("\nError:%s.\n", errmsg.c_str());
                        hr = VACP_NO_PROTECTED_VOLUMES;
                        AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, hr, TagType);
                        break;
                    }
                }
            }
            else
            {
                //Identify if there are any protected volumes 
                std::vector<std::string>::iterator iterVol = Request.vVolumes.begin();
                for(;iterVol != Request.vVolumes.end();iterVol++)
                {
                    if(IsThisVolumeProtected((*iterVol)))
                    {
                        Request.vProtectedVolumes.push_back((*iterVol));
                    }
                }
                std::vector<VOLMOUNTPOINT>::iterator iterVolMp = Request.volMountPoint_info.vVolumeMountPoints.begin();
                for(;iterVolMp != Request.volMountPoint_info.vVolumeMountPoints.end();iterVolMp++)
                {
                    if(IsThisVolumeProtected((*iterVolMp).strVolumeName))
                    {
                        Request.vProtectedVolumes.push_back((*iterVolMp).strVolumeName);
                    }
                }
          
                if (0 == Request.vProtectedVolumes.size())
                {
                    std::string errmsg = "There are no Protected Volumes[VACP_E_NO_PROTECTED_VOLUMES]";
                    inm_printf("\nError:%s.\n", errmsg.c_str());
                    hr = VACP_NO_PROTECTED_VOLUMES;
                    AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, hr, TagType);
                    break;
                }
            }
        }

        if (IS_APP_TAG(TagType))
        {
            bool bHasValidComponent = false;
            if (Request.Applications.size() > 0)
            {
                auto iter = Request.Applications.begin();
                for (/*empty*/; iter != Request.Applications.end(); iter++)
                {
                    auto iterCmp = iter->m_vComponents.begin();
                    for (/*empty*/; iterCmp != iter->m_vComponents.end(); iterCmp++)
                    {
                        if (iterCmp->bIsSelectable)
                        {
                            bHasValidComponent = true;
                            iter->bHasValidComponent = true;
                            break;
                        }
                    }
                }
            }

            if ((!gbUseDiskFilter) &&
                (0 == Request.volumeBitMask) &&
                (!Request.bDelete) &&
                (!bHasValidComponent))//MCHECK;if components are there without volumes then don't exit.
            {
                std::stringstream ss;
                ss << "ERROR:Zero affected volumes are found. To generate tags, there must be one or more affected volumes, exiting...";
                inm_printf("\n %s \n", ss.str().c_str());
                hr = VACP_NO_PROTECTED_VOLUMES;
                AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, ss.str(), hr, TagType);
                hr = VACP_E_ZERO_EFFECTED_VOLUMES;
                break;
            }
        }
   } while(false);

   return hr;
}

HRESULT ACSRequestGenerator::GenerateTagIdentifiers(ACSRequest_t &Request)
{
    HRESULT hr = S_OK;
    std::string strTagGuid_Id_Name;
    std::string strTagGuid;
    std::string strUniqueId;
    std::stringstream strSystemLevelTagName;
    std::string strTemp;
    std::string strTagName;
    std::vector<std::string>vDistributedBookMarks;
    int nIndex = 0;
    int nTemp = 0;
    DWORD dwWait = 0;
    USHORT TagId;
    GUID tagGuid = GUID_NULL;
    std::string uniqueIdStr;

    do
    {
        if (!Request.bDistributed)
        {
            tagGuid = Request.localTagGuid;
            uniqueIdStr = getUniqueSequence();
        }
        else
        {
            tagGuid = Request.distributedTagGuid;
            uniqueIdStr = Request.distributedTagUniqueId;
        }

        if (gbSystemLevelTag)
        {
            hr = MapApplication2StreamRecType("SystemLevel", TagId);
            if (hr != S_OK)
            {
                DebugPrintf("\nFILE = %s, LINE = %d, MapApplication2StreamRecType() failed for SystemLevel. hr = 0x%x\n", __FILE__, __LINE__, hr);
                return hr;
            }
            //append UniqueIdString to SystemLevel
            strSystemLevelTagName << "SystemLevelTag" << uniqueIdStr;
        }

        if (!IS_CRASH_TAG(Request.TagType))
        {
            for (unsigned int iApp = 0; iApp < Request.Applications.size(); iApp++)
            {
                AppSummary_t &ASum = Request.Applications[iApp];
                DWORD dwDriveMask = ASum.volumeBitMask;

                //If the application does not have any volumes being operated,do not generate any tag for the application.
                if (dwDriveMask == 0)
                    continue;

                if (!gbSystemLevelTag)//this condition is important, otherwise, the systemlevel tag identifier gets overwritten!
                {
                    hr = MapApplication2StreamRecType(ASum.m_appName.c_str(), TagId);
                    if (hr != S_OK)
                    {
                        DebugPrintf("FILE = %s, LINE = %d, MapApplicationName2StreamRecType() failed\n", __FILE__, __LINE__);
                        break;
                    }
                }

                if (ASum.includeAllComponents)
                {
                    char *TagString = new (std::nothrow) char[VACP_MAX_TAG_LENGTH];
                    if (TagString == NULL)
                    {
                        hr = E_FAIL;
                        inm_printf("Error: failed to allocate %d bytes.\n", VACP_MAX_TAG_LENGTH);
                        break;
                    }
                    memset((void*)TagString, 0x00, (size_t)VACP_MAX_TAG_LENGTH);
                    inm_sprintf_s(TagString, VACP_MAX_TAG_LENGTH, "%s%s", ASum.m_appName.c_str(), uniqueIdStr.c_str());

                    if (gbSystemLevelTag)
                    {
                        strSystemLevelTagName << " + " << TagString;
                    }
                    else
                    {
                        (void)BuildTagIdentifiers(Request, TagId, TagString, tagGuid);
                        (void)BuildRevocationTagIdentifiers(Request, TagId, TagString);
                    }

                    delete[] TagString;
                    continue;
                }

                for (unsigned int iComp = 0; iComp < ASum.m_vComponents.size(); iComp++)
                {
                    ComponentSummary_t &CSum = ASum.m_vComponents[iComp];
                    char *TagString = new (std::nothrow) char[VACP_MAX_TAG_LENGTH];
                    if (TagString == NULL)
                    {
                        hr = E_FAIL;
                        inm_printf("Error: failed to allocate %d bytes.\n", VACP_MAX_TAG_LENGTH);
                        break;
                    }
                    memset((void*)TagString, 0x00, (size_t)VACP_MAX_TAG_LENGTH);
                    inm_sprintf_s(TagString, VACP_MAX_TAG_LENGTH, "%s_%s_%s", ASum.m_appName.c_str(), CSum.UserPreferredName.c_str(), uniqueIdStr.c_str());
                    if (gbSystemLevelTag)
                    {
                        strSystemLevelTagName << " + " << TagString;
                    }
                    else
                    {
                        (void)BuildTagIdentifiers(Request, TagId, TagString, tagGuid);
                        (void)BuildRevocationTagIdentifiers(Request, TagId, TagString);
                    }
                    delete[] TagString;
                }
            }
        }

        if (hr != S_OK)
        {
            break;
        }

        if (gbSystemLevelTag)
        {
            (void)BuildTagIdentifiers(Request, TagId, (strSystemLevelTagName.str()).c_str(), tagGuid);
            (void)BuildRevocationTagIdentifiers(Request, TagId, (strSystemLevelTagName.str()).c_str());
        }

        if (!Request.m_hydrationTag.empty())
        {
            hr = MapApplication2StreamRecType("HYDRATION", TagId);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, MapApplicationName2TagId(HYDRATION) failed\n", __FILE__, __LINE__);
                break;
            }
            DebugPrintf("Hydration Tag: %s\n", Request.m_hydrationTag.c_str());
            (void)BuildTagIdentifiers(Request, TagId, Request.m_hydrationTag.c_str(), tagGuid);
        }

        if (!Request.m_clusterTag.empty())
        {
            hr = MapApplication2StreamRecType("CLUSTERINFO", TagId);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, MapApplicationName2TagId(CLUSTERINFO) failed\n", __FILE__, __LINE__);
                break;
            }
            DebugPrintf(" CLUSTERINFO TAG Tag: %s\n", Request.m_clusterTag.c_str());
            (void)BuildTagIdentifiers(Request, TagId, Request.m_clusterTag.c_str(), tagGuid);
        }

        if (!gbBaselineTag)
        {
            hr = MapApplication2StreamRecType("USERDEFINED", TagId);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, MapApplicationName2TagId() failed\n", __FILE__, __LINE__);
                break;
            }

            for (unsigned int iEvent = 0; iEvent < Request.vBookmarkEvents.size(); iEvent++)
            {
                const char *Bookmark = Request.vBookmarkEvents[iEvent];

                if (gbRemoteSend)
                {
                    TAG_IDENTIFIER_AND_DATA  tagIdAndData;
                    tagIdAndData.tagIdentifier = TagId;
                    tagIdAndData.strTagName = Bookmark;
                    Request.vacpServerCmdArg.vTagIdAndData.push_back(tagIdAndData);
                }

                (void)BuildTagIdentifiers(Request, TagId, Bookmark, tagGuid);
                (void)BuildRevocationTagIdentifiers(Request, TagId, Bookmark);
            }
        }

        if (Request.bDistributed)
        {
            for (unsigned int i = 0; i < Request.vDistributedBookmarkEvents.size(); i++)
            {
                bool bFound = false;
                for (unsigned int j = 0; j < Request.vBookmarkEvents.size(); j++)
                {
                    if (!strcmp(Request.vDistributedBookmarkEvents[i], Request.vBookmarkEvents[j]))
                    {
                        bFound = true;
                        break;
                    }
                }

                if (!bFound)
                {
                    const char *Bookmark = Request.vDistributedBookmarkEvents[i];
                    (void)BuildTagIdentifiers(Request, TagId, Bookmark, tagGuid);
                }
            }
        }

        // Generate FS Consistency Tag
        // volumeBitMask will be sent as Tag data.
        // Generate FS consistency tag only when writers are included.
        if (!IS_CRASH_TAG(Request.TagType) && (Request.volumeBitMask > 0) && (Request.DoNotIncludeWriters == false))
        {
            char *fsTag = "FS";
            char *fsTagData = "FileSystem";
            char *dfsTagData = "DistributedFileSystem";//Distributed FS tag from Distributed Vacp

            hr = MapApplication2StreamRecType("FS", TagId);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, MapApplicationName2TagId() failed\n", __FILE__, __LINE__);
                break;
            }

            char *TagString = new (std::nothrow) char[VACP_MAX_TAG_LENGTH];
            if (TagString == NULL)
            {
                inm_printf("Error: failed to allocate %d bytes.\n", VACP_MAX_TAG_LENGTH);
                hr = E_FAIL;
                break;
            }
            memset((void*)TagString, 0x00, (size_t)VACP_MAX_TAG_LENGTH);
            inm_sprintf_s(TagString, VACP_MAX_TAG_LENGTH, "%s%s", fsTagData, uniqueIdStr.c_str());
            (void)BuildTagIdentifiers(Request, TagId, TagString, tagGuid);
            (void)BuildRevocationTagIdentifiers(Request, TagId, TagString);
            delete[] TagString;

            if (Request.bDistributed)
            {
                char *DistributedTagString = new char[VACP_MAX_TAG_LENGTH];
                if (DistributedTagString == NULL)
                {
                    inm_printf("Error: failed to allocate %d bytes.\n", VACP_MAX_TAG_LENGTH);
                    hr = E_FAIL;
                    break;
                }
                memset((void*)DistributedTagString, 0x00, (size_t)VACP_MAX_TAG_LENGTH);
                inm_sprintf_s(DistributedTagString, VACP_MAX_TAG_LENGTH, "%s%s", dfsTagData, uniqueIdStr.c_str());
                (void)BuildTagIdentifiers(Request, TagId, DistributedTagString, tagGuid);
                (void)BuildRevocationTagIdentifiers(Request, TagId, DistributedTagString);
                delete[] DistributedTagString;
            }

#ifdef VACP_SERVER
            ZeroMemory(gszFileSystemTag, sizeof(gszFileSystemTag));
            inm_strcpy_s(gszFileSystemTag, ARRAYSIZE(gszFileSystemTag), TagString);
#endif
            if (gbRemoteSend)
            {
                TAG_IDENTIFIER_AND_DATA  tagIdAndData;
                tagIdAndData.tagIdentifier = TagId;
                tagIdAndData.strTagName = TagString;
                Request.vacpServerCmdArg.vTagIdAndData.push_back(tagIdAndData);
            }

        }

        if (IS_CRASH_TAG(Request.TagType))
        {
            const char *crashTag = "CRASH";
            const char *crashTagData = "CrashTag";
            const char *dcrashTagData = "DistributedCrashTag"; //Distributed Crash Consistent tag from Distributed Vacp

            hr = MapApplication2StreamRecType(crashTag, TagId);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, MapApplicationName2TagId() failed\n", __FILE__, __LINE__);
                break;
            }


            char *TagString = new (std::nothrow)  char[VACP_MAX_TAG_LENGTH];
            if (TagString == NULL)
            {
                hr = S_FALSE;
                break;
            }

            memset((void*)TagString, 0x00, (size_t)VACP_MAX_TAG_LENGTH);
            inm_strcpy_s(TagString, VACP_MAX_TAG_LENGTH, crashTagData);
            inm_strcat_s(TagString, VACP_MAX_TAG_LENGTH, uniqueIdStr.c_str());
            (void)BuildTagIdentifiers(Request, TagId, TagString, tagGuid);
            (void)BuildRevocationTagIdentifiers(Request, TagId, TagString);
            delete[] TagString;

            if (Request.bDistributed)
            {
                char *DistributedTagString = new (std::nothrow)  char[VACP_MAX_TAG_LENGTH];

                if (DistributedTagString == NULL)
                {
                    hr = S_FALSE;
                    break;
                }

                memset((void*)DistributedTagString, 0x00, (size_t)VACP_MAX_TAG_LENGTH);
                inm_strcpy_s(DistributedTagString, VACP_MAX_TAG_LENGTH, dcrashTagData);
                inm_strcat_s(DistributedTagString, VACP_MAX_TAG_LENGTH, uniqueIdStr.c_str());

                (void)BuildTagIdentifiers(Request, TagId, DistributedTagString, tagGuid);
                (void)BuildRevocationTagIdentifiers(Request, TagId, DistributedTagString);
                delete[] DistributedTagString;
            }
        }

        if (gbBaselineTag)
        {
            const char *baselineTag = "BASELINE";
            hr = MapApplication2StreamRecType(baselineTag, TagId);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, MapApplicationName2TagId() failed\n", __FILE__, __LINE__);
                break;
            }
            (void)BuildTagIdentifiers(Request, TagId, gStrBaselineId.c_str(), tagGuid);
            (void)BuildRevocationTagIdentifiers(Request, TagId, gStrBaselineId.c_str());
        }
        if ((Request.volumeBitMask > 0) && (Request.DoNotIncludeWriters == true))
        {
            vector<std::string>::iterator volumeNameIter = Request.vVolumes.begin();
            vector<std::string>::iterator volumeNameEnd = Request.vVolumes.end();
            while (volumeNameIter != volumeNameEnd)
            {
                std::string volname = *volumeNameIter;
                if (!IsVolumeAlreadyExists(Request.vApplyTagToTheseVolumes, volname.c_str()))
                {
                    Request.vApplyTagToTheseVolumes.push_back(volname);
                }
                ++volumeNameIter;
            }

            for (unsigned int i = 0; i < Request.volMountPoint_info.dwCountVolMountPoint; i++)
            {
                Request.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints.push_back(Request.volMountPoint_info.vVolumeMountPoints[i]);
                Request.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint++;
            }
        }
    } while (false);

    //For TCP/IP based solution, we need to generate revocation tag
    //When vacp client is changed to use configurator to communicate with CX, vacp client should not generate revocation tag
    //in fact it is not required as vacp client will send the information to cx in given format defined in vacp_tag_in.h file
    if (hr == S_OK)
    {
        //Generate "revocation" tag -- This special tag is used to delete/cancel already sent tags.
        hr = BuildRevocationTag(Request);
    }
    return hr;
}


/*
HRESULT ACSRequestGenerator::BuildTagIdentifiers(ACSRequest_t &Request, 
                                                 USHORT TagId, 
                                                 const char *TagName, 
                                                 GUID tagGuid, 
                                                 unsigned long long ullTagLifeTime)
A new parameter "unsigned long long ullTagLifeTime" is added. This parameter
will contain the life time of the tag in nanoseconds. By-default its value is 0.
If its value is 0, then normal Vacp Stream with Tag GUID and Tag data will be formed/built.
If ullTagLifeTime is non-zero value then a new record with TagLifeTime Header and TagLifeTime data
will be inserted into the Vacp's Tag stream.

Currently, the Vacp's Tag Stream looks like this...

Vacp Tag Stream = Tag GUID Record(Tag GUID Header + Tag GUID)
                                    + 
                  Tag Data Record (Tag Data Header + Actual Tag Data)

Now,with the introduction of Tag Life Time, the Vacp's Tag Stream will look like this

Vacp Tag Stream = Tag GUID Record(Tag GUID Header + Tag GUID)
                                    + 
                  Tag Data Record (Tag Data Header + Actual Tag Data)
                                    +
                  Tag LifeTime Record (Tag LifeTime Header + Tag LifeTime Data)

*/
HRESULT ACSRequestGenerator::BuildTagIdentifiers(ACSRequest_t &Request, 
                                                 USHORT TagId, 
                                                 const char *TagName, 
                                                 GUID tagGuid)
{
    HRESULT hr = S_OK;
    do
    {
        bool bTagLifeTime = false;
        ULONG ulTagLifeTimeDataLen = 0;
        char szTagLifeTime[65] = {0};//65 is the maximum value that _uitoa can store in the buffer!

        if(0 != gullLifeTime)
        {
            bTagLifeTime = true;
            //since unsigned long long is 8 bytes long!
            //ulTagLifeTimeRecordHeaderLen = sizeof(STREAM_REC_HDR_4B);
            _ui64toa((unsigned long long) gullLifeTime,szTagLifeTime,10);
            ulTagLifeTimeDataLen = (ULONG)strlen((const char*)szTagLifeTime) + 1;
            inm_printf("\n taglifetime in 100 nansecs is %s",szTagLifeTime);
        }
        
        unsigned char* strTagGuid = NULL;
        UuidToString(&tagGuid,(unsigned char**)&strTagGuid);
        //RpcStringFree((unsigned char**)&strTagGuid);
        
        //This Vacp Stream contains atleast two records. If TagLifeTime is there then three records
        //Each Record will contain one Header and one Data field.

        ULONG TagGuidRecLen = 0;//TagGuidRec = TagGuid Header + TagGuid
        ULONG TagDataRecLen = 0;//TagDataRec = TagData Header + TagData
        ULONG TagLifeTimeRecLen = 0;//TagLifeTimeRec = TagLifeTime Header + TagLifeTime Data

        ULONG ulTotalStreamLen = 0;
        ULONG ulTagDataLen = (ULONG)strlen(TagName) + 1;
        ULONG TagGuidLen = (ULONG)strlen((const char*)strTagGuid) + 1;

        StreamEngine *en = new StreamEngine(eStreamRoleGenerator);

        hr = en->GetDataBufferAllocationLength(TagGuidLen,&TagGuidRecLen);//First add the header length for TagGuid Record
        if(hr != S_OK)
        {
            DebugPrintf("\nFILE = %s, LINE = %d, GetDataBufferAllocationLength() failed for Tag GUID\n",__FILE__,__LINE__);
            delete en;
            break;
        }
        if(!bTagLifeTime)
        {
            hr = en->GetDataBufferAllocationLength(ulTagDataLen, &TagDataRecLen);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
                delete en;
                break;
            }
        }
        else
        {
            //Add one more Header for the TagLifeTime
            hr = en->GetDataBufferAllocationLength(ulTagDataLen,&TagDataRecLen);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
                delete en;
                break;
            }

            hr = en->GetDataBufferAllocationLength(ulTagLifeTimeDataLen,&TagLifeTimeRecLen);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
                delete en;
                break;
            }
        }

        if(!bTagLifeTime)
        {
          INM_SAFE_ARITHMETIC(ulTotalStreamLen = InmSafeInt<ULONG>::Type(TagGuidRecLen) + TagDataRecLen, INMAGE_EX(TagGuidRecLen)(TagDataRecLen))
        }
        else
        {
          INM_SAFE_ARITHMETIC(ulTotalStreamLen = InmSafeInt<ULONG>::Type(TagGuidRecLen) + TagDataRecLen + TagLifeTimeRecLen, INMAGE_EX(TagGuidRecLen)(TagDataRecLen)(TagLifeTimeRecLen))
        }

        char *TagData = new char[ulTotalStreamLen];
        memset((void*)TagData,0x00,(size_t)ulTotalStreamLen);

        hr = en->RegisterDataSourceBuffer(TagData, ulTotalStreamLen);
        if (hr != S_OK)
        {
            DebugPrintf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
            delete en;
            break;
        }
        
        if (IS_CRASH_TAG(Request.TagType))
        {
            CrashConsistency::Get().m_tagTelInfo.Add(TagGuidKey, std::string((char*)strTagGuid));
        }
        else if (IS_APP_TAG(Request.TagType))
        {
            AppConsistency::Get().m_tagTelInfo.Add(TagGuidKey, std::string((char*)strTagGuid));
        }
        
        //Generate a Header to store the Tag Guid
        DebugPrintf("\nGenerating Header for TagGuid: %s \n",(char*)strTagGuid);
        hr = en->BuildStreamRecordHeader(STREAM_REC_TYPE_TAGGUID_TAG,TagGuidLen);

        if(hr != S_OK)
        {
            DebugPrintf("\nFILE = %s, LINE = %d, BuildStreamRecordHeader() for TagGuid Header failed.\n",__FILE__,__LINE__);
            delete en;
            break;
        }

        //Fill in the Tag Guid in the Data Part of the Stream
        hr = en->BuildStreamRecordData((void*)strTagGuid,TagGuidLen);
        if(hr != S_OK)
        {
            DebugPrintf("\nFILE = %s, LINE = %d, BuildStreamRecordData() for TagGuid Data failed,\n", __FILE__,__LINE__);
            delete en;
            break;
        }
        //Again set the stream state as waiting for Header as the actual Tag Data header should be filled now
        en->SetStreamState(eStreamWaitingForHeader);

        //Build the Header for the actual Tag Data
        hr = en->BuildStreamRecordHeader(TagId, ulTagDataLen);
        if (hr != S_OK)
        {
            DebugPrintf("FILE = %s, LINE = %d, BuildStreamRecordHeader() failed for TagName Header.\n", __FILE__, __LINE__);
            delete en;
            break;
        }

        std::string TagKey = "Tag: ";
        TagKey += TagName;
        if (IS_CRASH_TAG(Request.TagType))
        {
            CrashConsistency::Get().m_tagTelInfo.Add(TagKey, std::string());
        }
        else if (IS_APP_TAG(Request.TagType))
        {
            AppConsistency::Get().m_tagTelInfo.Add(TagKey, std::string());
        }
        
        if(STREAM_REC_TYPE_USERDEFINED_EVENT == TagId)
        {
            DebugPrintf("Generating User Defined Tag: %s \t TagType = 0x%x\n", TagName,TagId);
        }
        else
        {
            DebugPrintf("Generating Tag: %s \t TagType = 0x%x\n",TagName,TagId);
        }

        if (IS_BASELINE_TAG(Request.TagType))
        {
            DebugPrintf("%s %s \t TagType = 0x%x\n", BaselineTagKey.c_str(), TagName, TagId);
        }

        hr = en->BuildStreamRecordData((void *)TagName, ulTagDataLen);
        if (hr != S_OK)
        {
            DebugPrintf("FILE = %s, LINE = %d, BuildStreamRecordData() failed for Tag Name\n", __FILE__, __LINE__);
            delete en;
            break;
        }

        if(bTagLifeTime)
        {
            //Again set the stream state as waiting for Header to fill the header for TagLifeTime
            en->SetStreamState(eStreamWaitingForHeader);

            //Build the Header for TagLifeTime
            hr = en->BuildStreamRecordHeader(STREAM_REC_TYPE_LIFETIME, ulTagLifeTimeDataLen);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, BuildStreamRecordHeader() failed\n", __FILE__,__LINE__);
                delete en;
                break;
            }
            //Build the actual data part of the TagLifeTime record in the Vacp's Tag Stream
            hr = en->BuildStreamRecordData(static_cast<void*>(szTagLifeTime),ulTagLifeTimeDataLen);
            if (hr != S_OK)
            {
                DebugPrintf("\nFILE = %s, LINE = %d, BuildStreamRecord() failed for TagLifeTime Record\n", __FILE__, __LINE__);
                delete en;
                break;
            }
        }
        
        
        Request.TagIdentifier.push_back(STREAM_REC_TYPE_TAGGUID_TAG);
        Request.TagLength.push_back((USHORT)ulTotalStreamLen);
        Request.TagData.push_back(TagData);
        
        delete en;
    } while(false);
    return hr;
}

HRESULT ACSRequestGenerator::BuildRevocationTagIdentifiers(ACSRequest_t &Request, USHORT TagId, const char *TagName)
{
    HRESULT hr = S_OK;
    do
    {
      ULONG TotalTagLen = 0;

            ULONG ulDataLength = (ULONG)strlen(TagName) + 1;

            StreamEngine *en = new StreamEngine(eStreamRoleGenerator);

            hr = en->GetDataBufferAllocationLength(ulDataLength, &TotalTagLen);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
                delete en;
                break;
            }

            char *TagData = new char[TotalTagLen];
            hr = en->RegisterDataSourceBuffer(TagData, TotalTagLen);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
                delete en;
                break;
            }
            hr = en->BuildStreamRecordHeader(TagId, ulDataLength);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, BuildStreamRecordHeader() failed\n", __FILE__, __LINE__);
                delete en;
                break;
            }
            hr = en->BuildStreamRecordData((void *)TagName, ulDataLength);
            if (hr != S_OK)
            {
                DebugPrintf("FILE = %s, LINE = %d, BuildStreamRecordData() failed\n", __FILE__, __LINE__);
                delete en;
                break;
            }
            Request.vRevocTagIdentifiers.push_back(TagId);
            Request.vRevocTagLengths.push_back((USHORT)TotalTagLen);
            Request.vRevocTagsData.push_back(TagData);

            delete en;

    } while(false);

    return hr;
}
HRESULT ACSRequestGenerator::BuildRevocationTag(ACSRequest_t &Request)
{
  HRESULT hr = S_OK;
  ULONG ulDataLength = 0;
  ULONG TotalTagLen = 0;
  ULONG ulPendingDataLength = 0;
  do
  {
      if (Request.TagLength.size() == 0)
      {
        break;
      }
      for (unsigned int tagIndex = 0; tagIndex < Request.vRevocTagLengths.size(); tagIndex++)
      {
        //skip user defined tags
        if (Request.vRevocTagIdentifiers[tagIndex] == STREAM_REC_TYPE_USERDEFINED_EVENT)
        {
          continue;
        }
        ulDataLength += Request.vRevocTagLengths[tagIndex];
      }

      //if no consistency tags
      if (ulDataLength == 0)
      {
        break;
      }
      char *RevocationTagData = new char[ulDataLength];

      char *pBuffer = RevocationTagData;
      ulPendingDataLength = ulDataLength;

      for (unsigned int tagIndex = 0; tagIndex < Request.vRevocTagLengths.size(); tagIndex++)
      {
        //skip user defined tags
        if (Request.vRevocTagIdentifiers[tagIndex] == STREAM_REC_TYPE_USERDEFINED_EVENT)
        {
          continue;
        }
        inm_memcpy_s(pBuffer, ulPendingDataLength, Request.vRevocTagsData[tagIndex], Request.vRevocTagLengths[tagIndex]);
        pBuffer += Request.vRevocTagLengths[tagIndex];
        ulPendingDataLength -= Request.vRevocTagLengths[tagIndex];
      }

      StreamEngine *en = new StreamEngine(eStreamRoleGenerator);

      hr = en->GetDataBufferAllocationLength(ulDataLength, &TotalTagLen);
      if (hr != S_OK)
      {
        DebugPrintf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
        delete RevocationTagData;
        delete en;
        break;
      }

      char *TagData = new char[TotalTagLen];
      hr = en->RegisterDataSourceBuffer(TagData, TotalTagLen);
      if (hr != S_OK)
      {
        DebugPrintf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
        delete	TagData;
        delete RevocationTagData;
        delete en;
        break;
      }
      hr = en->BuildStreamRecordHeader(STREAM_REC_TYPE_REVOCATION_TAG, ulDataLength);
      if (hr != S_OK)
      {
        DebugPrintf("FILE = %s, LINE = %d, BuildStreamRecordHeader() failed\n", __FILE__, __LINE__);
        delete	TagData;
        delete RevocationTagData;
        delete en;
        break;
      }
      XDebugPrintf("Generating \"Revocation\" tag ...\n");

      hr = en->BuildStreamRecordData((void *)RevocationTagData, ulDataLength);
      if (hr != S_OK)
      {
        DebugPrintf("FILE = %s, LINE = %d, BuildStreamRecordData() failed\n", __FILE__, __LINE__);
        delete	TagData;
        delete RevocationTagData;
        delete en;
        break;
      }
      if (hr != S_OK)
      {
        delete	TagData;
        delete RevocationTagData;
        break;
      }
      Request.RevocationTagLength = (USHORT)TotalTagLen;
      Request.RevocationTagDataPtr = (void *)(TagData);

      delete RevocationTagData;
      delete en;

  } while(false);
  return hr;
}

HRESULT ACSRequestGenerator::ValidateCLIRequest(CLIRequest_t &CLIRequest)
{
    HRESULT hr = S_OK;

    // remove duplicate application names
    
    for (unsigned int i = 0; i < CLIRequest.m_applications.size(); i++)
    {
            AppParams_t &App1 = CLIRequest.m_applications[i];
        
            for (unsigned int j = i+1; j < CLIRequest.m_applications.size(); j++)
            {
                    AppParams_t &App2 = CLIRequest.m_applications[j];

                    if (boost::iequals(App1.m_applicationName, App2.m_applicationName)) //duplicate application names
                    {
                        size_t NumComponents1 = App1.m_vComponentNames.size();
                        size_t NumComponents2 = App2.m_vComponentNames.size();

                        if (NumComponents1 == 0 || NumComponents2 == 0)
                        {
                            App1.m_vComponentNames.erase(App1.m_vComponentNames.begin(), App1.m_vComponentNames.end());
                            CLIRequest.m_applications.erase(CLIRequest.m_applications.begin() + j);
                            j--;
                            continue;
                        }

                        for(unsigned int k = 0; k<App2.m_vComponentNames.size(); k++)
                        {
                            bool found = false;
                    
                            for (unsigned int l = 0; l<App1.m_vComponentNames.size(); l++)
                            {
                                if (boost::iequals(App2.m_vComponentNames[k], App1.m_vComponentNames[l]))
                                {
                                    found = true;
                                    break;
                                }
                            }

                            if (found)
                            {
                                App2.m_vComponentNames.erase(App2.m_vComponentNames.begin() + k);
                                --k;
                                continue;
                            }

                            App1.m_vComponentNames.push_back(App2.m_vComponentNames[k]);
                        }

                        App2.m_vComponentNames.erase(App2.m_vComponentNames.begin(), App2.m_vComponentNames.end());
                        CLIRequest.m_applications.erase(CLIRequest.m_applications.begin() + j);
                        --j;
                        continue;
                    }
            }
    }
   return hr;
}


HRESULT ACSRequestGenerator::ValidateVolume(const char *volumeName)
{
   HRESULT hr = S_OK;

   do
   {
       if (volumeName == NULL || IsValidVolume(volumeName) == false)
        {
            hr = E_FAIL;
            break;
        }

   } while(false);

   return hr;
}

HRESULT ACSRequestGenerator::ValidateApplication(const char *appName)
{
   HRESULT hr = S_OK;

   do
   {
       if (appName == NULL)
        {
            hr = E_FAIL;
            break;
        }

   } while(false);

   return hr;
}

HRESULT ACSRequestGenerator::FillSharedDiskTagInfo(std::string& clusterTag, bool isDistributed, TAG_TYPE TagType)
{
    USES_CONVERSION;
    XDebugPrintf("ENTERED: ACSRequestGenerator::FillACSRequest\n");
    HRESULT hr = S_OK;
    using namespace std;
    std::stringstream ssErrMsg;
    FailoverClusterInfo clusInfo;
    do
    {
        if (!clusInfo.IsClusterNode())
        {
            hr = VACP_SHARED_DISK_CURRENT_NODE_EVICTED;
            ssErrMsg << "FillSharedDiskTagInfo: Current Node is evicted from cluster.";
            break;
        }

        SVSTATUS status = clusInfo.CollectFailoverClusterProperties(false);
        if (status != SVS_OK)
        {
            hr = VACP_SHARED_DISK_CLUSTER_INFO_FETCH_FAILED;
            ssErrMsg << "FillSharedDiskTagInfo: Failed to collect cluster properties.";
            break;
        }

        std::string clusterId = clusInfo.GetFailoverClusterProperty(FailoverCluster::FAILOVER_CLUSTER_ID);
        if (clusterId.empty())
        {
            hr = VACP_SHARED_DISK_CLUSTER_INFO_FETCH_FAILED;
            ssErrMsg << "FillSharedDiskTagInfo: cluster Id not present.";
            break;
        }

        VacpConfigPtr pVacpConf = VacpConfig::getInstance();
        if (pVacpConf.get() == NULL)
        {
            hr = VACP_SHARED_DISK_CLUSTER_HOST_MAPPING_FETCH_FAILED;
            ssErrMsg << "FillSharedDiskTagInfo: failed to load the vacp config.";
            break;
        }

        VacpConfigParser parser;
        boost::function<std::map<std::string, std::string> (std::string)> hostIdParseFunc =
            boost::bind(&VacpConfigParser::parseSharedDiskHostIdMappings, parser, _1);
        bool hostIdParsedSuccess = pVacpConf->initCustomParsedConfParams(VACP_CLUSTER_HOSTID_NODE_MAPPING, hostIdParseFunc);

        if (!hostIdParsedSuccess)
        {
            hr = VACP_SHARED_DISK_CLUSTER_HOST_MAPPING_FETCH_FAILED;
            ssErrMsg << "FillSharedDiskTagInfo: failed to parse the host mapping";
            break;
        }
        std::map<std::string, std::string> hostNameIdMap;
        hostIdParsedSuccess = pVacpConf->getCustomPrasedConfParamValues(VACP_CLUSTER_HOSTID_NODE_MAPPING,
            hostNameIdMap);
        if (!hostIdParsedSuccess)
        {
            hr = VACP_SHARED_DISK_CLUSTER_HOST_MAPPING_FETCH_FAILED;
            ssErrMsg << "FillSharedDiskTagInfo: failed to fetch the host mappings.";
            break;
        }

        std::string nodeName;
        std::string hostId;
        std::string clusTag = clusterId;
        bool isHostIdPresentInVacpConf = true;
        bool isNodeUp = false;
        int clusterUpNodesCount = 0;

        std::set<NodeEntity> clusNodes = clusInfo.GetClusterNodeSet();
        std::set<NodeEntity>::iterator clusNodesItr = clusNodes.begin();
        for (; clusNodesItr != clusNodes.end(); clusNodesItr++)
        {
            isNodeUp = (*clusNodesItr).nodeState == FailoverCluster::ClusterNodeUp;
            nodeName = (*clusNodesItr).nodeName;

            if (!isNodeUp)
            {
                DebugPrintf("FILE = %s, LINE = %d, cluster vm = %s is not up\n", __FILE__, __LINE__, nodeName.c_str());
                continue;
            }

            isHostIdPresentInVacpConf = isHostIdPresentInVacpConf
                && hostNameIdMap.find(nodeName) != hostNameIdMap.end();
            
            if (!isHostIdPresentInVacpConf)
                break;

            hostId = (*hostNameIdMap.find(nodeName)).second;
            
            if (!clusTag.empty())
                clusTag += ",";
            
            clusTag += hostId;
            clusterUpNodesCount += 1;
        }


        if (!isHostIdPresentInVacpConf || hostNameIdMap.size() != clusterUpNodesCount)
        {
            hr = VACP_SHARED_DISK_CLUSTER_UPDATED;
            ssErrMsg << "FillACSRequest: cluster updated, nodes are deleted/replaced.";    
            break;
        }

        clusTag += ";";
        clusterTag = "_clusterinfo:nodes=";
        clusterTag += clusTag;
        DebugPrintf("FILE %s: LINE %d, clustertag=%s\n", __FILE__, __LINE__, clusterTag.c_str());
    } while (false);

    if (hr != S_OK)
    {
        ssErrMsg << "closing vacp.";
        DebugPrintf("INFO: FILE %s: LINE %d, %s\n", __FILE__, __LINE__, ssErrMsg.str().c_str());
        GetVacpLastError(TagType).m_errMsg = ssErrMsg.str();
        GetVacpLastError(TagType).m_errorCode = hr;
        clusInfo.dumpInfo();
        return hr;
    }

    return S_OK;
}

HRESULT ACSRequestGenerator::FillACSRequest(CLIRequest_t &CLIRequest, ACSRequest_t &Request, TAG_TYPE TagType)
{
    USES_CONVERSION;
   XDebugPrintf("ENTERED: ACSRequestGenerator::FillACSRequest\n");
   HRESULT hr = S_OK;
   using namespace std;
   std::stringstream ssErrMsg;

   do
   {
        if (!gStrTagGuid.empty())
        {
            RPC_STATUS rstatus = UuidFromString((RPC_CSTR)gStrTagGuid.c_str(), &CLIRequest.localTagGuid);

            if (rstatus != RPC_S_OK)
            {
                hr = E_FAIL;
                ssErrMsg << "FillACSRequest: Failed to parse tag guid " << gStrTagGuid << " with status 0x" << std::hex << rstatus;
            }
        }
        else
        {
            hr = CoCreateGuid(&(CLIRequest.localTagGuid));
            if (hr != S_OK)
            {
                ssErrMsg << "FillACSRequest: CoCreateGuid failed with status 0x" << std::hex << hr;
            }
        }

        if (hr != S_OK)
        {
            DebugPrintf("FILE %s: LINE %d, %s.\n", __FILE__, __LINE__, ssErrMsg.str().c_str());
            GetVacpLastError(TagType).m_errMsg = ssErrMsg.str();
            GetVacpLastError(TagType).m_errorCode = hr;
            break;
        }
       //Tag Life Time attributes
        Request.bTagLifeTime		= gbTagLifeTime;
        Request.bTagLTForEver		= gbLTForEver;
        Request.ulMins				= gulLTMins;
        Request.ulHours				= gulLTHours;
        Request.ulDays				= gulLTDays;
        Request.ulWeeks				= gulLTWeeks;
        Request.ulMonths			= gulLTMonths;
        Request.ulYears				= gulLTYears;

        Request.bIgnoreNonDataMode = CLIRequest.bIgnoreNonDataMode;//issue tag even if one of the Vol is not in data mode!

        Request.bSkipChkDriverMode = CLIRequest.bSkipChkDriverMode;
        Request.bSkipUnProtected = CLIRequest.bSkipUnProtected;
        
        Request.bWriterInstance = CLIRequest.bWriterInstance;
        Request.bEnumSW = gbEnumSW;//CLIRequest.bEnumSW;

        //File system volumes or application volumes. All files in these volumes would be included for snapshot
        Request.RawVolumeBitMask = 0; 

        Request.bDoNotIssueTag = CLIRequest.bDoNotIssueTag;
        Request.DoNotIncludeWriters = CLIRequest.DoNotIncludeWriters;
        Request.bProvider = CLIRequest.bProvider;
        Request.strProviderID = CLIRequest.strProviderID;
        UUID uuidProvider;
        UUID uuidInMageVssProvider;
        if(UuidFromString((unsigned char*)Request.strProviderID.c_str(),
            (UUID*)&uuidProvider) == RPC_S_OK)
        {
            if(UuidFromString((unsigned char*)gstrInMageVssProviderGuid.c_str(),
                               (UUID*)&uuidInMageVssProvider) == RPC_S_OK)
            {
                if(!IsEqualGUID(uuidProvider,uuidInMageVssProvider))
                {
                    gbUseInMageVssProvider = false;
                }
                /*else
                {
                    gbUseInMageVssProvider = true;
                }*/
            }
        }      
        Request.strHydrationInfo = CLIRequest.strHydrationInfo;
        Request.bSyncTag = CLIRequest.bSyncTag;
        Request.bTagTimeout = CLIRequest.bTagTimeout;
        Request.dwTagTimeout = CLIRequest.dwTagTimeout;
        Request.bVerify = CLIRequest.bVerify;
        gbIssueTag = !(CLIRequest.bDoNotIssueTag);
        if(gbVerify)
        {
            gbIssueTag = false;
        }
        Request.bDelete = CLIRequest.bDelete;
        Request.bPersist = CLIRequest.bPersist;
        Request.TagType = TagType;

        //Distributed Vacp
        Request.bDistributed = CLIRequest.bDistributed;
        Request.bMasterNode = CLIRequest.bMasterNode;
        if (CLIRequest.bSharedDiskClusterContext)
        {
            hr = FillSharedDiskTagInfo(Request.m_clusterTag, CLIRequest.bDistributed, TagType);
            if (hr != S_OK)
                return hr;
        }
        Request.bCoordNodes = CLIRequest.bCoordNodes;
        Request.strMasterHostName = CLIRequest.strMasterHostName;
        Request.vCoordinatorNodes.insert(Request.vCoordinatorNodes.end(),
                                         CLIRequest.vCoordinatorNodes.begin(),
                                         CLIRequest.vCoordinatorNodes.end());
        Request.usDistributedPort = CLIRequest.usDistributedPort;

        Request.vBookmarkEvents.insert(Request.vBookmarkEvents.begin(),
            CLIRequest.BookmarkEvents.begin(),
            CLIRequest.BookmarkEvents.end());

        Request.vDistributedBookmarkEvents.insert(Request.vDistributedBookmarkEvents.begin(),
            CLIRequest.vDistributedBookmarkEvents.begin(),
            CLIRequest.vDistributedBookmarkEvents.end());

        Request.distributedTagGuid = CLIRequest.distributedTagGuid;
        Request.distributedTagUniqueId = CLIRequest.distributedTagUniqueId;

        Request.localTagGuid = CLIRequest.localTagGuid;

       //snapshot volumes bitmask. This is a UNION of RawVolumeBitMask and VolumeBitMask of volumes affected 
       // by invidual application components which are being selected for snapshot
        Request.volumeBitMask = 0; 
        bool validateMountPoint = false;

        if (IS_BASELINE_TAG(TagType))
        {
            Request.vDisks = CLIRequest.Volumes;
            auto iter = Request.vDisks.begin();
            for (/*empty*/; iter != Request.vDisks.end(); iter++)
            {
                std::wstring strDiskGuidVal = A2W(iter->c_str());
                Request.vDiskGuids.insert(strDiskGuidVal);
                Request.m_diskGuidMap.insert(std::make_pair(strDiskGuidVal, *iter));
            }

            return hr;
        }
        else if (IS_APP_TAG(TagType) || IS_CRASH_TAG(TagType))
        {
            Request.vDisks = CLIRequest.vDisks;
            Request.vDiskGuids = CLIRequest.vDiskGuids;
            Request.m_diskGuidMap = CLIRequest.m_diskGuidMap;
        }
        else
        {
            assert("Unknown TagType detected.");
        }

        auto volIter = CLIRequest.Volumes.begin();
        for (/*empty*/; volIter != CLIRequest.Volumes.end(); volIter++)
        {
            DWORD dwDriveLetterIndex = 0;
            DWORD dwDriveMask = 0;

            const char *volName = volIter->c_str();

            if (0 == stricmp(volName, "all"))
                continue;

            if (volName[0] == '\\')
            {
                VOLMOUNTPOINT volMP;
                volMP.strVolumeMountPoint = volName;
                volMP.strVolumeName = volName;
                Request.volMountPoint_info.dwCountVolMountPoint++;
                Request.volMountPoint_info.vVolumeMountPoints.push_back(volMP);
                dwDriveMask = 1 << 30;
                Request.volumeBitMask |= dwDriveMask;
                Request.RawVolumeBitMask |= dwDriveMask;
            }
            else if (validateMountPoint = IsItVolumeMountPoint(volName))
            {
                std::string svolName = volName;
                char szVolMP[MAX_PATH];
                GetVolumeRootPathEx(svolName.c_str(), szVolMP, ARRAYSIZE(szVolMP), validateMountPoint);
                VOLMOUNTPOINT volMP;

                volMP.strVolumeMountPoint = szVolMP;
                volMP.strVolumeName = svolName;
                if (!IsVolMountAlreadyExist(Request.volMountPoint_info, volMP))
                {
                    Request.volMountPoint_info.dwCountVolMountPoint++;
                    Request.volMountPoint_info.vVolumeMountPoints.push_back(volMP);
                }
                else
                {
                    inm_printf("\nINFO: Duplicate Volume mount points %s found , Skipping it.\n", volMP.strVolumeName.c_str());
                }
                //set bitmask for Mountpoints
                dwDriveMask = 1 << 30;
                Request.volumeBitMask |= dwDriveMask;
                Request.RawVolumeBitMask |= dwDriveMask;
            }
            else
            {
                hr = GetVolumeIndexFromVolumeName(volName, &dwDriveLetterIndex, validateMountPoint);
                if (hr != S_OK)
                {
                    DebugPrintf("\nERROR: volume \"%s\" is NOT valid driver letter index.\n", volName);
                    break;
                }
                if (!IsVolumeAlreadyExists(Request.vVolumes, volName))
                {
                    Request.vVolumes.push_back(*volIter);
                }
                else
                {
                    inm_printf("\nINFO: Duplicate Volume %s found , Skipping it.\n", volName);
                }
                //Get DriveMask from DriveLetterIndex
                dwDriveMask = (1 << dwDriveLetterIndex);
                Request.volumeBitMask |= dwDriveMask;
                Request.RawVolumeBitMask |= dwDriveMask;
            }
        }

        if(gbRemoteSend)
        {
            Request.strServerIP = CLIRequest.strServerIP;
            for(DWORD dwNumerofServerDeviceIDs = 0;dwNumerofServerDeviceIDs  < CLIRequest.vServerDeviceID.size(); dwNumerofServerDeviceIDs++)
            {
                Request.vServerDeviceID.push_back(CLIRequest.vServerDeviceID[dwNumerofServerDeviceIDs]);
            }
            Request.usServerPort = CLIRequest.usServerPort;
        }

       if (!IS_CRASH_TAG(TagType) && !IS_BASELINE_TAG(TagType))
       {
           try {
               vrPtr.reset(new InMageVssRequestor(Request, true));
           }
           catch (const std::exception &e)
           {
               hr = VACP_MEMORY_ALLOC_FAILURE;
               std::stringstream ss;
               ss << "Failed to allocate InMageVssRequestor.";
               DebugPrintf("\nERROR: %s\n", ss.str().c_str());
               GetVacpLastError(TagType).m_errMsg = ss.str();
               GetVacpLastError(TagType).m_errorCode = hr;
               return hr;
           }

           /*vr = new (nothrow) InMageVssRequestor();

           if (vr == NULL)
           {
               hr = VAP_MEMORY_ALLOC_FAILURE;
               std::stringstream ss;
               ss << "Failed to allocate InMageVssRequestor.";
               DebugPrintf("\nERROR: %s\n", ss.str().c_str());
               GetVacpLastError(TagType).m_errMsg = ss.str();
               GetVacpLastError(TagType).m_errorCode = hr;
               return hr;
           }
           std::vector<VssAppInfo_t> &AppInfo = vr->GetVssAppInfo();*/

           std::vector<VssAppInfo_t> &AppInfo = vrPtr->GetVssAppInfo(); 

           //Do not gather VSS app info if -x option is specified. as -x and -a are mutually exclusive
           if (CLIRequest.DoNotIncludeWriters == false)
           {
               if (gbIncludeAllApplications)
               {
                   vrPtr->GatherVssAppsInfo(AppInfo, CLIRequest, true);
               }
               else if (CLIRequest.m_applications.size() > 0)
               {
                   vrPtr->GatherVssAppsInfo(AppInfo, CLIRequest, false);
               }

               vDynamicVssWriter2Apps.clear();

               //Fill the dynamic VSSWriters2Apps list with the gathered VssAppsInfo
               std::vector<VssAppInfo_t>::iterator iterVssAppInfo = AppInfo.begin();
               while (iterVssAppInfo != AppInfo.end())
               {
                   Writer2Application_t *pWriter2App = new (nothrow) Writer2Application_t();
                   if (pWriter2App)
                   {
                       pWriter2App->VssWriterName = (*iterVssAppInfo).AppName;
                       pWriter2App->ApplicationName = (*iterVssAppInfo).AppName;
                       pWriter2App->usStreamRecType = /*STREAM_REC_TYPE_SYSTEMLEVEL*/STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_NEW_VSSWRITER;
                       pWriter2App->VssWriterInstanceName = (*iterVssAppInfo).szWriterInstanceName;
                       pWriter2App->VssWriterInstanceId = (*iterVssAppInfo).idInstance;
                       pWriter2App->VssWriterId = (*iterVssAppInfo).idWriter;
                       vDynamicVssWriter2Apps.push_back(*pWriter2App);
                   }
                   iterVssAppInfo++;
               }

               std::vector<std::string> vInvalidAppNames;
               for (unsigned int iApp = 0; iApp < CLIRequest.m_applications.size(); iApp++)
               {
                   AppParams_t &Aparam = CLIRequest.m_applications[iApp];

                   DWORD dwDriveMask = MapApplication2VolumeBitMaskEx(Aparam.m_applicationName.c_str(), Aparam.m_writerInstanceName.c_str(), AppInfo, CLIRequest.bWriterInstance);
                   MapApplication2VolumeListEx(Aparam.m_applicationName.c_str(), Aparam.m_writerInstanceName.c_str(), AppInfo, Request.vVolumes, CLIRequest.bWriterInstance);

                   AppSummary_t ASum;

                   ASum.m_appName = Aparam.m_applicationName.c_str();

                   if (Aparam.m_vComponentNames.size() == 0) //all components
                   {
                       FillAppSummaryAllComponentsInfo(ASum, AppInfo);
                       ASum.volumeBitMask = dwDriveMask;
                       ASum.includeAllComponents = true;
                       Request.volumeBitMask |= dwDriveMask;
                       Request.RawVolumeBitMask |= dwDriveMask;

                       //add volume mount points Request
                       GetVolumeMountPointInfoForAppEx(Aparam.m_applicationName.c_str(), Aparam.m_writerInstanceName.c_str(), AppInfo, Request.volMountPoint_info, CLIRequest.bWriterInstance);
                       //Add volume mount points to ASum
                       GetVolumeMountPointInfoForAppEx(Aparam.m_applicationName.c_str(), Aparam.m_writerInstanceName.c_str(), AppInfo, ASum.volMountPoint_info, CLIRequest.bWriterInstance);
                   }
                   for (unsigned int iComp = 0; iComp < Aparam.m_vComponentNames.size(); iComp++)
                   {
                       ComponentInfo_t AppComponent;
                       if (GetApplicationComponentInfo(Aparam.m_applicationName.c_str(), Aparam.m_vComponentNames[iComp].c_str(), AppInfo, AppComponent) == false)
                       {
                           hr = VACP_E_INVALID_COMPONENT;
                           std::stringstream ss;
                           ss << "Component \"" << Aparam.m_vComponentNames[iComp] << "\" of Application \"" << Aparam.m_applicationName << "\" is NOT a valid component";
                           DebugPrintf("\nERROR: %s\n", ss.str().c_str());
                           GetVacpLastError(TagType).m_errMsg = ss.str();
                           GetVacpLastError(TagType).m_errorCode = hr;
                           vrPtr.reset();
                           return hr;
                       }

                       XDebugPrintf("Identified Component %s (volBitMask 0x%x) of Application %s \n", Aparam.m_vComponentNames[iComp], AppComponent.dwDriveMask, Aparam.m_applicationName);
                       if (AppComponent.isSelectable == false && AppComponent.isTopLevel == false)
                       {
                           hr = VACP_E_INVALID_COMPONENT;
                           std::stringstream ss;
                           ss << "Component \"" << Aparam.m_vComponentNames[iComp] << "\" of Application \"" << Aparam.m_applicationName << "\" is NOT selectable for snapshot";
                           DebugPrintf("\nERROR: %s\n", ss.str().c_str());
                           GetVacpLastError(TagType).m_errMsg = ss.str();
                           GetVacpLastError(TagType).m_errorCode = hr;
                           vrPtr.reset();
                           return hr;
                       }


                       ComponentSummary_t CSum;

                       CSum.ComponentName = AppComponent.componentName;
                       CSum.ComponentCaption = AppComponent.captionName;
                       CSum.ComponentLogicalPath = AppComponent.logicalPath;

                       for (auto it = AppComponent.FileGroupFiles.begin();
                           it != AppComponent.FileGroupFiles.end();
                           it++)
                            CSum.FileGroupFiles.push_back(*it);

                       for (auto it = AppComponent.DataBaseFiles.begin();
                           it != AppComponent.DataBaseFiles.end();
                           it++)
                           CSum.DatabaseFiles.push_back(*it);

                       for (auto it = AppComponent.DataBaseLogFiles.begin();
                           it != AppComponent.DataBaseLogFiles.end();
                           it++)
                           CSum.DataBaseLogFiles.push_back(*it);

                       CSum.volumeBitMask = AppComponent.dwDriveMask;
                       CSum.IncludedForSnapshot = true;
                       CSum.UserPreferredName = Aparam.m_vComponentNames[iComp].c_str(); // User specified value in the CLIRequest
                       CSum.ComponentType = AppComponent.componentType;
                       CSum.bIsSelectable = AppComponent.isSelectable;//this info is needed to add the components who does not have any affected volumes like Sharepoint

                       CSum.volMountPoint_info.dwCountVolMountPoint = AppComponent.volMountPoint_info.dwCountVolMountPoint;
                       for (unsigned int i = 0; i < AppComponent.volMountPoint_info.dwCountVolMountPoint; i++)
                       {
                           VOLMOUNTPOINT volMP = AppComponent.volMountPoint_info.vVolumeMountPoints[i];
                           CSum.volMountPoint_info.vVolumeMountPoints.push_back(volMP);
                       }
                       DWORD dwMask = 1 << 30;
                       //copy component mountpoints to Request.volMountpoint_info
                       if (AppComponent.dwDriveMask & dwMask)
                       {
                           for (unsigned int i = 0; i < AppComponent.volMountPoint_info.dwCountVolMountPoint; i++)
                           {
                               VOLMOUNTPOINT volMP = AppComponent.volMountPoint_info.vVolumeMountPoints[i];
                               if (!IsVolMountPointAlreadyExist(Request.volMountPoint_info, volMP.strVolumeName.c_str()))
                               {
                                   Request.volMountPoint_info.dwCountVolMountPoint++;
                                   Request.volMountPoint_info.vVolumeMountPoints.push_back(volMP);
                               }
                           }
                       }

                       for (unsigned int i = 0; i < CSum.volMountPoint_info.dwCountVolMountPoint; i++)
                       {
                           VOLMOUNTPOINT volMP = CSum.volMountPoint_info.vVolumeMountPoints[i];
                           if (!IsVolMountAlreadyExist(ASum.volMountPoint_info, volMP))
                           {
                               ASum.volMountPoint_info.dwCountVolMountPoint++;
                               ASum.volMountPoint_info.vVolumeMountPoints.push_back(volMP);
                           }
                       }

                       Request.volumeBitMask |= CSum.volumeBitMask;
                       ASum.volumeBitMask |= CSum.volumeBitMask;
                       ASum.includeAllComponents = false;
                       ASum.m_vComponents.push_back(CSum);
                   }

                   Request.Applications.push_back(ASum);
               }
               int nCounter = 0;
               if (vInvalidAppNames.size() > 0)
               {
                   std::vector<std::string>::iterator iterInvalidApp = vInvalidAppNames.begin();
                   inm_printf("\nError: The following applications are not part of the consistency...\n");
                   for (; iterInvalidApp != vInvalidAppNames.end(); iterInvalidApp++)
                   {
                       ++nCounter;
                       if (nCounter == vInvalidAppNames.size())
                       {
                           inm_printf("%s", (*iterInvalidApp).c_str());
                       }
                       else
                       {
                           inm_printf("%s, ", (*iterInvalidApp).c_str());
                       }
                   }
                   inm_printf("\n\nfor the following possible reasons....\n");
                   inm_printf("\n 1. Either the Applications are not installed		OR");
                   inm_printf("\n 2. The services of the Applications are not running	OR");
                   inm_printf("\n 3. The VSS Writer Service associated with the Applications are not running	OR");
                   inm_printf("\n 4. The InVolFlt Driver is not installed in this system	OR");
                   inm_printf("\n 5. There are zero affected volumes for each of the above Applications\n");
               }
           }
       }

   } while(false);

   XDebugPrintf("EXITED: ACSRequestGenerator::FillACSRequest\n");
   return hr;
}


bool ACSRequestGenerator::IsVolMountAlreadyExist(VOLMOUNTPOINT_INFO& targetVolMP_info,VOLMOUNTPOINT& addVolMP)
{
    bool ret = false;
    for(DWORD  i = 0; i <targetVolMP_info.dwCountVolMountPoint; i++)
    {
        if(!strnicmp(targetVolMP_info.vVolumeMountPoints[i].strVolumeMountPoint.c_str(), addVolMP.strVolumeMountPoint.c_str(),addVolMP.strVolumeMountPoint.size()))
        {
            ret = true;
        }
    }
    return ret;
}
