#include <string>
#include "deviceidinformer.h"


DeviceIDInformer::DeviceIDInformer() : 
 m_TryScsiID(true)
{
    /* TODO: check return value ? */
    m_ScsiIDInformer.Init();
    InitializeGetIDMethods();
}


DeviceIDInformer::DeviceIDInformer(ScsiCommandIssuer *pscsicommandissuer)
: m_ScsiIDInformer(pscsicommandissuer),
  m_TryScsiID(true)
{
    /* TODO: check return value ? */
    m_ScsiIDInformer.Init();
    InitializeGetIDMethods();
}


void DeviceIDInformer::DoNotTryScsiID(void)
{
    m_TryScsiID = false;
}


std::string DeviceIDInformer::GetID(const std::string &device)
{
    m_ErrorMessage.clear();
    GetIDMethods_t p = m_GetIDMethods[m_TryScsiID];
    return (this->*p)(device);
}


std::string DeviceIDInformer::GetVendorAssignedSerialNumber(const std::string &device)
{
    m_ErrorMessage.clear();
    GetIDMethods_t p = m_GetVendorAssignedSerialNumberMethods[m_TryScsiID];
    return (this->*p)(device);
}


std::string DeviceIDInformer::GetErrorMessage(void)
{
    return m_ErrorMessage;
}


void DeviceIDInformer::InitializeGetIDMethods(void)
{
    m_GetIDMethods[0] = &DeviceIDInformer::DummyGetID;
    m_GetIDMethods[1] = &DeviceIDInformer::GetScsiID;

    m_GetVendorAssignedSerialNumberMethods[0] = &DeviceIDInformer::DummyGetVendorAssignedSerialNumber;
    m_GetVendorAssignedSerialNumberMethods[1] = &DeviceIDInformer::GetVendorAssignedScsiSerialNumber;
}


std::string DeviceIDInformer::GetScsiID(const std::string &device)
{
    std::string id = m_ScsiIDInformer.GetID(device);
    if (id.empty())
    {
        m_ErrorMessage = m_ScsiIDInformer.GetErrorMessage();
    }

    return id;
}


std::string DeviceIDInformer::DummyGetID(const std::string &device)
{
    return std::string();
}


std::string DeviceIDInformer::GetVendorAssignedScsiSerialNumber(const std::string &device)
{
    std::string sno = m_ScsiIDInformer.GetVendorAssignedSerialNumber(device);
    if (sno.empty())
        m_ErrorMessage = m_ScsiIDInformer.GetErrorMessage();

    return sno;
}


std::string DeviceIDInformer::DummyGetVendorAssignedSerialNumber(const std::string &device)
{
    return std::string();
}
