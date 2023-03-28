#include "stdafx.h"
#include "vacpwmiutils.h"
#include "diskutils.h"

HRESULT WmiHelper::ComInitialize()
{
    HRESULT             hr = S_OK;
    std::stringstream   ssError;
    do
    {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

        /*Already Com is Initialized*/
        if ((hr == S_OK) || (hr == S_FALSE)) {
            hr = S_OK;
        }
        else {
            ssError << "CoInitializeEx COINIT_MULTITHREADED failed with hr=0x%x" << std::hex << hr;
            m_errorMessage = ssError.str();
            break;
        }

        m_initialized = TRUE;

        hr = CoInitializeSecurity
            (
            NULL,                                //  IN PSECURITY_DESCRIPTOR         pSecDesc,
            -1,                                  //  IN LONG                         cAuthSvc,
            NULL,                                //  IN SOLE_AUTHENTICATION_SERVICE *asAuthSvc,
            NULL,                                //  IN void                        *pReserved1,
            RPC_C_AUTHN_LEVEL_CONNECT,           //  IN DWORD                        dwAuthnLevel,
            RPC_C_IMP_LEVEL_IMPERSONATE,         //  IN DWORD                        dwImpLevel,
            NULL,                                //  IN void                        *pAuthList,
            EOAC_NONE,                           //  IN DWORD                        dwCapabilities,
            NULL                                 //  IN void                        *pReserved3
            );

        // Check if the securiy has already been initialized by someone in the same process.
        if (hr == RPC_E_TOO_LATE)
        {
            hr = S_OK;
        }

        if (hr != S_OK)
        {
            ssError << "CoInitializeSecurity COINIT_MULTITHREADED failed with hr=0x%x" << std::hex << hr;
            m_errorMessage = ssError.str();
            break;
        }
    } while (FALSE);

    return hr;
}

HRESULT   WmiHelper::Init()
{
    HRESULT             hr = S_OK;
    std::stringstream   ssError;

    hr = ComInitialize();

    if (FAILED(hr)) {
        ssError << "Com Initialization failed. Error Code: " << std::hex << hr;
        m_errorMessage = ssError.str();
    }

    return hr;
}

HRESULT WmiHelper::GetData(const std::wstring wmiProvider, std::wstring className, WmiRecordProcessor& processor)
{
    HRESULT                             hr = S_OK;
    CComPtr<IWbemLocator>               pLoc;
    CComPtr<IWbemServices>              pWbemService;
    CComPtr<IEnumWbemClassObject>       pEnum;
    std::wstringstream                  wQueryStream;
    std::stringstream                   ssErrMessage;

    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&pLoc);

    if (FAILED(hr)) {
        ssErrMessage << "Failed to create IWbemLocator object. Error Code: " << std::hex << hr;
        m_errorMessage = ssErrMessage.str();
        goto Cleanup;
    }

    hr = pLoc->ConnectServer(BSTR(wmiProvider.c_str()), NULL, NULL, 0, NULL, 0, 0, &pWbemService);

    if (FAILED(hr))
    {
        ssErrMessage << "Could not connect to WMI service." << CW2A(wmiProvider.c_str());
        m_errorMessage = ssErrMessage.str();

        goto Cleanup;
    }

    wQueryStream << L"Select * FROM " << className.c_str();

    hr = pWbemService->ExecQuery(
        BSTR(L"WQL"),
        BSTR(wQueryStream.str().c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,         // Flags
        0,                              // Context
        &pEnum
        );

    if (FAILED(hr))
    {
        ssErrMessage << "Query " << CW2A(wmiProvider.c_str()) << " failed. Error code: " << std::hex << hr;
        m_errorMessage = ssErrMessage.str();
        DebugPrintf(SV_LOG_ERROR, "%s: error: %s\n", __FUNCTION__, m_errorMessage.c_str());
        goto Cleanup;
    }

    USES_CONVERSION;

    while (pEnum)
    {

        ULONG                           uReturned = 0;
        CComPtr<IWbemClassObject>       pObj;
        DWORD                           dwErr = ERROR_SUCCESS;

        hr = pEnum->Next
            (
            WBEM_INFINITE,
            1,
            &pObj,
            &uReturned
            );

        if (FAILED(hr))
        {
            ssErrMessage << "WmiHelper::GetData: IEnumWbemClassObject->Next failed." << "Query: " << CW2A(wQueryStream.str().c_str()) << ". ErrorCode " << std::hex << hr;
            m_errorMessage = ssErrMessage.str();
            break;
        }

        if (uReturned == 0)
            break;

        hr = processor.Process(pObj);
        // Log any failures with current object
        if (FAILED(hr)) {
            m_errorMessage = processor.GetErrorMessage();
            DebugPrintf(SV_LOG_ERROR, "%s: error: %s\n", __FUNCTION__, m_errorMessage.c_str());
        }
    }

Cleanup:
    if (!FAILED(hr))
        hr = S_OK;

    return hr;
}

