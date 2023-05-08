#include <algorithm>
#include <windows.h>
#include <atlbase.h>
#include "operatingsysteminfomajor.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "configwrapper.h"


bool WmiOSClassRecordProcessor::Process(IWbemClassObject *precordobj)
{
	if (0 == m_pOsInfo)
	{
		m_ErrMsg = "os info to fill is not supplied in process wmi os class";
		return false;
	}

	if (0 == precordobj)
	{
		m_ErrMsg = "record object is not specified for wmi os class";
		return false;
	}

	bool bprocessed = true;
	const char CAPTIONCOLUMNNAME[] = "Caption";
    const char LASTBOOTUPTIME[] = "LastBootUpTime";

	USES_CONVERSION;
	VARIANT vtProp;
    HRESULT hrCol;

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(CAPTIONCOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
		if (VT_BSTR == V_VT(&vtProp))
		{
			m_pOsInfo->m_Attributes.insert(std::make_pair(NSOsInfo::CAPTION, W2A(V_BSTR(&vtProp))));
		}
        VariantClear(&vtProp);
    }
	else
	{
		bprocessed = false;
		m_ErrMsg = "failed to get ";
		m_ErrMsg += CAPTIONCOLUMNNAME;
		m_ErrMsg += " from wmi operating system class";
	}

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(LASTBOOTUPTIME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
        if (VT_BSTR == V_VT(&vtProp))
        {
            m_pOsInfo->m_Attributes.insert(std::make_pair(NSOsInfo::LASTBOOTUPTIME, W2A(V_BSTR(&vtProp))));
        }
        VariantClear(&vtProp);
    }
    else
    {
        bprocessed = false;
        m_ErrMsg = "failed to get ";
        m_ErrMsg += LASTBOOTUPTIME;
        m_ErrMsg += " from wmi operating system class";
    }

	return bprocessed;
}

/* Process the response of wmi class Win32_ComputerSystem */
bool WmiCSClassRecordProcessor::Process(IWbemClassObject *precordobj)
{
    if (0 == m_pOsInfo)
    {
        m_ErrMsg = "cs info to fill is not supplied in process wmi cs class";
        return false;
    }

    if (0 == precordobj)
    {
        m_ErrMsg = "record object is not specified for wmi cs class";
        return false;
    }

    bool bprocessed = true;
    const char SYSTEMTYPECOLUMNNAME[] = "SystemType";

    USES_CONVERSION;
    VARIANT vtProp;
    HRESULT hrCol;

    VariantInit(&vtProp);
    hrCol = precordobj->Get(A2W(SYSTEMTYPECOLUMNNAME), 0, &vtProp, 0, 0);
    if (!FAILED(hrCol))
    {
        if (VT_BSTR == V_VT(&vtProp))
        {
            m_pOsInfo->m_Attributes.insert(std::make_pair(NSOsInfo::SYSTEMTYPE, W2A(V_BSTR(&vtProp))));
        }
        VariantClear(&vtProp);
    }
    else
    {
        bprocessed = false;
        m_ErrMsg = "failed to get ";
        m_ErrMsg += SYSTEMTYPECOLUMNNAME;
        m_ErrMsg += " from wmi computer system class";
    }

    return bprocessed;
}

