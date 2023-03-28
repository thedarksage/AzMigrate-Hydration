#ifndef _DEVICE__ID__INFORMER__H_
#define _DEVICE__ID__INFORMER__H_

#include <string>
#include "scsi_idinformer.h"


class DeviceIDInformer
{
public:
    DeviceIDInformer();
    DeviceIDInformer(ScsiCommandIssuer *pscsicommandissuer);
    void DoNotTryScsiID(void);
    std::string GetID(const std::string &device);
    std::string GetErrorMessage(void);
  
    /// \brief Gets vendor assigned serial number
    std::string GetVendorAssignedSerialNumber(const std::string &device); ///< device

private:
    typedef std::string (DeviceIDInformer::*GetIDMethods_t)(const std::string &device);
    std::string GetScsiID(const std::string &device);
    std::string DummyGetID(const std::string &device);
    void InitializeGetIDMethods(void);

    /// \brief Gets vendor assigned scsi serial number
    std::string GetVendorAssignedScsiSerialNumber(const std::string &device); ///< device

    /// \brief returns empty
    std::string DummyGetVendorAssignedSerialNumber(const std::string &device); ///< device

private:
    ScsiIDInformer m_ScsiIDInformer;
    std::string m_ErrorMessage;
    bool m_TryScsiID;
    GetIDMethods_t m_GetIDMethods[2];

    GetIDMethods_t m_GetVendorAssignedSerialNumberMethods[2]; ///< vendor assigned serial no. methods
};

#endif /* _DEVICE__ID__INFORMER__H_ */
