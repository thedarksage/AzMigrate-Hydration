#include <string>
#include <sstream>
#include <cstring>
#include "scsi_idinformer.h"
#include "inmsafecapis.h"

#define NELEMS(ARR) ((sizeof (ARR)) / (sizeof (ARR[0])))

ScsiIDInformer::ScsiIDInformer()
: m_IsScsiCommandIssuerExternal(false),
  m_pScsiCommandIssuer(0)
{
}


ScsiIDInformer::ScsiIDInformer(ScsiCommandIssuer *pscsicommandissuer)
: m_pScsiCommandIssuer(pscsicommandissuer),
  m_IsScsiCommandIssuerExternal(true)
{
}


bool ScsiIDInformer::Init(void)
{
    bool initialized = true;
   
    if (!m_IsScsiCommandIssuerExternal)
    {
        m_pScsiCommandIssuer = new (std::nothrow) ScsiCommandIssuer();
        initialized = m_pScsiCommandIssuer ? true : false;
    }

    return initialized;
}


ScsiIDInformer::~ScsiIDInformer()
{
    if (!m_IsScsiCommandIssuerExternal)
    {
        if (m_pScsiCommandIssuer)
        {
            delete m_pScsiCommandIssuer;
        }
    }
}


std::string ScsiIDInformer::GetID(const std::string &dev)
{
    std::string scsi_id;

    m_ErrorMessage.clear();
    if (!m_pScsiCommandIssuer->StartSession(dev))
    {
        m_ErrorMessage = std::string("scsi command session start for ") + dev + " failed with error " + m_pScsiCommandIssuer->GetErrorMessage();
        return scsi_id;
    }

    INQUIRY_DETAILS InqDetails;
    std::stringstream msg;

    msg << "device " << dev << ", ";
    InqDetails.device = dev;
    bool bgotstdinq = m_pScsiCommandIssuer->GetStdInqValues(&InqDetails);
    if (bgotstdinq)
    {
        unsigned char page0data[INQLEN] = "";
        bool bgotp0 = GetPage0Inq(&InqDetails, page0data, sizeof page0data);
        bool bgotidfromp83 = false;
        if (bgotp0)
        {
            for (int i = PAGE0_PAGESOFFSET; i <= (page0data[PAGE0_LEN_BYTENUMBER] + PAGE0_LEN_BYTENUMBER); i++)
            {
                if (page0data[i] == INQPAGE83)
                {
                    bgotidfromp83 = GetIDFromPage83(&InqDetails);
                    if (bgotidfromp83)
                    {
                        MakeIDFormatted(InqDetails.m_Id);
                        scsi_id = InqDetails.m_Id;
                    }
                    else
                    {
                        msg << "did not give page 83 ID with error " << m_ErrorMessage;
                    }
                    break;
                }
            }

            if (!bgotidfromp83)
            {
                msg << "does not support page 83.";
            }
        }
        else
        {
            msg << "did not give page 0 data with error " << m_ErrorMessage;
        }
    }
    else
    {
        msg << "did not give standard inquiry data with error: " << m_pScsiCommandIssuer->GetErrorMessage();
    }

    if (scsi_id.empty())
    {
        m_ErrorMessage = std::string("could not find scsi id with error ") + msg.str();
    }

    m_pScsiCommandIssuer->EndSession();

    return scsi_id;
}


