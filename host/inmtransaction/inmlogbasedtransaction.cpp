#include <algorithm>
#include <utility>
#include <cstring>
#include "inmlogbasedtransaction.h"
#include "inmlogbasedtransactiondefs.h"
#include "portablehelpers.h"
#include "logger.h"


InmLogBasedTransaction::InmLogBasedTransaction(const std::string &logname, BackupManager *pbackupmanager)
: m_Log(logname),
  m_pBackupManager(pbackupmanager),
  m_FpLog(0)
{
}


InmLogBasedTransaction::~InmLogBasedTransaction()
{
    /* TODO: not checking the return value since
     * this is best effort as at last */
    InmFclose(&m_FpLog, m_Log, m_ErrMsg);
}


bool InmLogBasedTransaction::Begin(const std::string &name)
{
    bool begined = false;

    if (Reset())
    {
        DebugPrintf(SV_LOG_DEBUG, "reset of previous transaction succeeded\n");
        m_Name = name;

		std::string suberrmsg;
		E_INM_TRISTATE st = DoesFileExist(m_Log, suberrmsg);
		if (E_INM_TRISTATE_FLOATING == st)
		{
			m_ErrMsg = "Failed to find existence of transaction log file ";
			m_ErrMsg += m_Log;
			m_ErrMsg += " with error: ";
			m_ErrMsg += suberrmsg;
		}
        else if (E_INM_TRISTATE_TRUE == st)
        {
            m_ErrMsg = "Transaction log file " + m_Log + " is already present. First rollback previous transaction.";
        }
        else
        {
            begined = RecordName() && Log(std::string(BEGIN) + "\n");
            if (begined)
            {
                DebugPrintf(SV_LOG_DEBUG, "begun transaction %s\n", m_Name.c_str());
            }
        }
    }
    else
    {
        m_ErrMsg = "Before beginning transaction, earlier transaction reset failed with: " + m_ErrMsg;
    }

    return begined;
}


bool InmLogBasedTransaction::End(void)
{
    bool bended = Log(std::string(END) + "\n") && CommitTransaction();
    if (bended)
    {
        DebugPrintf(SV_LOG_DEBUG, "ended transaction %s\n", m_Name.c_str());
    }

    return bended;
}


bool InmLogBasedTransaction::CommitTransaction(void)
{
    return InmFclose(&m_FpLog, m_Log, m_ErrMsg) && 
           SyncFile(m_Log, m_ErrMsg) &&
           DestroyBackup() &&
           InmRenameAndRemoveFile(m_Log, m_ErrMsg);
}


bool InmLogBasedTransaction::DestroyBackup(void)
{
    bool bdestroyed =  m_pBackupManager->DestroyBackup();
    if (!bdestroyed)
    {
        m_ErrMsg = m_pBackupManager->GetErrorMsg();
    }

    return bdestroyed;
}


bool InmLogBasedTransaction::RecordName(void)
{
    return OpenLog(E_FRESHTRANSACTION) && Log(m_Name + "\n");
}


bool InmLogBasedTransaction::OpenLog(const LogOpenFor_t &logopenfor)
{
    return InmFclose(&m_FpLog, m_Log, m_ErrMsg) && InmFopen(&m_FpLog, m_Log, OPENMODES[logopenfor], m_ErrMsg);
}


bool InmLogBasedTransaction::Log(const std::string &text)
{
    bool blogged = false;

    if (text.size() <= LOGLINELENGTH)
    {
        std::string errmsg;
        blogged = InmFprintf(m_FpLog, m_Log, errmsg, false, 0, "%s", text.c_str());
        if (!blogged)
        {
            m_ErrMsg = "Writing text: \"";
            m_ErrMsg += text;
            m_ErrMsg += "\" in transaction log failed with: ";
            m_ErrMsg += errmsg;
        }
    }
    else
    {
        m_ErrMsg = "text: \"";
        m_ErrMsg += text;
        m_ErrMsg += "\" too big to log in transaction log.";
    }

    return blogged;
}


std::string InmLogBasedTransaction::GetErrorMsg(void)
{
    return m_ErrMsg;
}


