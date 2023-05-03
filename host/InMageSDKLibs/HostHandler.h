#ifndef HOST__HANDLER__H
#define HOST__HANDLER__H

#include "Handler.h"

class HostHandler :
	public Handler
{
public:
	HostHandler(void);
	~HostHandler(void);
	virtual INM_ERROR process(FunctionInfo& request);
	virtual INM_ERROR validate(FunctionInfo& request);
};


#endif /* HOST__HANDLER__H */