bool ScsiIDInformer::GetPage0Inq(INQUIRY_DETAILS *pInqDetails, unsigned char *pp0data, const unsigned int &p0datalen)
{
    unsigned char page = 0x0;
    bool bgot = m_pScsiCommandIssuer->DoInquiryCmd(pInqDetails, 1, page, pp0data, p0datalen);

    if (bgot)
    {
        if (pp0data[PAGEDATA_BYTENUMBER] == page)
        {
            if (pp0data[PAGE0_LEN_BYTENUMBER] <= p0datalen)
            {
                if (pp0data[PAGE0_LEN_BYTENUMBER] > PRODUCTLEN)
                {
                    if (0 == strncmp((char *)(pp0data + VENDOROFFSET), pInqDetails->m_Vendor, VENDORLEN))
                    {
                        /* DebugPrintf(SV_LOG_WARNING, "device %s returned standard inquiry when asked for page 0\n", pInqDetails->device.c_str()); */
                        bgot = false;
                    }
                }
            }
            else
            {
                /* DebugPrintf(SV_LOG_WARNING, "For device %s, got page 0 data length %d exceeds input length\n", 
                                            pInqDetails->device.c_str(),  pp0data[PAGE0_LEN_BYTENUMBER]); */
                bgot = false;
            }
        }
        else
        {
            /* DebugPrintf(SV_LOG_WARNING, "For device %s, the page 0 buffer got is illegal\n", pInqDetails->device.c_str()); */
            bgot = false;
        }
    }
    else
    {
        m_ErrorMessage = m_pScsiCommandIssuer->GetErrorMessage();
    }

    return bgot;
}


bool ScsiIDInformer::GetIDFromPage83(INQUIRY_DETAILS *pInqDetails)
{
    bool bgotid = false;
    unsigned char page83data[INQLEN] = "";
    bool bgotp83 = m_pScsiCommandIssuer->DoInquiryCmd(pInqDetails, 1, INQPAGE83, page83data, sizeof page83data);
     
    if (bgotp83)
    {
        bgotid = GetIDFromPage83Data(pInqDetails, page83data);
    }
    else
    {
        m_ErrorMessage = m_pScsiCommandIssuer->GetErrorMessage();
    }

    return bgotid;
}


bool ScsiIDInformer::GetIDFromPage83Data(INQUIRY_DETAILS *pInqDetails, unsigned char *ppage83data)
{
    bool bgotid = false;

    if (ppage83data[PAGEDATA_BYTENUMBER] == INQPAGE83) 
    {
        if (ppage83data[PAGE83_PRESPC3_BYTE])
        {
            bgotid = GetIDFromPage83PreSpc3(pInqDetails, ppage83data); 
        }
        else
        {
            bgotid = GetIDFromCurrentPage83(pInqDetails, ppage83data);
        }
    }
    else
    {
        m_ErrorMessage = "the page 83 buffer got is illegal";
    }
 
    return bgotid;
}


bool ScsiIDInformer::GetIDFromCurrentPage83(INQUIRY_DETAILS *pInqDetails, unsigned char *ppage83data)
{
    bool bgotid = false;

    for (unsigned int i = 0; (i < NELEMS(idTypeAndCodeSetList)) && (false == bgotid); i++)
    {
        unsigned int j = PAGE83_IDOFFSET;
        while (j <= (unsigned int)(ppage83data[PAGE83_LENBYTE] + PAGE83_LENBYTE))
        {
            bgotid = GetIDFromIdTypeAndCodeSet(pInqDetails, ppage83data + j,
                                               idTypeAndCodeSetList + i, IDLENMAX);
            if (bgotid)
            {
                break;
            }
            j += (ppage83data[j +  PAGE83_DESCIDLEN_BYTE] + PAGE83_IDOFFSET_INDESC);
        }
    }
 
    return bgotid;
}


