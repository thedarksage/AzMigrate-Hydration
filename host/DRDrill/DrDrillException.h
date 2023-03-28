#ifndef __DRDRILL_EXCEPTION_H
#define __DRDRILL_EXCEPTION_H


#include "Common.h"
using namespace DrDrillNS;


class DrDrillException 
{
	public:
		DrDrillException(void);
		~DrDrillException(void);
		void SetDescription(std::string strDescription);
		void SetErrorCode(ULONG ulErrorCode);
		_tstring GetDescription();
		ULONG GetErrorCode();

    private:
		_tstring m_strDescription;
		ULONG m_ulErrorCode;
  };
#endif