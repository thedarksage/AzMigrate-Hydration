#pragma once

#include <string>
#include <list>

#define _WIN32_DCOM

#include <iostream>
#include <atlbase.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <set>
#include <algorithm>
#include <vector>

#define HYPERVMANUFACTURER      "Microsoft Corporation"
#define HYPERVMODEL             "Virtual Machine"

#define VMWAREPAT "VMware"
#define XENPAT    "Xen"
#define MICROSOFTPAT "Microsoft"

#define VMWAREMFPAT "Manufacturer: VMware"
#define MICROSOFTCORPMFPAT "Manufacturer: Microsoft Corporation"
#define MICROSOFTMFPAT "Manufacturer: Microsoft"
#define VIRTUALBOXMFPAT "Manufacturer: innotek GmbH"
#define XENMFPAT "Manufacturer: Xen"

/* hypervisor names */
#define XENNAME                 "xen"
#define HYPERVNAME              "Microsoft HyperV"
#define VMWARENAME              "VMware"
#define VIRTUALPCNAME           "virtualpc"
#define VIRTUALBOXNAME          "virtualbox"
#define MICROSOFTNAME           "Microsoft"
#define PHYSICALMACHINE         "Physical Machine"

typedef enum _CANNOT_POOL_REASON {
    CannotPoolReasonUnknown = 0,
    CannotPoolReasonNone,
    CannotPoolReasonAlreadyPooled,
    CannotPoolReasonNotHealthy,
    CannotPoolReasonRemovableMedia,
    CannotPoolReasonClustered,
    CannotPoolReasonOffline,
    CannotPoolReasonInsufficientCapacity,
    CannotPoolReasonMax
} CANNOT_POOL_REASON;

class WmiRecordProcessor
{
protected:
    std::string                         m_errorMessage;

public:
    virtual HRESULT Process(IWbemClassObject *precordobj) = 0;
    virtual std::string GetErrorMessage(void);

    virtual ~WmiRecordProcessor() { }
};

class WmiHelper
{
public:
    WmiHelper() : m_initialized(FALSE)
    {
    }

    ~WmiHelper();

    HRESULT     Init();
    HRESULT     GetData(const std::wstring wmiProvider, std::wstring className, WmiRecordProcessor& processor);

    std::string GetErrorMessage();

private:
    BOOL                m_initialized;
    std::string         m_errorMessage;
    HRESULT             ComInitialize();
};

class WmiDiskProcessor : public WmiRecordProcessor
{
protected:
    std::set<std::string>              m_diskNameSet;

public:
    virtual HRESULT                 Process(IWbemClassObject *precordobj) = 0;
    virtual std::set<std::string>   GetDiskNames();
};

class MsftPhysicalDiskRecordProcessor : public WmiDiskProcessor
{
public:
    HRESULT                 Process(IWbemClassObject *precordobj);
    std::set<std::string>   GetStoragePoolDisks();

private:
    std::set<std::string>             m_storagePoolDiskNames;
};


class Win32DiskDriveRecordProcessor : public WmiDiskProcessor
{
public:
    HRESULT                             Process(IWbemClassObject *precordobj);
    std::set<std::string>               GetStorageSpaceDisks();

private:
    std::set<std::string>              m_storageSpaceDiskNames;
};



class Win32NetworkAdapterConfigProcessor : public WmiRecordProcessor
{
protected:
    std::vector<std::string>              m_ipAddresses;

public:
    HRESULT                 Process(IWbemClassObject *precordobj);
    std::vector<std::string>   GetIPAddresses();
};

class Win32ComputerSystem : public WmiRecordProcessor
{
protected:
    bool            m_isVirtual;
    std::string     m_hypervisorName;

public:
    HRESULT                 Process(IWbemClassObject *precordobj);
    bool    IsVirtual(std::string& hypervisor);
};

bool IsVirutalMachine(bool& isVirtual, std::string& hypervisorName);