#ifndef CONFSCHEMA_READER_WRITER 
#define CONFSCHEMA_READER_WRITER 

#include <boost/shared_ptr.hpp>
#include "confschema.h"
#include "confschemamgr.h"
#include "inmlogbasedtransaction.h"
#include "inmfilebackupmgr.h"
#include "InmsdkGlobals.h"

class ConfSchemaReaderWriter ;
typedef boost::shared_ptr<ConfSchemaReaderWriter> ConfSchemaReaderWriterPtr ;

class ConfSchemaReaderWriter
{
	std::string m_apiname ;
    static ConfSchemaReaderWriterPtr m_ConfSchemaReaderWriterPtr ;
    boost::shared_ptr<InmFileBackupManager> m_bkpmgr ;
    boost::shared_ptr<InmTransaction> m_pit ; 
    std::set<std::string> m_filenames ;
    ConfSchemaMgrPtr schemaMgrPtr ;
    ConfSchemaReaderWriter( const std::string& repoLocation, const std::string& repoLocationForHost ) ;
    std::string m_confSchemaPath ;
    void BeginTransaction(boost::shared_ptr<InmTransaction> pit) ;
    void EndTransaction(boost::shared_ptr<InmTransaction> pit) ;
    ConfSchema::Groups_t& getGroups(void) ;
	bool RetryRollback(const std::string& transactionLog) ;
public:
    void ResetTransactionLogLocation() ;
    void RollBackTransaction() ;
	static bool ActOnCorruptedGroups();
    static ConfSchemaReaderWriterPtr GetInstance() ;
	static void CreateInstance(const std::string& apiname, const std::string& repoLocation, const std::string& repositoryPathForHost, bool bProcessEvenWithoutlock) ;
    void Sync() ;
    ConfSchema::Group* getGroup(const std::string& grpname) ;
} ;

class TransactionException
{
    std::string m_whatStr ;
    INM_ERROR m_errCode ;
public:
    TransactionException(const std::string& str, INM_ERROR errCode ) ;
    const char* what() const ;
    INM_ERROR getError() const ;
} ;
#endif