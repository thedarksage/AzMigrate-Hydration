#include <algorithm>
#include <utility>
#include <windows.h>
#include <atlbase.h>
#include "hostinfocollectormajor.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "configwrapper.h"


bool WmiProcessorClassRecordProcessor::Process(IWbemClassObject *precordobj)
{
	if (0 == m_pCpuInfos)
	{
		m_ErrMsg = "cpu infos to fill is not supplied in process wmi processor class";
		return false;
	}

	if (0 == precordobj)
	{
		m_ErrMsg = "record object is not specified for wmi processor class";
		return false;
	}

	bool bprocessed = true;
	const char DEVICEIDCOLUMNNAME[] = "DeviceID";
	const char NAMECOLUMNNAME[] = "Name";
	const char ARCHITECTURECOLUMNNAME[] = "Architecture";
	const char MANUFACTURERCOLUMNNAME[] = "Manufacturer";
	const char NUMCORESCOLUMNNAME[] = "NumberOfCores";
	const char NUMLOGICALPROCESSORSCOLUMNNAME[] = "NumberOfLogicalProcessors";

	USES_CONVERSION;
	VARIANT vtProp;
    HRESULT hrCol;

	std::string deviceid;
	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(DEVICEIDCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
		if (VT_BSTR == V_VT(&vtProp))
		{
			deviceid = W2A(V_BSTR(&vtProp));
		}
        VariantClear(&vtProp);
    }
	else
	{
		bprocessed = false;
		m_ErrMsg = "failed to get ";
		m_ErrMsg += DEVICEIDCOLUMNNAME;
		m_ErrMsg += " from wmi processor class";
		return bprocessed;
	}

	if (deviceid.empty())
	{
		return bprocessed;
	}

	std::pair<CpuInfosIter_t, bool> pr = m_pCpuInfos->insert(std::make_pair(deviceid, Object()));
	CpuInfosIter_t cit = pr.first;
	Object &ci = cit->second;
	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(NAMECOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
		if (VT_BSTR == V_VT(&vtProp))
		{
			ci.m_Attributes.insert(std::make_pair(NSCpuInfo::NAME, W2A(V_BSTR(&vtProp))));
		}
        VariantClear(&vtProp);
    }
	else
	{
		bprocessed = false;
		m_ErrMsg = "failed to get ";
		m_ErrMsg += NAMECOLUMNNAME;
		m_ErrMsg += " from wmi processor class";
	}

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(MANUFACTURERCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
		if (VT_BSTR == V_VT(&vtProp))
		{
			ci.m_Attributes.insert(std::make_pair(NSCpuInfo::MANUFACTURER, W2A(V_BSTR(&vtProp))));
		}
        VariantClear(&vtProp);
    }
	else
	{
		bprocessed = false;
		m_ErrMsg = "failed to get ";
		m_ErrMsg += MANUFACTURERCOLUMNNAME;
		m_ErrMsg += " from wmi processor class";
	}

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(NUMCORESCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
		if (VT_I4 == V_VT(&vtProp))
		{
			std::stringstream ssnc;
			ssnc << V_I4(&vtProp);
			ci.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_CORES, ssnc.str()));
		}
        VariantClear(&vtProp);
    }
	else
	{
		bprocessed = false;
		m_ErrMsg = "failed to get ";
		m_ErrMsg += NUMCORESCOLUMNNAME;
		m_ErrMsg += " from wmi processor class";
	}

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(NUMLOGICALPROCESSORSCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
		if (VT_I4 == V_VT(&vtProp))
		{
		    std::stringstream ssnl;
			ssnl << V_I4(&vtProp);
			ci.m_Attributes.insert(std::make_pair(NSCpuInfo::NUM_LOGICAL_PROCESSORS, ssnl.str()));
		}
        VariantClear(&vtProp);
    }
	else
	{
		bprocessed = false;
		m_ErrMsg = "failed to get ";
		m_ErrMsg += NUMLOGICALPROCESSORSCOLUMNNAME;
		m_ErrMsg += " from wmi processor class";
	}

	std::map<unsigned short, std::string> codetoarch;
	codetoarch.insert(std::make_pair(0, "x86"));
	codetoarch.insert(std::make_pair(1, "MIPS"));
	codetoarch.insert(std::make_pair(2, "Alpha"));
	codetoarch.insert(std::make_pair(3, "PowerPC"));
	codetoarch.insert(std::make_pair(6, "Itanium-based systems"));
	codetoarch.insert(std::make_pair(9, "x64"));

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(ARCHITECTURECOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
		if (VT_I4 == V_VT(&vtProp))
		{
			ci.m_Attributes.insert(std::make_pair(NSCpuInfo::ARCHITECTURE, codetoarch[V_I4(&vtProp)]));
		}
        VariantClear(&vtProp);
    }
	else
	{
		bprocessed = false;
		m_ErrMsg = "failed to get ";
		m_ErrMsg += ARCHITECTURECOLUMNNAME;
		m_ErrMsg += " from wmi processor class";
	}

	return bprocessed;
}