bool InmLogBasedTransaction::AddItemToUpdate(const std::string &item)
{
    bool badded = false;

    bool doesitemexist;
    if (m_pBackupManager->Backup(item, doesitemexist))
    {
        std::string logtext(1,  ITEMDELIMITER);
        logtext += item;
        logtext += ITEMDELIMITER;
        logtext += ' ';
        logtext += PENDINGSTATUS;
        logtext += '\n';
        badded = Log(logtext) && RecordItemPosition(item);
        if (badded && (!doesitemexist))
        {
            badded = SyncFile(m_Log, m_ErrMsg);
        }
    }
    else
    {
        m_ErrMsg = m_pBackupManager->GetErrorMsg();
    }

    if (badded)
    {
        DebugPrintf(SV_LOG_DEBUG, "For transaction %s, added item to update: %s\n", m_Name.c_str(), item.c_str());
    }
     
    return badded;
}


bool InmLogBasedTransaction::RecordItemPosition(const std::string &item)
{
    bool brecorded = false;
    long pos = ftell(m_FpLog);
   
    if (-1 == pos)
    {
        m_ErrMsg = "To record update position of item " + item + ", in log file " + m_Log + ", ftell failed with error: " + strerror(errno);
    }
    else if ((pos - STATUSBACKPOS) > 0)
    {
        m_ItemPos.insert(std::make_pair(item, pos - STATUSBACKPOS));
        brecorded = true;
    }
    else
    {
        m_ErrMsg = "Recording update position of item " + item + ", in log file " + m_Log + ", failed";
    }

    return brecorded;
}


bool InmLogBasedTransaction::SetItemUpdateStatus(const std::string &item, const char &c)
{
    bool bupdated = false;

    ConstItemPosIter_t it = m_ItemPos.find(item);
    if (it != m_ItemPos.end())
    {
        std::string errmsg;
        bupdated = InmFprintf(m_FpLog, m_Log, errmsg, true, it->second, "%c", c);
        if (!bupdated)
        {
            m_ErrMsg = " setting item ";
            m_ErrMsg += item;
            m_ErrMsg += " as ";
            m_ErrMsg += c;
            m_ErrMsg += " failed with error: ";
            m_ErrMsg += errmsg;
        }
    }
    else
    {
        m_ErrMsg = "Item " + item + " never logged but updation of status is asked for" + " in log " + m_Log;
    }

    return bupdated;
}


bool InmLogBasedTransaction::Rollback(void)
{
    RollbackActions_t actions[] = {&InmLogBasedTransaction::RollbackWhenCannotFindLogExistence,
		                           &InmLogBasedTransaction::RollbackWhenUnknownState, /* TODO: write to error msg that please 
                                                                                       * rollback manually, and delete transaction log
                                                                                       * and backup if user can. Here also above 
                                                                                       * procedure seems to work because of the way
                                                                                       * we sequenced this whole operation. 
                                                                                       * TODO: we can automate this rather than 
                                                                                       * asking manual intervention */
                                   &InmLogBasedTransaction::RollbackWhenLogNotFound,
                                   &InmLogBasedTransaction::RollbackWhenNoTransactionName,
                                   &InmLogBasedTransaction::RollbackWhenUptoTransactionName,
                                   &InmLogBasedTransaction::RollbackWhenUptoBegin,
                                   &InmLogBasedTransaction::RollbackWhenNoEnd,
                                   &InmLogBasedTransaction::RollbackWhenEnd};
    State_t st = GetLogStatus();
    DebugPrintf(SV_LOG_DEBUG, "state of transaction log %s is \"%s\" for transaction %s\n", 
                              m_Log.c_str(), STRLOGSTATUS[st], m_Name.c_str());
    RollbackActions_t p = actions[st];
    bool brolledback = (this->*p)();

    if (brolledback)
    {
        DebugPrintf(SV_LOG_DEBUG, "successfully rolledback transaction %s\n", m_Name.c_str());
    }

    return brolledback;
}