std::string WmiHelper::GetErrorMessage()
{
    return m_errorMessage;
}

WmiHelper::~WmiHelper()
{
    if (m_initialized) {
        CoUninitialize();
    }
}

std::set<std::string>   WmiDiskProcessor::GetDiskNames()
{
    return m_diskNameSet;
}

HRESULT MsftPhysicalDiskRecordProcessor::Process(IWbemClassObject *pDiskObj)
{
    HRESULT             hr = S_OK;
    BOOL                bCanPool;
    CComVariant         varCanPool;
    CComVariant         varCannotPoolReason;
    CComVariant         varWDeviceId;
    std::wstring        wcsDeviceId;
    ULONG               ulDeviceNum;
    std::stringstream   ssDeviceName;
    std::stringstream   ssErrorMsg;

    hr = pDiskObj->Get(L"DeviceId", 0, &varWDeviceId, NULL, NULL);
    if (FAILED(hr) || (VT_BSTR != V_VT(&varWDeviceId))) {
        ssErrorMsg << "MSFT_PhysicalDisk DeviceId failed with. Error Code " << std::hex << hr << " type: " << V_VT(&varWDeviceId);;
        m_errorMessage = ssErrorMsg.str();
        return hr;
    }

    wcsDeviceId = _bstr_t(V_BSTR(&varWDeviceId));
    ulDeviceNum = _wtol(wcsDeviceId.c_str());
    ssDeviceName << DISK_NAME_PREFIX << ulDeviceNum;

    std::string strPhysicalDiskName = ssDeviceName.str();
    std::transform(strPhysicalDiskName.begin(), strPhysicalDiskName.end(), strPhysicalDiskName.begin(), ::toupper);

    STORAGE_BUS_TYPE    busType;

    if (!GetBusType(strPhysicalDiskName, busType, m_errorMessage)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        ssErrorMsg << "MsftPhysicalDiskRecordProcessor: " << strPhysicalDiskName << " GetBusType failed with.Error Code " << GetLastError();
        m_errorMessage = ssErrorMsg.str();
        DebugPrintf(SV_LOG_ERROR, "Error: %s", m_errorMessage.c_str());
        return hr;
    }

    // skip any disk reported on storage space bus
    if (BusTypeSpaces == busType) {
        return S_OK;
    }

    hr = pDiskObj->Get(L"CanPool", 0, &varCanPool, NULL, NULL);
    if (FAILED(hr) || (VT_BOOL != V_VT(&varCanPool))) {
        ssErrorMsg << "MSFT_PhysicalDisk: " << strPhysicalDiskName << " CanPool failed with. Error Code " << std::hex << hr << " type: " << V_VT(&varCanPool);
        m_errorMessage = ssErrorMsg.str();
        return hr;
    }

    bCanPool = varCanPool.boolVal;

    // Not a part of pool. Ignore this disk
    if (bCanPool) {
        return hr;
    }

    hr = pDiskObj->Get(L"CannotPoolReason", 0, &varCannotPoolReason, NULL, NULL);
    if (FAILED(hr) || ((VT_ARRAY | VT_I4) != V_VT(&varCannotPoolReason))) {
        ssErrorMsg << "MSFT_PhysicalDisk: " << strPhysicalDiskName << " CannotPoolReason failed with. Error Code " << std::hex << hr << " type: " << V_VT(&varCannotPoolReason);
        m_errorMessage = ssErrorMsg.str();
        return hr;
    }

    LONG    lLower, lUpper;

    SAFEARRAY *pSafeArray = varCannotPoolReason.parray;
    if (NULL == pSafeArray) {
        return S_OK;
    }

    hr = SafeArrayGetLBound(pSafeArray, 1, &lLower);
    if (FAILED(hr)) {
        ssErrorMsg << "MSFT_PhysicalDisk: " << strPhysicalDiskName << " CannotPoolReason SafeArrayGetLBound failed with. Error Code " << std::hex << hr;
        m_errorMessage = ssErrorMsg.str();
        return hr;
    }

    hr = SafeArrayGetUBound(pSafeArray, 1, &lUpper);
    if (FAILED(hr)) {
        ssErrorMsg << "MSFT_PhysicalDisk: " << strPhysicalDiskName << " CannotPoolReason SafeArrayGetUBound failed with. Error Code " << std::hex << hr;
        m_errorMessage = ssErrorMsg.str();
        return hr;
    }

    for (long l = lLower; l <= lUpper; l++) {
        CComVariant                    varElement;

        hr = SafeArrayGetElement(pSafeArray, &l, &varElement);
        if (FAILED(hr)) {
            ssErrorMsg << "MSFT_PhysicalDisk: " << strPhysicalDiskName << " CannotPoolReason  SafeArrayGetElement " << l << " failed with. Error Code " << std::hex << hr;
            m_errorMessage = ssErrorMsg.str();
            break;
        }

        if (CannotPoolReasonAlreadyPooled == varElement.vt) {
            m_storagePoolDiskNames.insert(strPhysicalDiskName);
            m_diskNameSet.insert(strPhysicalDiskName);
            break;
        }
    }

    return hr;
}

