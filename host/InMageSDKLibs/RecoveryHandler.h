#ifndef RECOVERY__HANDLER__H 
#define RECOVERY__HANDLER__H 

#include "Handler.h"
#include <boost/lexical_cast.hpp>
#include "confschema.h"
#include "confschemamgr.h"
#include "protectionpairobject.h"
#include "retevtpolicyobject.h"

enum RecoveryPointType{COMMON_TAG,COMMON_TIME};

class RecoveryHandler :
	public Handler
{
private:
public:
	RecoveryHandler(void);
	~RecoveryHandler(void);
	virtual INM_ERROR process(FunctionInfo& request);
	virtual INM_ERROR validate(FunctionInfo& request);
	INM_ERROR ListRestorePoints(FunctionInfo& request);
};


#endif /*RECOVERY__HANDLER__H */
