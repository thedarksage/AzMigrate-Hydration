#include "mssqlmetadata.h"
#include <atlconv.h>
#include "ruleengine.h"

std::string MSSQLInstanceMetaData::summary(std::list<ValidatorPtr>& list)
{
	std::stringstream stream ;
    /*
    stream << "\tInstall Path : "<< this->m_installPath << std::endl ;
    stream << "\tVersion :" << this->m_version <<std::endl ;
    */
	stream << "\n\t\t\tDatabases Details : \n" <<std::endl;
    std::map<std::string, MSSQLDBMetaData>::iterator dbMapIter = this->m_dbsMap.begin() ;
    while( dbMapIter != this->m_dbsMap.end() )
    {
        stream << "\t\t\t\tName:" << dbMapIter->first <<std::endl ;
        std::list<std::string>::iterator volListIter = dbMapIter->second.m_volumes.begin();
        stream << "\t\t\t\tVolumes:";
        
		while( volListIter != dbMapIter->second.m_volumes.end() )
		{
			stream << "\n\t\t\t\t\t" << *volListIter ;
			volListIter++ ;
		}

        stream << "\n\t\t\t\tDatabase Files:";
        std::list<std::string>::iterator dbFilesIter = dbMapIter->second.m_dbFiles.begin();
		while( dbFilesIter != dbMapIter->second.m_dbFiles.end() )
		{
			stream << "\n\t\t\t\t\t" << *dbFilesIter ;
			dbFilesIter++ ;
		}
        stream << std::endl  << std::endl ;
        dbMapIter++ ;
    }		
    stream << "\n" ;
    stream << "\n\t\t\tConsolidated database Volumes List:\n" <<std::endl ;
	std::list<std::string>::iterator volIter = m_volumesList.begin() ;
	while( volIter != m_volumesList.end() )
	{
        stream << "\t\t\t\t" << *volIter << std::endl;
		volIter++ ;
	}
    std::list<ValidatorPtr>::iterator ruleiter = list.begin() ;
    stream << "\n\n\t\t\tMicrosoft SQL Server Instance Pre-Requisites Validation Results" << std::endl <<std::endl ;
	while (ruleiter!= list.end() )
	{
		std::string RuleId = Validator::getInmRuleIdStringFromID((*ruleiter)->getRuleId() );
        stream << "\t\t\t\tRule Name\t\t:" << RuleId << "\n";
        stream << "\t\t\t\tResult\t\t\t:" << (*ruleiter)->statusToString() << std::endl ;
		ruleiter++ ;
	}

	stream << std::endl << std::endl ;
	return stream.str() ;
}