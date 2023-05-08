#ifndef HOSTINFO_COLLECTOR_MAJOR_H
#define HOSTINFO_COLLECTOR_MAJOR_H

#include "volumegroupsettings.h"
#include "genericwmi.h"

class WmiProcessorClassRecordProcessor : public GenericWMI::WmiRecordProcessor
{
public:
	WmiProcessorClassRecordProcessor(CpuInfos_t *p)
		:m_pCpuInfos(p)
	{
	}
	bool Process(IWbemClassObject *precordobj);
	std::string GetErrMsg(void)
	{
		return m_ErrMsg;
	}

private:
	CpuInfos_t *m_pCpuInfos;
	std::string m_ErrMsg;
};

#endif 
