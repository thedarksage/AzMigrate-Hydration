#ifndef _SCSI__ID__INFORMER__H_
#define _SCSI__ID__INFORMER__H_

#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "scsicommandissuer.h"


#define INQPAGE83 0x83
#define INQPAGE80 0x80
const char LowerStrHex[]="0123456789abcdef";

#define PAGEDATA_BYTENUMBER 1

#define PAGE0_LEN_BYTENUMBER 3
#define PAGE0_PAGESOFFSET 4

#define PAGE83_PRESPC3_BYTE 6
#define PAGE83_PRESPC3_IDOFFSET 4
#define PAGE83_PRESPC3_LENBYTE 3

#define PAGE83_LENBYTE 3
#define PAGE83_IDOFFSET 4

#define PAGE83_DESCIDLEN_BYTE 3
#define PAGE83_IDOFFSET_INDESC 4

typedef enum eIdType
{
    E_VENDORSPECIFICID = 0,
    E_T10VENDORID = 1,
    E_EUI64BASEDID = 2,
    E_NAAID = 3

} E_IDTYPE;

typedef enum eNaaType
{
    E_NAAIEEEREG = 5,
    E_NAAIEEEREGEXT = 6,
    E_NAANOTNEEDED = 0xff

} E_NAATYPE;

typedef enum eCodeSet
{
    E_CODESET_BINARY = 1,
    E_CODESET_ASCII = 2
    
} E_CODESET;

typedef struct idTypeAndCodeSet
{
    unsigned char m_IdType;
    unsigned char m_NaaType;
    unsigned char m_CodeSet;

} IDTYPEANDCODESET;


const IDTYPEANDCODESET idTypeAndCodeSetList[] = {
                                                 {E_NAAID, E_NAAIEEEREGEXT, E_CODESET_BINARY },
                                                 {E_NAAID, E_NAAIEEEREGEXT, E_CODESET_ASCII },
                                                 {E_NAAID, E_NAAIEEEREG, E_CODESET_BINARY },
                                                 {E_NAAID, E_NAAIEEEREG, E_CODESET_ASCII },
                                                 {E_NAAID, E_NAANOTNEEDED, E_CODESET_BINARY },
                                                 {E_NAAID, E_NAANOTNEEDED, E_CODESET_ASCII },
                                                 {E_EUI64BASEDID, E_NAANOTNEEDED, E_CODESET_BINARY },
                                                 {E_EUI64BASEDID, E_NAANOTNEEDED, E_CODESET_ASCII },
                                                 {E_T10VENDORID, E_NAANOTNEEDED, E_CODESET_BINARY },
                                                 {E_T10VENDORID, E_NAANOTNEEDED, E_CODESET_ASCII },
                                                 {E_VENDORSPECIFICID, E_NAANOTNEEDED, E_CODESET_BINARY },
                                                 {E_VENDORSPECIFICID, E_NAANOTNEEDED, E_CODESET_ASCII },
                                                };

class ScsiIDInformer
{
public:
    ScsiIDInformer();
    ScsiIDInformer(ScsiCommandIssuer *pscsicommandissuer);
    bool Init(void);
    ~ScsiIDInformer();

    std::string GetID(const std::string &dev);
    std::string GetErrorMessage(void);

    /// \brief returns serial number of device from page 80
    std::string GetVendorAssignedSerialNumber(const std::string &dev); ///< device

private:
    bool GetPage0Inq(INQUIRY_DETAILS *pInqDetails, unsigned char *pp0data, const unsigned int &p0datalen);
    bool GetIDFromPage83(INQUIRY_DETAILS *pInqDetails);
    bool GetIDFromPage83Data(INQUIRY_DETAILS *pInqDetails, unsigned char *ppage83data);
    bool GetIDFromPage83PreSpc3(INQUIRY_DETAILS *pInqDetails, unsigned char *ppage83data);
    bool GetIDFromCurrentPage83(INQUIRY_DETAILS *pInqDetails, unsigned char *ppage83data);
    bool GetIDFromIdTypeAndCodeSet(INQUIRY_DETAILS *pInqDetails, unsigned char *ppage83data,
                                   const IDTYPEANDCODESET *pIdAndCodeSet, const int &maxlen);
    void AddVendorProductPrefix(INQUIRY_DETAILS *pInqDetails, char *id, const size_t &idsize);
    void MakeIDFormatted(char *id);

    /// \brief fills serial number in inquiry details
    bool GetVendorAssignedSerialNumber(INQUIRY_DETAILS *pInqDetails); ///< inquiry details

private:
    ScsiCommandIssuer *m_pScsiCommandIssuer;
    bool m_IsScsiCommandIssuerExternal;
    std::string m_ErrorMessage;
};

#endif /* _SCSI__ID__INFORMER__H_ */
