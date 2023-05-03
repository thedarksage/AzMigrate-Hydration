#include <string>
#include <sstream>
#include "error.h"
#include "host.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "genericwmi.h"
#include "hostinfocollectormajor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "configwrapper.h"
#include "errorexception.h"

extern std::string ValidateAndGetIPAddress(const std::string &ipAddress, std::string &errMsg);

bool Host::SetCpuInfos() 
{
    bool bset = false;

	WmiProcessorClassRecordProcessor p(&m_CpuInfos);
    GenericWMI gwmi(&p);

    SVERROR sv = gwmi.init();
    if (sv != SVS_OK)
    {
		DebugPrintf(SV_LOG_ERROR, "Failed to initialize the generice wmi\n");
    }
    else
    {
		bset = true;
		gwmi.GetData("Win32_Processor");
	}

    return bset;
}


bool Host::SetMemory()
{
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof (statex);

	bool bset = GlobalMemoryStatusEx(&statex);
	if (bset)
	{
		m_Memory = statex.ullTotalPhys;
	}
	else
	{
		std::stringstream ss;
		ss << GetLastError();
		DebugPrintf(SV_LOG_ERROR, "GlobalMemoryStatusEx failed with errno %s\n", ss.str().c_str());
	}

	return bset;
}

bool Host::GetFreeMemory(unsigned long long &freeMemory) const
{
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof (statex);

    bool bset = GlobalMemoryStatusEx(&statex);
    if (bset)
    {
        freeMemory = statex.ullAvailPhys;
    }
    else
    {
        std::stringstream ss;
        ss << GetLastError();
        DebugPrintf(SV_LOG_ERROR, "GlobalMemoryStatusEx failed with errno %s\n", ss.str().c_str());
    }

    return bset;
}

bool Host::GetSystemUptimeInSec(unsigned long long& uptime) const
{
    uptime = GetTickCount64();
    // convert to secs as tick count is in milli seconds
    uptime /= 1000;
    return true;
}

void Host::GetIPAddressInAddrInfo(strset_t &ipsInAddrInfo, std::string &errMsg) const
{
    std::string sHostName = Host::GetInstance().GetHostName();

    AutoLock CreateGuard(m_SyncLock); // Add the lock after GetHostName since there is a lock inside GetHostName too.

    try
    {
        if (!sHostName.empty())
        {
            WSADATA wsaData;
            int iretv;

            iretv = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (iretv != 0)
            {
                std::stringstream sserr;
                sserr << "WSAStartup failed. Error: ";
                sserr << iretv;
                errMsg += sserr.str();
                errMsg += ", ";
                return;
            }

            addrinfoW *paddInfo = NULL;
            DWORD dwretv = GetAddrInfoW(convertUtf8ToWstring(sHostName).c_str(), NULL, NULL, &paddInfo);
            if (0 == dwretv)
            {
                LPSOCKADDR socaddr_ip = NULL;
                for (addrinfoW *ptmp = paddInfo; ptmp != NULL; ptmp = ptmp->ai_next)
                {
                    if (ptmp->ai_family == AF_INET)
                    {
                        socaddr_ip = (LPSOCKADDR)ptmp->ai_addr;

                        std::wstring ipwsBuffer(MAXIPV4ADDRESSLEN + 1, 0);
                        DWORD ipBuffLen = ipwsBuffer.size();
                        iretv = WSAAddressToStringW(socaddr_ip, (DWORD)ptmp->ai_addrlen, NULL, (LPWSTR)&ipwsBuffer[0], &ipBuffLen);
                        if (0 != iretv){
                            std::stringstream sserr;
                            iretv = WSAGetLastError();
                            sserr << "WSAAddressToStringW failed. Error: ";
                            sserr << iretv;
                            errMsg += sserr.str();
                            errMsg += ", ";
                        }

                        std::string err;
                        std::string ipAddr = ValidateAndGetIPAddress(convertWstringToUtf8(ipwsBuffer.c_str()), err);
                        if (!ipAddr.empty())
                        {
                            ipsInAddrInfo.insert(ipAddr);
                        }
                        else
                        {
                            errMsg += err;
                            errMsg += ", ";
                        }
                    }
                }
                if (NULL != paddInfo)
                {
                    FreeAddrInfoW(paddInfo);
                }
            }
            else
            {
                iretv = WSAGetLastError();
                std::stringstream sserr;
                sserr << "GetAddrInfoW failed. Error: ";
                sserr << iretv;
                errMsg += sserr.str();
                errMsg += ", ";
            }
        }
        else
        {
            errMsg += "HostName is blank.";
        }
    }
    catch (std::exception exc)
    {
        errMsg += exc.what();
    }
    WSACleanup();

	return;
}


const std::string Host::GetHostName() const
{
    AutoLock CreateGuard(m_SyncLock);
    std::wstring wszTemp(MAXHOSTNAMELEN + 1, 0);
    size_t wszTempLen = wszTemp.size();
    if (0 == GetComputerNameW((LPWSTR)&wszTemp[0], LPDWORD(&wszTempLen)))
    {
        DWORD err = GetLastError();
        DebugPrintf(SV_LOG_ERROR,
            "GetComputerNameW failed: Couldn't determine local host name. Error: %d. @LINE %d in FILE %s \n",
            err,
            LINE_NO,
            FILE_NAME);
        throw ERROR_EXCEPTION << "GetComputerNameW failed. Error: " << err << '\n';
    }
    return convertWstringToUtf8(wszTemp.c_str());
}

const std::string Host::GetFQDN() const
{
    AutoLock CreateGuard(m_SyncLock);
    std::wstring wszTemp(MAXHOSTNAMELEN + 1, 0);
    size_t wszTempLen = wszTemp.size();

    while (true)
    {
        if (0 == GetComputerNameExW(ComputerNamePhysicalDnsFullyQualified, (LPWSTR)&wszTemp[0], LPDWORD(&wszTempLen)))
        {
            DWORD err = GetLastError();
            if (err == ERROR_MORE_DATA)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: GetComputerNameExW requires buffer len %d\n", FUNCTION_NAME, wszTempLen);

                wszTemp.resize(wszTempLen, 0);
                continue;
            }

            DebugPrintf(SV_LOG_ERROR,
                "GetComputerNameExW failed: Couldn't determine local host FQDN. Error: %d.\n",
                err);
            throw ERROR_EXCEPTION << "GetComputerNameExW failed. Error: " << err << '\n';
        }

        break;
    }

    std::string sFqdn = convertWstringToUtf8(wszTemp.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s with FQDN %s\n", FUNCTION_NAME, sFqdn.c_str());

    return sFqdn;
}

