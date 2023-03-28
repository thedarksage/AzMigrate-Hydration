#ifndef OPERATINGSYSTEM_INFO_MAJOR_H
#define OPERATINGSYSTEM_INFO_MAJOR_H

#include "volumegroupsettings.h"
#include "genericwmi.h"

class WmiOSClassRecordProcessor : public GenericWMI::WmiRecordProcessor
{
public:
	WmiOSClassRecordProcessor(Object *p)
		:m_pOsInfo(p)
	{
	}
	bool Process(IWbemClassObject *precordobj);
	std::string GetErrMsg(void)
	{
		return m_ErrMsg;
	}

protected:
	Object *m_pOsInfo;
	std::string m_ErrMsg;
};

class WmiCSClassRecordProcessor : public WmiOSClassRecordProcessor 
{
public:
    WmiCSClassRecordProcessor(Object *p)
        :WmiOSClassRecordProcessor(p)
    {
    }
    bool Process(IWbemClassObject *precordobj);
};

#endif 
