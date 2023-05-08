#include "portablehelpers.h"
#include "inmfilebackupmgr.h"
#include "inmstrcmp.h"
#include "logger.h"


InmFileBackupManager::InmFileBackupManager(const std::string &backupsuffix, const std::string &backupdir):
 m_BackupSuffix(backupsuffix),
 m_BackupDir(backupdir)
{
    if (m_BackupDir.size() && (m_BackupDir[m_BackupDir.length() - 1] != ACE_DIRECTORY_SEPARATOR_CHAR_A))
    {
        m_BackupDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    }
}


bool InmFileBackupManager::Backup(const std::string &item, bool &doesitemexist)
{
    std::string filetobackup = m_BackupDir + item;
    std::string backup = filetobackup + m_BackupSuffix;
    std::string errmsg;

    bool backedup = true;
    doesitemexist = true;
	E_INM_TRISTATE st = DoesFileExist(filetobackup, errmsg);
	if (E_INM_TRISTATE_FLOATING == st)
	{
		backedup = false;
	}
	else if (E_INM_TRISTATE_TRUE == st)
    {
        backedup = InmCopyTextFile(filetobackup, backup, true, errmsg) && SyncFile(backup, errmsg);
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "backup of file %s cannot be made as it does not exist\n", filetobackup.c_str());
        doesitemexist = false;
    }

    if (backedup)
    {
        /* this is to destroy the backup */
        m_BackupFiles.insert(std::make_pair(backup, doesitemexist ? E_DONTCARE : E_DOESNOTEXIST));
    }
    else
    {
        m_ErrMsg = "Backup of file ";
        m_ErrMsg += filetobackup; 
        m_ErrMsg += " failed with error: ";
        m_ErrMsg += errmsg;
    }

    return backedup;
}


bool InmFileBackupManager::Restore(const std::string &item)
{
    std::string filetorestore = m_BackupDir + item;
    std::string backup = filetorestore + m_BackupSuffix;
    std::string errmsg;
    bool brestored;

    
    /* If we collected backup file earlier */
    FilesIter_t it = m_BackupFiles.find(backup);
    if (it != m_BackupFiles.end())
    {
        brestored = InmCopyTextFile(backup, filetorestore, true, errmsg) &&
                    SyncFile(filetorestore, errmsg);
    }
    else
    {
        /* If there is no backup, then
         * the filetorestore must not have existed
         * when it was updated since for 
         * backup, we do fsync; This covers 
         * case where in before fsync if we 
         * crash, then entry should not have 
         * been present */
        /* remove the file to restore only if it exists;
         * If no backup exists, and the file to restore itself did not
         * exist, then nothing to do */
		E_INM_TRISTATE st = DoesFileExist(filetorestore, errmsg);
		if (E_INM_TRISTATE_FLOATING == st)
			brestored = false;
		else if (E_INM_TRISTATE_TRUE == st)
			brestored = InmRenameAndRemoveFile(filetorestore, errmsg);
		else
			brestored = true;

		if (brestored)
        {
            DebugPrintf(SV_LOG_DEBUG, "Backup of file %s does not exist. "
                                      "Hence removed it, if it had been " 
                                      "written newly while doing transaction, else nothing to do.\n", filetorestore.c_str());
        }
    }

    if (brestored)
    {
        if (it != m_BackupFiles.end())
        {
            it->second = E_RESTORED;
        }
        else
        {
            /* should this occur: yes in case of backup 
             * not existing */
        }
    }
    else
    {
        m_ErrMsg = "restore of file ";
        m_ErrMsg += filetorestore; 
        m_ErrMsg += " failed with error: ";
        m_ErrMsg += errmsg;
    }

    return brestored;
}


bool InmFileBackupManager::DestroyBackup(void)
{
    bool bdestroyed = m_BackupFiles.empty();

    std::string errmsg;
    for (ConstFilesIter_t it = m_BackupFiles.begin(); it != m_BackupFiles.end(); it++)
    {
        if (E_DOESNOTEXIST == it->second)
        {
            bdestroyed = true;
            continue;
        }

        const std::string &backup = it->first;
        /* TODO: base assumption is removal of files
         * do not fail; If this fails, should we 
         * stop here asking from manual delete of
         * backup files until quit requested, so that
         * this does not interfere with the next
         * time transaction ? But for now assumption 
         * is that remove succeeds */
        bdestroyed = InmRenameAndRemoveFile(backup, errmsg);
        if (!bdestroyed)
        {
            break;
        }
    }

    if (bdestroyed)
    {
        m_BackupFiles.clear();
    }
    else
    {
        m_ErrMsg = "destroying backup files failed with ";
        m_ErrMsg += errmsg;
    }
  
    return bdestroyed;
}


bool InmFileBackupManager::RestoreRemaining(void)
{
    bool brestored = m_BackupFiles.empty();
    std::string::size_type idx;
    std::string errmsg;

    for (ConstFilesIter_t it = m_BackupFiles.begin(); it != m_BackupFiles.end(); it++)
    {
        if (it->second == E_RESTORED)
        {
            brestored = true;
            continue;
        }

        const std::string &backup = it->first;
        idx = backup.rfind(m_BackupSuffix);
        if (std::string::npos != idx)
        {
            std::string item = backup.substr(0, idx);
            brestored = (InmCopyTextFile(backup, item, true, errmsg) && SyncFile(item, errmsg));
            if (!brestored)
            {
                m_ErrMsg = "restoration from ";
                m_ErrMsg += backup;
                m_ErrMsg += " failed with error: ";
                m_ErrMsg += errmsg;
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "restored item %s from remaining backups\n", item.c_str());
            }
        }
        else
        {
            m_ErrMsg = "restoration failed as ";
            m_ErrMsg = "backup file ";
            m_ErrMsg += backup;
            m_ErrMsg += " does not end with ";
            m_ErrMsg += m_BackupSuffix;
            brestored = false;
            break;
        }
    }

    return brestored;
}


bool InmFileBackupManager::GetBackupFiles(void)
{
    bool bformed = true;
    ACE_DIR *dirp;
    std::string fullbackupfilename;
	std::string dname;

    if (dirp = sv_opendir(m_BackupDir.c_str()))
    {
        struct ACE_DIRENT *dp;

        while (dp = ACE_OS::readdir(dirp))
        {
			dname = ACE_TEXT_ALWAYS_CHAR(dp->d_name);
            if (EndsWith(dname, m_BackupSuffix))
            {
                fullbackupfilename = m_BackupDir;
                fullbackupfilename += dname;
                m_BackupFiles.insert(std::make_pair(fullbackupfilename, E_DONTCARE));
            }
        }
        ACE_OS::closedir(dirp);
    }
    else
    {
        bformed = false; /* should this be left as true ? */
        m_ErrMsg = "Failed to open directory ";
        m_ErrMsg += m_BackupDir;
        m_ErrMsg += " for getting backed up files";
    }

    return bformed;
}


std::string InmFileBackupManager::GetErrorMsg(void)
{
    return m_ErrMsg;
}


bool InmFileBackupManager::Reset(void)
{
    m_BackupFiles.clear();
    m_ErrMsg.clear();

    return true;
}