std::set<std::string>   MsftPhysicalDiskRecordProcessor::GetStoragePoolDisks()
{
    return m_storagePoolDiskNames;
}

std::set<std::string> Win32DiskDriveRecordProcessor::GetStorageSpaceDisks()
{
    return m_storageSpaceDiskNames;
}

HRESULT Win32DiskDriveRecordProcessor::Process(IWbemClassObject *pDiskObj)
{
    HRESULT             hr = S_OK;
    CComVariant         varWDeviceId;
    std::stringstream   ssErrorMsg;

    hr = pDiskObj->Get(L"DeviceID", 0, &varWDeviceId, 0, 0);
    if (FAILED(hr) || (VT_BSTR != V_VT(&varWDeviceId))) {
        ssErrorMsg << "Win32DiskDrive DeviceID failed with. Error Code " << std::hex << hr << " type: " << V_VT(&varWDeviceId);
        m_errorMessage = ssErrorMsg.str();
        DebugPrintf(SV_LOG_ERROR, "Error: %s", m_errorMessage.c_str());
        return hr;
    }

    std::string strPhysicalDiskName = std::string(CW2A(varWDeviceId.bstrVal));
    std::transform(strPhysicalDiskName.begin(), strPhysicalDiskName.end(), strPhysicalDiskName.begin(), ::toupper);

    if (IsWindows8OrGreater()) {
        STORAGE_BUS_TYPE    busType;

        if (!GetBusType(strPhysicalDiskName.c_str(), busType, m_errorMessage)) {
            hr = HRESULT_FROM_WIN32(GetLastError());
            ssErrorMsg << "Win32DiskDrive :" << strPhysicalDiskName << " GetBusType failed with. Error Code " << std::hex << hr;
            m_errorMessage = ssErrorMsg.str();
            DebugPrintf(SV_LOG_ERROR, "Error: %s", m_errorMessage.c_str());
            return hr;
        }

        if (BusTypeSpaces == busType) {
            m_storageSpaceDiskNames.insert(strPhysicalDiskName);
            return hr;
        }
    }

    m_diskNameSet.insert(strPhysicalDiskName);
    return hr;
}


std::string  WmiRecordProcessor::GetErrorMessage(void)
{
    return m_errorMessage;
}

