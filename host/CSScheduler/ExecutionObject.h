#ifndef __EXECUTION_OBJECT_H_
#define __EXECUTION_OBJECT_H_
#include "common.h"

class ERP;
 
class BaseObject
{
public:

	virtual SCH_STATUS execute(ERP* erp)=0;
	virtual BaseObject *clone()=0;

	virtual ~BaseObject(){ }
};

class FileRepImpl:public BaseObject
{
public:

	SCH_STATUS execute(ERP* erp);

	BaseObject *clone()
	{
		FileRepImpl *tmp = new FileRepImpl;
		return tmp;
	}

	~FileRepImpl(){ }
};


class BaseSnapshot:public BaseObject
{
public:

	SCH_STATUS execute(ERP* erp);

	BaseObject *clone()
	{
		BaseSnapshot *tmp = new BaseSnapshot;
		return tmp;
	}

	void ReplaceChar(char *pszInString, char inputChar, char outputChar) ;
	~BaseSnapshot(){ }
};

class FileRepInit:public BaseObject
{
public:

	SCH_STATUS execute(ERP *perp);

	BaseObject *clone()
	{
		FileRepInit *tmp = new FileRepInit;
		return tmp;
	}

	~FileRepInit(){ }
};

#endif