InmLogBasedTransaction::State_t InmLogBasedTransaction::GetLogStatus(void)
{
	State_t logstate;

	E_INM_TRISTATE existencestate = DoesFileExist(m_Log, m_ErrMsg);
	if (E_INM_TRISTATE_FLOATING == existencestate)
		logstate = E_CANNOTFINDLOGEXISTENCE;
	else if (E_INM_TRISTATE_TRUE == existencestate)
		logstate = GetPreviousTransactionState();
	else
		logstate = E_LOGNOTFOUND;

    return logstate;
} 


InmLogBasedTransaction::State_t InmLogBasedTransaction::GetPreviousTransactionState(void)
{
    State_t st = E_UNKNOWN;

    if (OpenLog(E_ROLLBACK))
    {
        char line[LOGLINELENGTH + 1];

        /* able to use fgets blindly as we do not accept 
         * lines greater than LOGLINELENGTH including '\n' */
        /* TODO: inm getline function that puts or not '\n' 
         * on reading line */
        if (fgets(line, sizeof line, m_FpLog))
        {
            TrimChar(line, '\n');
            m_Name = line;
            st = E_UPTOTRANSACTIONNAME;
        }
        else if (feof(m_FpLog))
        {
            st = E_NOTRANSACTIONNAME;
        }
        else
        {
            st = UnknownStatus(std::string("Reading previous transaction name failed with error: ") + strerror(errno));
        }

        if (E_UPTOTRANSACTIONNAME == st)
        {
            if (fgets(line, sizeof line, m_FpLog))
            {
                TrimChar(line, '\n');
                st = (0 == strcmp(BEGIN, line)) ? 
                     E_UPTOBEGIN : 
                     UnknownStatus(std::string("transaction log line: ") + line + " does not match " + BEGIN + ", for transaction " + m_Name);
            }
            else if (!feof(m_FpLog))
            {
                st = UnknownStatus(std::string("Reading previous transaction begin failed with error: ") + strerror(errno)
                                   + ", for transaction " + m_Name);
            }
        }

        if (E_UPTOBEGIN == st)
        {
            st = RecordItemUpdateStatusesAndEnd(line, sizeof line);
        }
    }
    
    return st;
}


InmLogBasedTransaction::State_t InmLogBasedTransaction::UnknownStatus(const std::string &errtxt)
{
    m_ErrMsg = errtxt;
    return E_UNKNOWN;
}


InmLogBasedTransaction::State_t InmLogBasedTransaction::RecordItemUpdateStatusesAndEnd(char *line, const size_t &linesize)
{
    State_t st = E_UPTOBEGIN;

    while (!feof(m_FpLog))
    {
        if (fgets(line, linesize, m_FpLog))
        {
            TrimChar(line, '\n');
            st = (0 == strcmp(END, line)) ? E_UPTOEND : RecordItemUpdateStatus(line);
            if ((E_UPTOEND == st) || (E_UNKNOWN == st))
            {
                break;
            }
        }
        else if (!feof(m_FpLog))
        {
            st = UnknownStatus(std::string("Reading previous transaction items / end failed with error: ") + strerror(errno) 
                               + ", for transaction " + m_Name);
            break;
        }
    }

    return st;
} 


InmLogBasedTransaction::State_t InmLogBasedTransaction::RecordItemUpdateStatus(char *line)
{
    State_t st;

    size_t len = strlen(line);
    if (len && ((line[len-1] == PENDINGSTATUS) || (line[len-1] == UPDATEDSTATUS)))
    {
        if ((len-1) && (line[len-2] == ' '))
        {
            if ((len-2) && (line[len-3] == ITEMDELIMITER) && 
                (line[0] == ITEMDELIMITER))
            {
                line[len-3] = '\0';
                st = RecordItemPosition(line+1) ? E_NOEND : E_UNKNOWN;
            }
            else
            {
                st = UnknownStatus(std::string("delimiter(s) not found in item record: ") + line + ", for transaction name " + m_Name);
            }
        }
        else
        {
            st = UnknownStatus(std::string("invalid transaction entry in item record: ") + line + ", for transaction name " + m_Name);
        }
    }
    else
    {
        st = UnknownStatus(std::string("invalid transaction status in item record: ") + (len ? line : "") + ", for transaction name " + m_Name);
    }

    return st;
}