bool ScsiIDInformer::GetIDFromIdTypeAndCodeSet(INQUIRY_DETAILS *pInqDetails, unsigned char *ppage83data,
                                               const IDTYPEANDCODESET *pIdAndCodeSet, const int &maxlen)
{
    bool bgotid = false;
    bool bisdataproper = true;

    if ((ppage83data[1] & 0x30) != 0)
    {
        bisdataproper = false;
    }

    if (bisdataproper && ((ppage83data[1] & 0x0f) != pIdAndCodeSet->m_IdType))
    {
        bisdataproper = false;
    }

    if (bisdataproper && 
        (pIdAndCodeSet->m_NaaType != E_NAANOTNEEDED) &&
        (pIdAndCodeSet->m_NaaType != ((ppage83data[4] & 0xf0) >> 4)))
    {
        bisdataproper = false;
    }

    if (bisdataproper && 
        ((ppage83data[0] & 0x0f) != pIdAndCodeSet->m_CodeSet))
    {
        bisdataproper = false;
    }

    if (bisdataproper)
    {
        int length = ppage83data[3]; 
        if ((ppage83data[0] & 0x0f) != E_CODESET_ASCII)
        {
            length *= 2;
        }
        length += 2;
        if (pIdAndCodeSet->m_IdType == E_VENDORSPECIFICID)
        {
            length += (VENDORLEN + PRODUCTLEN);
        }
        if (maxlen < length) 
        {
            /* DebugPrintf(SV_LOG_WARNING, "For device %s, calculated id length %d crosses maxlength %d\n", 
                                        pInqDetails->device.c_str(), length, maxlen); */
            bisdataproper = false;
        }
    }

    if (bisdataproper)
    {
        pInqDetails->m_Id[0] = LowerStrHex[pIdAndCodeSet->m_IdType];
        if (pIdAndCodeSet->m_IdType == E_VENDORSPECIFICID)
        {
            AddVendorProductPrefix(pInqDetails, pInqDetails->m_Id+1, sizeof(pInqDetails->m_Id)-1);
        }
        int i = PAGE83_IDOFFSET_INDESC;
        int j = strlen(pInqDetails->m_Id);

        if ((ppage83data[0] & 0x0f) == E_CODESET_ASCII) 
        {
            while (i < (PAGE83_IDOFFSET_INDESC + ppage83data[PAGE83_DESCIDLEN_BYTE]))
            {
                pInqDetails->m_Id[j++] = ppage83data[i++];
            }
        } 
        else 
        {
            while (i < (PAGE83_IDOFFSET_INDESC + ppage83data[PAGE83_DESCIDLEN_BYTE])) 
            {
                pInqDetails->m_Id[j++] = LowerStrHex[(ppage83data[i] & 0xf0) >> 4];
                pInqDetails->m_Id[j++] = LowerStrHex[ppage83data[i] & 0x0f];
                i++;
            }
        }
        bgotid = bisdataproper;
    }
    else
    {
        m_ErrorMessage = "page 83 data is not proper";
    }

    return bgotid;
}


void ScsiIDInformer::AddVendorProductPrefix(INQUIRY_DETAILS *pInqDetails, char *id, const size_t &idsize)
{
    inm_strncpy_s(id, idsize, pInqDetails->m_Vendor, VENDORLEN);
    inm_strncat_s(id, idsize, pInqDetails->m_Product, PRODUCTLEN);
}


void ScsiIDInformer::MakeIDFormatted(char *id)
{
    char *pid = id;
    char *tp = pid;

    while (*pid != '\0') 
    {
        if (isspace(*pid)) 
        {
            if ((tp > id) && (tp[-1] != '_'))
            {
                *tp = '_';
                tp++;
            }
        } 
        else 
        {
            *tp = *pid;
            tp++;
        }
        pid++;
    }
    *tp = '\0';
}


bool ScsiIDInformer::GetIDFromPage83PreSpc3(INQUIRY_DETAILS *pInqDetails, unsigned char *ppage83data)
{
    bool bgot = true;
   
    pInqDetails->m_Id[0] = LowerStrHex[idTypeAndCodeSetList[0].m_IdType];
    int idx = strlen(pInqDetails->m_Id); 

    for (int p83idx = PAGE83_PRESPC3_IDOFFSET; p83idx < (ppage83data[PAGE83_PRESPC3_LENBYTE] + PAGE83_PRESPC3_IDOFFSET); p83idx++)
    {
        pInqDetails->m_Id[idx++] = LowerStrHex[(ppage83data[p83idx] & 0xf0) >> 4];
        pInqDetails->m_Id[idx++] = LowerStrHex[ ppage83data[p83idx] & 0x0f];
    }

    return bgot;
}