HRESULT     Win32NetworkAdapterConfigProcessor::Process(IWbemClassObject *precordobj)
{
    HRESULT             hr;
    CComVariant         varIPAddresses;
    std::stringstream   ssErrorMsg;

    hr = precordobj->Get(L"IPAddress", 0, &varIPAddresses, NULL, NULL);

    if (FAILED(hr) || ((VT_ARRAY | VT_BSTR) != V_VT(&varIPAddresses))){
        ssErrorMsg << "Win32_NetworkAdapterConfiguration IPAddress failed with. Error Code " << std::hex << hr << " type: " << V_VT(&varIPAddresses);
        m_errorMessage = ssErrorMsg.str();
        return hr;
    }

    LONG    lLower, lUpper;

    SAFEARRAY *pSafeArray = varIPAddresses.parray;
    if (NULL == pSafeArray) {
        return S_OK;
    }

    hr = SafeArrayGetLBound(pSafeArray, 1, &lLower);
    if (FAILED(hr)) {
        ssErrorMsg << "Win32_NetworkAdapterConfiguration IPAddress SafeArrayGetLBound failed with. Error Code " << std::hex << hr;
        m_errorMessage = ssErrorMsg.str();
        return hr;
    }

    hr = SafeArrayGetUBound(pSafeArray, 1, &lUpper);
    if (FAILED(hr)) {
        ssErrorMsg << "Win32_NetworkAdapterConfiguration IPAddress SafeArrayGetUBound failed with. Error Code " << std::hex << hr;
        m_errorMessage = ssErrorMsg.str();
        return hr;
    }

    BSTR    *pIpAddresses = NULL;

    hr = SafeArrayAccessData(pSafeArray, (void **)&pIpAddresses);
    if (FAILED(hr)) {
        ssErrorMsg << "Win32_NetworkAdapterConfiguration IPAddress SafeArrayAccessData failed with. Error Code " << std::hex << hr;
        m_errorMessage = ssErrorMsg.str();
        return hr;
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s: IpAddresses:", __FUNCTION__);
    for (long l = lLower; l <= lUpper; l++) {
        std::string             ipAddress;
        ipAddress = CW2A(pIpAddresses[l]);
        m_ipAddresses.push_back(ipAddress);
        DebugPrintf(SV_LOG_ALWAYS, " %s", ipAddress.c_str());
    }
    DebugPrintf(SV_LOG_ALWAYS, "\n");

    hr = SafeArrayUnaccessData(pSafeArray);
    if (FAILED(hr)) {
        ssErrorMsg << "Win32_NetworkAdapterConfiguration IPAddress SafeArrayUnaccessData failed with. Error Code " << std::hex << hr;
        m_errorMessage = ssErrorMsg.str();
    }

    return hr;
}

std::vector<std::string>   Win32NetworkAdapterConfigProcessor::GetIPAddresses()
{
    return m_ipAddresses;
}

HRESULT     Win32ComputerSystem::Process(IWbemClassObject *precordobj)
{
    HRESULT             hr;
    CComVariant         varModel;
    CComVariant         varManufacturer;
    std::stringstream   ssErrorMsg;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    hr = precordobj->Get(L"Model", 0, &varModel, NULL, NULL);

    if (FAILED(hr) || (VT_BSTR != V_VT(&varModel))){
        ssErrorMsg << "Win32_ComputerSystem Model failed with. Error Code " << std::hex << hr << " type: " << V_VT(&varModel);
        m_errorMessage = ssErrorMsg.str();
        DebugPrintf(SV_LOG_ERROR, m_errorMessage.c_str());
        return hr;
    }

    std::string     strModel = CW2A(varModel.bstrVal);
    DebugPrintf(SV_LOG_DEBUG, "Model: %s\n", strModel.c_str());

    hr = precordobj->Get(L"Manufacturer", 0, &varManufacturer, NULL, NULL);

    if (FAILED(hr) || (VT_BSTR != V_VT(&varManufacturer))){
        ssErrorMsg << "Win32_ComputerSystem Manufacturer failed with. Error Code " << std::hex << hr << " type: " << V_VT(&varManufacturer);
        m_errorMessage = ssErrorMsg.str();
        DebugPrintf(SV_LOG_ERROR, m_errorMessage.c_str());
        return hr;
    }


    std::string     strManufacturer = CW2A(varManufacturer.bstrVal);
    DebugPrintf(SV_LOG_DEBUG, "Manufacturer: %s\n", strManufacturer.c_str());

    size_t lenmf = strlen(HYPERVMANUFACTURER);
    size_t lenmdl = strlen(HYPERVMODEL);

    m_isVirtual = false;

    if ((0 == strncmp(HYPERVMANUFACTURER, strManufacturer.c_str(), lenmf)) &&
        (0 == strncmp(HYPERVMODEL, strModel.c_str(), lenmdl)))
    {
        m_isVirtual = true;
        m_hypervisorName = HYPERVNAME;
        DebugPrintf(SV_LOG_DEBUG, "Hypervisor: Hyper-v virtual\n");
    }

    if (false == m_isVirtual)
    {
        lenmf = strlen(VMWAREPAT);
        if (0 == strncmp(VMWAREPAT, strManufacturer.c_str(), lenmf))
        {
            m_isVirtual = true;
            m_hypervisorName = VMWARENAME;
            DebugPrintf(SV_LOG_DEBUG, "Hypervisor: VMWARE virtual\n");
        }
    }

    if (!m_isVirtual) {
        m_hypervisorName = "physical";
        DebugPrintf(SV_LOG_DEBUG, "No hypervisor.. Physical Box\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return S_OK;
}

bool    Win32ComputerSystem::IsVirtual(std::string& hypervisor)
{
    hypervisor = m_hypervisorName;
    return m_isVirtual;
}

bool IsVirutalMachine(bool& isVirtual, std::string& hypervisorName)
{

    HRESULT                             hr = S_OK;
    WmiHelper                           wmiHelper;
    Win32ComputerSystem                 computerSystemProcessor;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    hr = wmiHelper.Init();
    if (FAILED(hr))
    {
        DebugPrintf(SV_LOG_ERROR, "Com Initialization failed. Error Code %0x\n", hr);
        DebugPrintf(SV_LOG_ERROR, "%s Error Code %0x\n", wmiHelper.GetErrorMessage(), hr);
        return (hr == S_OK);
    }

    hr = wmiHelper.GetData(L"Root\\cimv2", L"Win32_ComputerSystem", computerSystemProcessor);
    if (FAILED(hr)) {
        DebugPrintf(SV_LOG_ERROR, "%s hr = %x\n", wmiHelper.GetErrorMessage(), hr);
        return false;
    }

    isVirtual = computerSystemProcessor.IsVirtual(hypervisorName);
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return true;
}