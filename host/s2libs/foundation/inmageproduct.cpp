/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : inmageproduct.cpp
 *
 * Description: 
 */
#include <string>
#include <cassert>

#include "inmageproduct.h"
#include "entity.h"
#include "error.h"
#include "portableheaders.h"
#include "hostagenthelpers_ported.h"
#include "synchronize.h"
#include "version.h"
#include "devicestream.h"
#include "filterdrvifmajor.h"
#include "errorexception.h"

/******************************************************************************************
*** Note: Please write any platform specific code in ${platform}/inmageproduct_port.cpp ***
******************************************************************************************/

std::string  InmageProduct::m_sProductVersion;
std::string  InmageProduct::m_sProductVersionStr;
InmageProduct* InmageProduct::theProduct = NULL;
std::string InmageProduct::m_sDriverVersion;
Lockable InmageProduct::m_CreateLock;

/*
 * FUNCTION NAME : InmageProduct
 *
 * DESCRIPTION : Constructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
InmageProduct::InmageProduct()
{
}

/*
 * FUNCTION NAME : InmageProduct
 *
 * DESCRIPTION : copy constructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
InmageProduct::InmageProduct(const InmageProduct &right)
{
}


/*
 * FUNCTION NAME : ~InmageProduct
 *
 * DESCRIPTION : Destructor
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
InmageProduct::~InmageProduct()
{

}

/*
 * FUNCTION NAME : Init
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
void InmageProduct::Init()
{
    if (!m_bInitialized)
    {
        m_sProductVersionStr = INMAGE_PRODUCT_VERSION_STR;

        std::stringstream   ssProdVersion;
        uint32_t vProdVersion[] = { INMAGE_PRODUCT_VERSION };
        ssProdVersion << vProdVersion[0] << "." << vProdVersion[1] << ".";
        ssProdVersion << vProdVersion[2] << "." << vProdVersion[3];

        m_sProductVersion = ssProdVersion.str();
        m_sDriverVersion = GetFilterDriverVersion();

        m_bInitialized = true;
    }
}

/*
* FUNCTION NAME : GetProductVersionStr
*
* DESCRIPTION :
*
* INPUT PARAMETERS :
*
* OUTPUT PARAMETERS :
*
* NOTES :
*
* return value :
*
*/
const std::string& InmageProduct::GetProductVersionStr()
{
    return m_sProductVersionStr;
}

/*
 * FUNCTION NAME : GetProductVersion
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
const std::string& InmageProduct::GetProductVersion()
{
    return m_sProductVersion;
}

/*
* FUNCTION NAME : GetDriverVersion
*
* DESCRIPTION : abstract to GetFilterDriverVersion
*
* INPUT PARAMETERS :
*
* OUTPUT PARAMETERS :
*
* NOTES :
*
* return value : filter driver version string
*
*/
const std::string& InmageProduct::GetDriverVersion()
{
    if (m_sDriverVersion.empty())
    {
        m_sDriverVersion = GetFilterDriverVersion();
    }
    return m_sDriverVersion;
}

/*
* FUNCTION NAME : GetFilterDriverVersion
*
* DESCRIPTION : Fetch filter driver version using driver IOCTL
*
* INPUT PARAMETERS :
*
* NOTES :
*
* return value : filter driver version string
*
*/
const std::string InmageProduct::GetFilterDriverVersion() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string driverVersion;
    FilterDrvIfMajor drvif;
    std::string filterDriverName = drvif.GetDriverName(VolumeSummary::DISK);

    DeviceStream::Ptr pDriverStream;
    try {
        pDriverStream.reset(new DeviceStream(Device(filterDriverName)));
        DebugPrintf(SV_LOG_DEBUG, "Created device stream.\n");
    }
    catch (std::bad_alloc &e) {
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception: %s.\n", FUNCTION_NAME, e.what());
        return std::string();
    }
    catch (...) {
        DebugPrintf(SV_LOG_ERROR, "%s: instantiation of DeviceStream object failed with unknown exception.\n", FUNCTION_NAME);
        return std::string();
    }

    if (!pDriverStream->IsOpen())
    {
        if (SV_SUCCESS != pDriverStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_Asynchronous | DeviceStream::Mode_Read | DeviceStream::Mode_ShareRead))
        {
            std::stringstream ssErrMsg;
            ssErrMsg << "Failed to open filter driver" << filterDriverName << ". Error: ";
            ssErrMsg << pDriverStream->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, ssErrMsg.str().c_str());
            return std::string();
        }
    }

    DRIVER_VERSION stDrvVersion;
    if (drvif.GetDriverVersion(pDriverStream.get(), stDrvVersion))
    {
        std::stringstream ss;
        ss << stDrvVersion.ulPrMajorVersion
            << "."
            << stDrvVersion.ulPrMinorVersion
            << "."
            << stDrvVersion.ulPrBuildNumber
            << "."
            << stDrvVersion.ulPrMinorVersion2;

        driverVersion = ss.str();
    }
    else
    {
        std::stringstream ssErrMsg;
        ssErrMsg << "Filter driver version ioctl failed. Error: ";
        ssErrMsg << pDriverStream->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, ssErrMsg.str().c_str());
        return std::string();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITING %s driverVersion=\"%s\".\n", FUNCTION_NAME, driverVersion.c_str());
    return driverVersion;
}

/*
 * FUNCTION NAME : Destroy
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
bool InmageProduct::Destroy()
{
    bool bStatus = true;
    if (theProduct)
    {
        AutoLock CreateGuard(m_CreateLock);
        if (theProduct)
        {
            delete theProduct;
            theProduct = NULL;
        }
    }
    return bStatus;
}

/*
 * FUNCTION NAME : GetInstance
 *
 * DESCRIPTION : Creates and does one time initialization of InmageProduct singleton instance
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : If successful returns InmageProduct singleton object
 *
 */
InmageProduct& InmageProduct::GetInstance()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (!theProduct)
    {
        AutoLock CreateGuard(m_CreateLock);
        if (!theProduct)
        {
            theProduct = new InmageProduct;
            theProduct->Init();
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return *theProduct;
	
}

