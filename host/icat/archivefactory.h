#ifndef ARCHIVE_FACTORY_H
#define ARCHIVE_FACTORY_H

#include <boost/shared_ptr.hpp>
#include "archiveobject.h"
#include "archiveprocess.h"
class archiveFactory ;
typedef boost::shared_ptr<archiveFactory> archiveFactoryPtr ;

class archiveFactory
{
	static bool m_valid ;
	static archiveFactoryPtr m_instance ;
public:
	static archiveFactoryPtr getInstance() ;
	archiveProcessPtr createArchiveProcess() ;
	filelistProcessPtr createFileListingProcess();
	ArchiveObjPtr createArchiveObject() ;
} ;

#endif