std::string ScsiIDInformer::GetVendorAssignedSerialNumber(const std::string &dev)
{
    std::string serial_no;

    m_ErrorMessage.clear();
    if (!m_pScsiCommandIssuer->StartSession(dev))
    {
        m_ErrorMessage = std::string("scsi command session start for ") + dev + " failed with error " + m_pScsiCommandIssuer->GetErrorMessage();
        return serial_no;
    }

    INQUIRY_DETAILS InqDetails;
    std::stringstream msg;

    msg << "device " << dev << ", ";
    InqDetails.device = dev;
    bool bgotstdinq = m_pScsiCommandIssuer->GetStdInqValues(&InqDetails);
    if (bgotstdinq)
    {
        unsigned char page0data[INQLEN] = "";
        bool bgotp0 = GetPage0Inq(&InqDetails, page0data, sizeof page0data);
        bool bgotserialno = false;
        if (bgotp0)
        {
            for (int i = PAGE0_PAGESOFFSET; i <= (page0data[PAGE0_LEN_BYTENUMBER] + PAGE0_LEN_BYTENUMBER); i++)
            {
                if (page0data[i] == INQPAGE80)
                {
                    bgotserialno = GetVendorAssignedSerialNumber(&InqDetails);
                    if (bgotserialno)
                    {
                        MakeIDFormatted(InqDetails.m_Id);
                        serial_no = InqDetails.m_Id;
                    }
                    else
                    {
                        msg << "did not give page 80 ID with error " << m_ErrorMessage;
                    }
                    break;
                }
            }

            if (!bgotserialno)
            {
                msg << "does not support page 80.";
            }
        }
        else
        {
            msg << "did not give page 0 data with error " << m_ErrorMessage;
        }
    }
    else
    {
        msg << "did not give standard inquiry data with error: " << m_pScsiCommandIssuer->GetErrorMessage();
    }

    if (serial_no.empty())
    {
        m_ErrorMessage = std::string("could not find vendor assigned serial number with error ") + msg.str();
    }

    m_pScsiCommandIssuer->EndSession();

    return serial_no;
}


bool ScsiIDInformer::GetVendorAssignedSerialNumber(INQUIRY_DETAILS *pInqDetails)
{
    bool bgotid = false;
    unsigned char ppage80data[INQLEN] = "";
    bool bgotp80 = m_pScsiCommandIssuer->DoInquiryCmd(pInqDetails, 1, INQPAGE80, ppage80data, sizeof ppage80data);
     
    if (bgotp80) {
        // Page 80 response bytes are shown below:
        //        ----------------------------------------
        // byte 0 | PERIPHERAL QUALIFIER AND DEVICE TYPE |
        //        ----------------------------------------
        // 1      | PAGE CODE (80h)                      |
        //        ----------------------------------------
        // 2      | Reserved                             |
        //        ----------------------------------------
        // 3      | PAGE LENGTH (n)                      |
        //        ----------------------------------------
        // 4 to n | PRODUCT SERIAL NUMBER                |
        //        ----------------------------------------
        const unsigned char SNO_INDEX = 4;
        const unsigned char PAGE_LEN_INDEX = 3;
        if ((ppage80data[PAGE_LEN_INDEX] > SNO_INDEX) && 
            ((ppage80data[PAGE_LEN_INDEX]-SNO_INDEX) < sizeof(pInqDetails->m_Id))) {
            unsigned int i, s;
            for (i = 0, s = SNO_INDEX; s < ppage80data[PAGE_LEN_INDEX] + SNO_INDEX; s++, i++)
                pInqDetails->m_Id[i] = ppage80data[s];
            pInqDetails->m_Id[i] = '\0';
            bgotid = true;
        }
        else {
            std::stringstream sserr;
            sserr << "Page 80 page length value " << ((unsigned)ppage80data[PAGE_LEN_INDEX]) << " is either invalid or more than max expected " << sizeof(pInqDetails->m_Id);
            m_ErrorMessage = sserr.str();
        }
    }
    else
        m_ErrorMessage = m_pScsiCommandIssuer->GetErrorMessage();

    return bgotid;
}


std::string ScsiIDInformer::GetErrorMessage(void)
{
    return m_ErrorMessage;
}
