#ifndef MASTER_TARGET
#define MASTER_TARGET

#include "Common.h"
using namespace std;
class MasterTarget:public Common
{
public:
	MasterTarget()
	{
		cout<<"\n in the Master tgt constructor"<<endl;
	}
	~MasterTarget()
	{
		cout<<"\n in the Master tgt destructor"<<endl;
	}
	Common sd;
};
#endif