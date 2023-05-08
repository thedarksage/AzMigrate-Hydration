#ifndef VMASSOCIATION__HANDLER__H
#define VMASSOCIATION__HANDLER__H

#include "Handler.h"

class VmAssociationHandler :
	public Handler
{
public:
	VmAssociationHandler(void);
	~VmAssociationHandler(void);
	virtual INM_ERROR process(FunctionInfo& request);
	virtual INM_ERROR validate(FunctionInfo& request);
};


#endif /* VMASSOCIATION__HANDLER__H */
