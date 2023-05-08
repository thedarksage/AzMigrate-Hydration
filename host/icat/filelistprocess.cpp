#include "archiveprocess.h"
#include "defs.h"
#include "configvalueobj.h"
#include "archivecontroller.h"
#include "icatexception.h"
#include "resumetracker.h"
#include "archivefactory.h"


void FilelistProcessImpl::listingIntoFile(fileSystemObj obj)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);

	m_outputfile << obj.absPath().c_str();
	m_outputfile <<std::endl;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
    return;
}
void FilelistProcessImpl::doProcess(fileSystemObj obj)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    switch( obj.getOperation() )
    {
        case ICAT_OPER_LISTINTOFILE :
            listingIntoFile(obj);
            break ;
        default :
            break ;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
}


void FilelistProcessImpl::init()
{
    ConfigValueObject *temp_obj = ConfigValueObject::getInstance() ;
    std::string FileListPath = temp_obj->getFilelistToTarget();
    m_outputfile.open(FileListPath.c_str(),ios::trunc);
}
void FilelistProcessImpl::closeFile()
{
    m_outputfile.close();
}
int FilelistProcessImpl::open(void *args)
{
    return this->activate(THR_BOUND);
}
int FilelistProcessImpl::close(u_long flags)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    return 0;
}
int FilelistProcessImpl::svc()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG, "Thread started. (Archive Process)\n") ;
	m_resume->Signal(true) ;
	fileSystemObj *obj = NULL ;
	init();
	try
	{
        while( !archiveController::getInstance()->QuitSignalled() && 
                m_queuePtr.get() != NULL)
		{
			if( m_resume->IsSignalled() )
			{
				if( getNextObj(&obj) )
				{
					doProcess(*obj);
					DebugPrintf(SV_LOG_DEBUG, "processed %s\n", obj->absPath().c_str()) ;
                    delete obj ;
                    obj = NULL ;
				}
				else
				{
					if (!isDeleteQueue()) 
					{
						DebugPrintf(SV_LOG_DEBUG, "Didnt find any objects in queue... sending ICAT_MSG_AP_DONE to controller\n") ;
						archiveController::getInstance()->postMsg(ICAT_MSG_AP_DONE, ICAT_MSGPRIO_NORMAL, this) ;
					}
				}
			}
			else
			{
				if( isDeleteQueue() )
				{
					break ;
				}
			}
		}
	}
	catch(icatOutOfMemoryException &oome)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception : %s\n", oome.what()) ;
		archiveController::getInstance()->QuitSignal() ;
	}
	catch(icatException&e)
	{
		DebugPrintf(SV_LOG_ERROR, "Exception : %s\n", e.what()) ;
		archiveController::getInstance()->QuitSignal() ;
	}
	closeFile();	
	DebugPrintf(SV_LOG_DEBUG, "Thread exited. (Archive Process)\n") ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return 0 ;
}
