#ifndef INMLOGBASEDTRANSACTION_H
#define INMLOGBASEDTRANSACTION_H

#include <string>
#include <cstdio>
#include <map>
#include "inmtransaction.h"

const char ITEMDELIMITER = '\"';
const char * const STRLOGOPENEDFOR[] = {"fresh transaction", "rollback transaction"};
const char * const BEGIN = "BEGIN";
const char * const END = "END";
const char PENDINGSTATUS = 'p';
const char UPDATEDSTATUS = 'u';
const std::string::size_type LOGLINELENGTH = 1024;
const char * const STRLOGSTATUS[] = {"cannot find existence",
	                                 "unknown", 
                                     "not present", 
                                     "no transaction name found", 
                                     "only transaction name found",
                                     "upto begin found",
                                     "no end found",
                                     "upto end found"};

class InmLogBasedTransaction : public InmTransaction
{
public:
    InmLogBasedTransaction(const std::string &logname, 
                           BackupManager *pbackupmanager);
    ~InmLogBasedTransaction();
    bool Begin(const std::string &name);
    bool AddItemToUpdate(const std::string &item);
    bool SetItemAsUpdated(const std::string &item);
    bool End(void);
    bool Rollback(void);
    std::string GetErrorMsg(void);
    bool Reset(void);

private:
    typedef enum State
    {
		E_CANNOTFINDLOGEXISTENCE,
        E_UNKNOWN,        

        E_LOGNOTFOUND,           /* Nothing to do; just restore from backup files if any ?
                                  * destroy the backup files if any since before system 
                                  * could flush file, it crashed */
        E_NOTRANSACTIONNAME,     /* same as above as system might have flushed part of log file */
        E_UPTOTRANSACTIONNAME,   /* same as above as system might have flushed part of log file */
        E_UPTOBEGIN,      /* Nothing to do; But restore from backup if any. As anything  
                           * written after BEGIN might not have been synched due to reasons 
                           * mentioned above */
        E_NOEND,          /* Rollback whether 'y' or 'n'. As even though file is updated, 
                           * and we changed 'n' to 'y' but this has not been synched. 
                           * Here also restore from backup if any since some entries 
                           * might might not synced; If we do not find backup while restoring, 
                           * state is irrecoverable ? curretly yes; Log file has to be synched 
                           * the moment we record END */
                           /* If the underlying hard disk has write caching enabled, 
                            * then the data may not really be on permanent storage when fsync() / fdatasync()
                            * return. Hence write caching must be disabled where backup and transaction log file 
                            * is being stored. Also fsync does not guarantee the directory entry is flushed to disk. 
                            * This utmost can result in log file being not found, but we cover it by
                            * restoring files from the available backup store */
        E_UPTOEND         /* Nothing to do regardless of updation state of sub transactions
                           * remove if any backup copies and remove the log file; */
                          /* restoration should make sure the config files restored back are synched and
                           * should make 'y' to 'n'. All backup files again should be deleted only 
                           * on restoring all files after the log is deleted */
    } State_t;

    typedef enum LogOpenFor
    {
        E_FRESHTRANSACTION,
        E_ROLLBACK
  
    } LogOpenFor_t;

    typedef std::map<std::string, long> ItemPos_t;
    typedef ItemPos_t::const_iterator ConstItemPosIter_t;
    typedef bool (InmLogBasedTransaction::*RollbackActions_t)(void);

    bool RecordName(void);
    bool OpenLog(const LogOpenFor_t &logopenfor);
    bool Log(const std::string &text);
    bool CommitTransaction(void);
    bool DestroyBackup(void);
    bool RecordItemPosition(const std::string &item);
    State_t GetLogStatus(void);
    State_t GetPreviousTransactionState(void);
    State_t UnknownStatus(const std::string &errtxt);
    State_t RecordItemUpdateStatusesAndEnd(char *line, const size_t &linesize);
    State_t RecordItemUpdateStatus(char *line);
	bool RollbackWhenCannotFindLogExistence(void);
    bool RollbackWhenUnknownState(void);
    bool RollbackWhenLogNotFound(void);
    bool RollbackWhenNoTransactionName(void);
    bool RollbackWhenUptoTransactionName(void);
    bool RollbackWhenUptoBegin(void);
    bool RollbackWhenNoEnd(void);
    bool RollbackWhenEnd(void);
    bool RollbackWhenNoItems(void);
    bool SetItemAsNotUpdated(const std::string &item);
    bool SetItemUpdateStatus(const std::string &item, const char &c);
    bool RestoreRemaining(void);

private:
    /* TODO: should this be long path enabled ? */
    std::string m_Log;
    BackupManager *m_pBackupManager;
    FILE *m_FpLog;
    std::string m_Name;
    std::string m_ErrMsg;
    ItemPos_t m_ItemPos;
};

#endif /* INMLOGBASEDTRANSACTION_H */
