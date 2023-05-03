#ifndef __APPLICATION__H
#define __APPLICATION__H

#include "appglobals.h"
#include <boost/shared_ptr.hpp>

class Application
{
public:
    virtual SVERROR discoverApplication() = 0 ;
    virtual bool isInstalled() = 0 ;
	virtual SVERROR discoverMetaData() = 0 ;
} ;



typedef boost::shared_ptr<Application> ApplicationPtr ;

#endif