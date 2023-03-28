#ifndef INM_FILEBACKUPMGR_H
#define INM_FILEBACKUPMGR_H

#include "inmtransaction.h"
#include <set>
#include <iterator>

class InmFileBackupManager : public InmTransaction::BackupManager
{
public:
    InmFileBackupManager(const std::string &backupsuffix, const std::string &backupdir);
    bool Backup(const std::string &item, bool &doesitemexist);
    bool Restore(const std::string &item);
    bool DestroyBackup(void);
    bool RestoreRemaining(void);
    std::string GetErrorMsg(void);
    bool Reset(void);

public:
    bool GetBackupFiles(void);

private:
    typedef enum FileState
    {
        E_DONTCARE,
        E_RESTORED,
        E_DOESNOTEXIST

    } FileState_t;

    typedef std::map<std::string, FileState_t> Files_t;
    typedef Files_t::const_iterator ConstFilesIter_t;
    typedef Files_t::iterator FilesIter_t;
    Files_t m_BackupFiles;
    std::string m_BackupSuffix;
    std::string m_ErrMsg;
    std::string m_BackupDir;
};

#endif /* INM_FILEBACKUPMGR_H */
