#ifndef STUB__HANDLER__H
#define STUB__HANDLER__H

#include "Handler.h"

class StubHandler :
	public Handler
{
	INM_ERROR m_errCode;
public:
	StubHandler(void);
	~StubHandler(void);
	virtual INM_ERROR process(FunctionInfo& request);
	virtual INM_ERROR validate(FunctionInfo& request);
	INM_ERROR updateErrorCode(INM_ERROR errCode)
	{
		return Handler::updateErrorCode(errCode);
	}
};


#endif /* STUB__HANDLER__H */

