#include "archivefactory.h"
#include "defs.h"
#include "configvalueobj.h"
#include "icatexception.h"


archiveFactoryPtr archiveFactory::m_instance ;
bool archiveFactory::m_valid = false ;

/*
* Function Name: archiveFactory::getInstance
* Arguements: 
*                  None
* Return Value:
*                  archiveFactoryPtr : Returns the archive factory instance
* Description:                    
* Called by: 
*                  archiveController::init
*                  archiveProcess::archiveProcess
* Calls:
    
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
archiveFactoryPtr archiveFactory::getInstance()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	if( m_valid == false )
	{
		m_instance.reset( new (std::nothrow) archiveFactory) ;
		if( m_instance.get() == NULL )
		{
			throw icatOutOfMemoryException("Failed to create the icatFactory Instance\n") ;
		}
		m_valid = true ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return m_instance ;
}

/*
* Function Name: archiveFactory::createArchiveProcess
* Arguements: 
*                  None
* Return Value:
*                  archiveProcessPtr : Returns the archive processor object pointer 
* Description: Creates the archive processor object
* Called by: 
*                  archiveController::init
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
archiveProcessPtr archiveFactory::createArchiveProcess()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	ConfigValueObject * obj = ConfigValueObject::getInstance() ;
	archiveProcessPtr arcProcessPtr ;
	if( obj->getTransport().compare("http") == 0 || 
		obj->getTransport().compare("cifs") == 0 || 
		obj->getTransport().compare("nfs") == 0 )
	{
		arcProcessPtr.reset(new (std::nothrow) ArchiveProcessImpl) ;
	}
	if( arcProcessPtr.get() == NULL )
	{
		throw icatOutOfMemoryException("Failed to create the file icat processor Object\n") ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return arcProcessPtr ;
}


filelistProcessPtr archiveFactory::createFileListingProcess()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    filelistProcessPtr flistProcessPtr ;
    {
        flistProcessPtr.reset(new (std::nothrow) FilelistProcessImpl ) ;
    }
    if( flistProcessPtr.get() == NULL )
    {
        throw icatOutOfMemoryException("Failed to create the file icat processor Object\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return flistProcessPtr ;
}



/*
* Function Name: archiveFactory::createArchiveObject
* Arguements: 
*                  None
* Return Value:
*                  ArchiveObjPtr : Returns the archive object pointer 
* Description: Creates the archive object 
                            HttpArchiveObj - if transport is http
                            CifsNfsArchiveObj - if transport is nfs or cifs
* Called by: 
*                  archiveProcess::archiveProcess
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
ArchiveObjPtr archiveFactory::createArchiveObject()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	ConfigValueObject * obj = ConfigValueObject::getInstance() ;
	ArchiveObjPtr objPtr ;
	if( obj->getTransport().compare("http") == 0 )
	{
		objPtr.reset(new (std::nothrow) HttpArchiveObj) ;
	}
	if( obj->getTransport().compare("cifs") == 0 || obj->getTransport().compare("nfs") == 0 )
	{
		objPtr.reset(new (std::nothrow) CifsNfsArchiveObj) ;
	}

	if( objPtr.get() == NULL )
	{
		throw icatOutOfMemoryException("Failed to create the file icat archive object\n") ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return objPtr ;
}
