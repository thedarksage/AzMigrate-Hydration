#ifndef __DRDRILL_REPORT_H
#define __DRDRILL_REPORT_H



#include "Common.h"
using namespace DrDrillNS;

#include "ProductValidator.h"
#include <fstream>


class DrDrillReport
{
	public:
		//virtual bool Create()= 0;
		DrDrillReport(void);
		~DrDrillReport(void);
		void CreateReport();
		ProtectionValidator m_Validator;
    private:
		void WriteToFile();
		void WriteToScreen();
		_tstring m_strFilePath;
		std::fstream m_fsObj;
		bool m_bScreenDisplay;
};
#endif