bool InmLogBasedTransaction::RollbackWhenCannotFindLogExistence(void)
{
    DebugPrintf(SV_LOG_ERROR, "Transaction rollback failed, because cannot find existence of log file %s, with error %s\n", 
		                      m_Log.c_str(), m_ErrMsg.c_str());

    return false;
}


bool InmLogBasedTransaction::RollbackWhenUnknownState(void)
{
    DebugPrintf(SV_LOG_ERROR, "Transaction log file %s, for transaction name %s, is improper with error: %s. "
                              "But still items will be restored from backup if any.\n",
                              m_Log.c_str(), m_Name.c_str(), m_ErrMsg.c_str());

    return RestoreRemaining() && CommitTransaction();
}


bool InmLogBasedTransaction::RollbackWhenLogNotFound(void)
{
    return RestoreRemaining() && DestroyBackup();
}


bool InmLogBasedTransaction::RollbackWhenNoTransactionName(void)
{
    return RestoreRemaining() && CommitTransaction();
}


bool InmLogBasedTransaction::RollbackWhenUptoTransactionName(void)
{
    return RestoreRemaining() && CommitTransaction();
}


bool InmLogBasedTransaction::RollbackWhenUptoBegin(void)
{
    return RollbackWhenNoItems();
}


bool InmLogBasedTransaction::RollbackWhenNoEnd(void)
{
    bool brolledback = false;
    std::string errmsg;

    for (ConstItemPosIter_t it = m_ItemPos.begin(); it != m_ItemPos.end(); it++)
    {
        if (m_pBackupManager->Restore(it->first))
        {
            brolledback = InmFprintf(m_FpLog, m_Log, errmsg, true, it->second, "%c", PENDINGSTATUS);
        }
        else
        {
            errmsg = m_pBackupManager->GetErrorMsg();
            brolledback = false;
        }

        if (!brolledback)
        {
            m_ErrMsg = "For transaction ";
            m_ErrMsg += m_Name;
            m_ErrMsg += ' ';
            m_ErrMsg += errmsg;
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "For transaction name %s, restored item %s\n", 
                                      m_Name.c_str(), it->first.c_str());
        }
    }

    if (brolledback)
    {
        brolledback = RollbackWhenNoItems();
    }

    return brolledback;
}


bool InmLogBasedTransaction::RollbackWhenEnd(void)
{
    return CommitTransaction();
}


bool InmLogBasedTransaction::RollbackWhenNoItems(void)
{
    /* RestoreRemaining should not restore what has 
     * been already restored earlier via Restore 
     * interface */
    return RestoreRemaining() && End();
}


bool InmLogBasedTransaction::SetItemAsNotUpdated(const std::string &item)
{
    return SetItemUpdateStatus(item, PENDINGSTATUS);
}


bool InmLogBasedTransaction::SetItemAsUpdated(const std::string &item)
{
    bool bset = SetItemUpdateStatus(item, UPDATEDSTATUS);

    if (bset)
    {
        DebugPrintf(SV_LOG_DEBUG, "item %s status set as updated in transaction %s\n", item.c_str(), m_Name.c_str());
    }

    return bset;
}


bool InmLogBasedTransaction::RestoreRemaining(void)
{
    bool brestored = m_pBackupManager->RestoreRemaining();
    if (!brestored)
    {
        m_ErrMsg = m_pBackupManager->GetErrorMsg();
    }

    return brestored;
}


bool InmLogBasedTransaction::Reset(void)
{
    bool breset = false;

    if (m_pBackupManager->Reset())
    {
        breset = InmFclose(&m_FpLog, m_Log, m_ErrMsg);
        if (breset)
        {
            m_Name.clear();
            m_ErrMsg.clear();
            m_ItemPos.clear();
        }
    }
    else
    {
        m_ErrMsg = "Reset of backup manager failed with ";
        m_ErrMsg += m_pBackupManager->GetErrorMsg();
    }

    return breset;
}

