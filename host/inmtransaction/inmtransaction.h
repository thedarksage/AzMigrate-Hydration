#ifndef INMTRANSACTION_H
#define INMTRANSACTION_H

#include <string>

class InmTransaction
{
public:
    class BackupManager
    {
    public:
        virtual bool Backup(const std::string &item, bool &doesitemexist) = 0;    /* called inside AddItemToUpdate */
        virtual bool Restore(const std::string &item) = 0;   /* called for items updated while rolling back */
        virtual bool DestroyBackup(void) = 0;                /* called after successful transaction completion and rollback completion */
        virtual bool RestoreRemaining(void) = 0;             /* called for restoring items from backup that are not recognized as 
                                                              * participated in previous transaction, but had their backups */
        virtual std::string GetErrorMsg(void) = 0;
        virtual bool Reset(void) = 0;                        /* has to be provided by backup manager since this is called from
                                                              * Begin method of inmtransaction */
        virtual ~BackupManager(){}
    };

    virtual bool Begin(const std::string &name) = 0;
    virtual bool AddItemToUpdate(const std::string &item) = 0;
    virtual bool SetItemAsUpdated(const std::string &item) = 0;
    virtual bool End(void) = 0;
    virtual bool Rollback(void) = 0;                              /* assuming transaction is global */
    virtual std::string GetErrorMsg(void) = 0;
    virtual bool Reset(void) = 0;

    virtual ~InmTransaction() {}
};

#endif /* INMTRANSACTION_H */
