/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : transportstream.cpp
 *
 * Description: 
 */
#include <string>
#include <cassert>

#include "portable.h"
#include "error.h"

#include "cxtransport.h"
#include "transportstream.h"

steady_clock::time_point TransportStream::s_LastTransportErrLogTime = steady_clock::time_point::min();
uint64_t TransportStream::s_TransportFailCnt = 0;
uint64_t TransportStream::s_TransportSuccCnt = 0;

/*
 * FUNCTION NAME :  Abort
 *
 * DESCRIPTION : aborts an existing stream
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */


int TransportStream::Abort(char const* pData)
{
    int iStatus = SV_FAILURE;
    if (0 != m_cxTransport.get() )
    {
        if (!m_cxTransport->abortRequest(pData)) {
            SetState(STRM_BAD);
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR,"%s: Failed to abort request. %s\n", FUNCTION_NAME, m_cxTransport->status());
        } else {
            iStatus = SV_SUCCESS;
            SetState(STRM_GOOD);
        }
    }
    else
    {
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
        DebugPrintf(SV_LOG_ERROR,"%s: Failed to abort request: stream is not open\n", FUNCTION_NAME);
    }
        
    return iStatus;
}

/*
 * FUNCTION NAME :  Open  
 *
 * DESCRIPTION :    Opens the transportstream for read / write operations    
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
 
int TransportStream::Open(STREAM_MODE Mode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);
    int iStatus = SV_FAILURE;

    m_Mode = Mode;
    if ( !ModeOn(Mode_RW, m_Mode) &&  !ModeOn(Mode_Read, m_Mode) && !ModeOn(Mode_Write, m_Mode) )
    {
        DebugPrintf(SV_LOG_ERROR,"%s: Invalid Open mode %d.\n", FUNCTION_NAME, Mode);
        DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n", FUNCTION_NAME);
        SetState(STRM_BAD);
        return iStatus;
    }


    if (ModeOn(Mode_Asynchronous, m_Mode)) 
    {
        DebugPrintf(SV_LOG_WARNING,"%s Currently no support for async data transfers.\n", FUNCTION_NAME);
    }

    m_secure = ModeOn(Mode_Secure, m_Mode);

    try { 
        m_cxTransport.reset(new CxTransport(m_protocol,
                                            m_transportSettings,
                                            m_secure));
        SetState(STRM_GOOD);
        iStatus = SV_SUCCESS;
    } catch (std::exception const& e) {
        DebugPrintf(SV_LOG_ERROR,"%s create CxTransport failed: %s\n ", FUNCTION_NAME, e.what());
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    } catch (...) {
        DebugPrintf(SV_LOG_ERROR,"%s create CxTransport failed: unkown error\n ", FUNCTION_NAME);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }   

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

/*
 * FUNCTION NAME :  
 *
 * DESCRIPTION :    Closes the open transport session and resets the state.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :
 *
 */
 
int TransportStream::Close()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
    if ( 0 != m_cxTransport.get() )
    {
        m_cxTransport.reset(static_cast<CxTransport*>(0));
        this->Clear();
        SetState(STRM_CLEAR);
        iStatus = SV_SUCCESS;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return iStatus;
}

/*
 * FUNCTION NAME :  Rename  
 *
 * DESCRIPTION :    Given old filename renames it to a new file.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if transport rename succeeds else SV_FAILURE
 *
 */
 
int TransportStream::Rename(const std::string& sOldFileName, const std::string& sNewFileName, COMPRESS_MODE compressMode, std::string const& finalPaths)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;

    if ( 0 != m_cxTransport.get() )
    {
        if ( !sOldFileName.empty() && !sNewFileName.empty() )
        {
            std::map<std::string, std::string> headers;

            if (m_bAppendSystemTimeUtcOnRename)
            {
                static const std::string HTTP_PARAM_TAG_APPENDMTIMEUTC("appendmtimeutc"); ///< http tag for appendmtimeutc=<0|1> (0: no 1: yes)
                static const std::string _1("1");

                headers.insert(std::pair<std::string, std::string>(HTTP_PARAM_TAG_APPENDMTIMEUTC, _1));
            }

            if ( m_cxTransport->renameFile(sOldFileName, sNewFileName, compressMode, headers, finalPaths) )
            {
                SetState(STRM_GOOD);
                iStatus = SV_SUCCESS;
            }
            else
            {
                SetState(STRM_BAD);
                iStatus = SV_FAILURE;
            }

            if (LogTransportFailures(iStatus))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to rename file with error %s. Old Filename: %s. New Filename: %s. Transport failures = %d, success = %d since %d secs.\n",
                    FUNCTION_NAME, m_cxTransport->status(), sOldFileName.c_str(), sNewFileName.c_str(),  s_TransportFailCnt, s_TransportSuccCnt, m_transportErrorLogInterval);
                s_TransportFailCnt = s_TransportSuccCnt = 0;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"%s: Failed to rename file. Invalid name specified. Old Filename: %s. New Filename: %s.\n", FUNCTION_NAME, sOldFileName.c_str(),sNewFileName.c_str());
            iStatus = SV_FAILURE;
        }
    }       
    else
    {
        DebugPrintf(SV_LOG_ERROR,"%s: Failed to rename file. Stream is not open for rename operation.\n", FUNCTION_NAME);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return iStatus;
}

/*
 * FUNCTION NAME :  Write 
 *
 * DESCRIPTION :    Uploads in-memory data to the remote host. The target file must be specified as the absolute path.
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if transport upload succeeds else SV_FAILURE
 *
 */
 
int TransportStream::Write(const char* sData, unsigned long int uliDataLen, const std::string& sDestination, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs, bool bIsFull)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;

    if ( 0 != m_cxTransport.get() )
    {
        if ( ModeOn(Mode_Write, m_Mode) )
        {
            if ( !sDestination.empty())
            {
                if (!m_cxTransport->putFile(sDestination, uliDataLen, sData, bMoreData, compressMode, m_headers, createDirs))
                {
                    SetState(STRM_BAD);
                    iStatus = SV_FAILURE;
                }
                else
                {
                    SetState(STRM_GOOD);
                    iStatus = SV_SUCCESS;
                }

                if (LogTransportFailures(iStatus))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to send data with error %s. Transport failures = %d, success = %d since %d secs.\n",
                        FUNCTION_NAME, m_cxTransport->status(), s_TransportFailCnt, s_TransportSuccCnt, m_transportErrorLogInterval);
                    s_TransportFailCnt = s_TransportSuccCnt = 0;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"%s: Failed to write data. Invalid destination\n", FUNCTION_NAME);
                SetState(STRM_BAD);
                iStatus = SV_FAILURE;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"%s: Failed to write data. Stream not opened in Write mode.\n", FUNCTION_NAME);
            SetState(STRM_BAD);
            iStatus = SV_FAILURE;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"%s: Failed to write data. Stream is not open for write operation.\n", FUNCTION_NAME);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return iStatus;
}

/*
 * FUNCTION NAME :  Write  
 *
 * DESCRIPTION :    Uploads the local file to the remote host. The local and remote file names must be specified as the absolute path
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if transport upload succeeds else SV_FAILURE
 *
 */

int TransportStream::Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode, bool createDirs)
{
    return Write(localFile, targetFile, compressMode, std::string(), std::string(), createDirs);
}

/*
 * FUNCTION NAME :  Write  
 *
 * DESCRIPTION :    Uploads the local file to the remote host. The local and remote file names must be specified as the absolute path
 *                  optionally rename the file after the write completes
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if transport upload succeeds else SV_FAILURE
 *
 */
 
int TransportStream::Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode, const std::string& renameFile, std::string const& finalPaths, bool createDirs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_SUCCESS;

    if ( 0 != m_cxTransport.get() )
    {
        if ( ModeOn(Mode_Write, m_Mode) )
        {
            if ( !localFile.empty() && !targetFile.empty())
            {
                if (!m_cxTransport->putFile(targetFile, localFile, compressMode, m_headers, createDirs)) {
                    SetState(STRM_BAD);
                    iStatus = SV_FAILURE;
                }
                else
                {
                    if (!renameFile.empty() )
                    {
                        if (!m_cxTransport->renameFile(targetFile, renameFile, compressMode, finalPaths)) {
                            DebugPrintf(SV_LOG_ERROR,"%s: Failed to send data. %s\n", FUNCTION_NAME, m_cxTransport->status());
                            SetState(STRM_BAD);
                            iStatus = SV_FAILURE;
                        }
                        else
                        {
                            SetState(STRM_GOOD);
                            iStatus = SV_SUCCESS;
                        }
                    } 
                    else
                    {
                        SetState(STRM_GOOD);
                        iStatus = SV_SUCCESS;
                    }
                }

                if (LogTransportFailures(iStatus))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to send file with error %s. Transport failures = %d, success = %d since %d secs.\n",
                        FUNCTION_NAME, m_cxTransport->status(), s_TransportFailCnt, s_TransportSuccCnt, m_transportErrorLogInterval);
                    s_TransportFailCnt = s_TransportSuccCnt = 0;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"%s: Failed to write data. Invalid file name specified. Source File: %s. Target File: %s.\n", FUNCTION_NAME, localFile.c_str(),targetFile.c_str());
                iStatus = SV_FAILURE;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"%s: Failed to write data. Stream not opened in Write mode.\n", FUNCTION_NAME);
            SetState(STRM_BAD);
            iStatus = SV_FAILURE;
        }
    }       
    else
    {
        DebugPrintf(SV_LOG_ERROR,"%s: Failed to write data. Stream is not open for write operation.\n", FUNCTION_NAME);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return iStatus;
}

/*
 * FUNCTION NAME :  DeleteFile  
 *
 * DESCRIPTION :    Deletes the remote file.  The remote file name must be specified as the absolute path 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if transport delete succeeds else SV_FAILURE
 *
 */        
int TransportStream::DeleteFile(std::string const& names, int mode)
{
    return this->DeleteFile(names, std::string(), mode);
}

int TransportStream::DeleteFile(std::string const& names, std::string const& fileSpec, int mode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int iStatus = SV_FAILURE;
    if ( 0 != m_cxTransport.get() )
    {
        boost::tribool tb = m_cxTransport->deleteFile(names, fileSpec, mode);
        if ( tb )
        {
            SetState(STRM_GOOD);
            iStatus = SV_SUCCESS;
            DebugPrintf(SV_LOG_WARNING,"%s: Deleted file %s.\n", FUNCTION_NAME, names.c_str());
        }
        else if (!tb)
        {
            SetState(STRM_BAD);
            iStatus = SV_FAILURE;
        }
        else
        {
            SetState(STRM_GOOD);
            iStatus = SV_SUCCESS;
            DebugPrintf(SV_LOG_WARNING, "%s: file %s is *not found*.\n", FUNCTION_NAME, names.c_str());
        }

        if (LogTransportFailures(iStatus))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to delete file %s with error %s. Transport failures = %d, success = %d since %d secs.\n",
                FUNCTION_NAME, names.c_str(), m_cxTransport->status(), s_TransportFailCnt, s_TransportSuccCnt, m_transportErrorLogInterval);
            s_TransportFailCnt = s_TransportSuccCnt = 0;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"%s: Failed to delete remote file. Stream is not open.\n", FUNCTION_NAME);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    
    return iStatus;

}

/*
 * FUNCTION NAME :  DeleteFiles
 *
 * DESCRIPTION :    Deletes the list of files specified. All filenames must be the absolute path
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value :   returns SV_SUCCESS if transport delete succeeds else SV_FAILURE
 *
 */
 
int TransportStream::DeleteFiles(const ListString& DeleteFileList)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);
    SVERROR se = SVS_OK;

    int iStatus = SV_SUCCESS;
    if ( 0 != m_cxTransport.get() )
    {
        ListString::const_iterator iter(DeleteFileList.begin());
        ListString::const_iterator iterEnd(DeleteFileList.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            boost::tribool tb = m_cxTransport->deleteFile(*iter);
            if (tb)
            {
                SetState(STRM_GOOD);
                iStatus = SV_SUCCESS;
                DebugPrintf(SV_LOG_WARNING, "%s: Deleted file %s.\n", FUNCTION_NAME, (*iter).c_str());
            }
            else if (!tb)
            {
                SetState(STRM_BAD);
                iStatus = SV_FAILURE;
            }
            else
            {
                SetState(STRM_GOOD);
                iStatus = SV_SUCCESS;
                DebugPrintf(SV_LOG_WARNING, "%s: file %s *not found*.\n", FUNCTION_NAME, (*iter).c_str());
            }

            if (LogTransportFailures(iStatus))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to delete file %s with error %s. Transport failures = %d, success = %d since %d secs.\n",
                    FUNCTION_NAME, (*iter).c_str(), m_cxTransport->status(), s_TransportFailCnt, s_TransportSuccCnt, m_transportErrorLogInterval);
                s_TransportFailCnt = s_TransportSuccCnt = 0;
            }
        }
    } 
    else
    {
        DebugPrintf(SV_LOG_ERROR,"%s: Failed to delete remote files. Stream is not open.\n", FUNCTION_NAME);
        SetState(STRM_BAD);
        iStatus = SV_FAILURE;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return iStatus;
}           

bool TransportStream::NeedToDeletePreviousFile(void)
{
    return m_bNeedToDeletePrevFile;
}


void TransportStream::SetNeedToDeletePreviousFile(bool bneedtodeleteprevfile)
{
    m_bNeedToDeletePrevFile = bneedtodeleteprevfile;
}


bool TransportStream::heartbeat(bool forceSend)
{
    return m_cxTransport->heartbeat(forceSend);
}

/*
* FUNCTION NAME :  LogTransportFailures
*
* DESCRIPTION :    Sets s_LastTransportErrLogTime, s_TransportSuccCnt and s_TransportFailCnt
*
* INPUT PARAMETERS :  SV_SUCCESS or SV_FAILURE
*
* OUTPUT PARAMETERS :  NONE
*
* NOTES :
*
* return value : returns true if transport error log threshold is reached otherwise false
*
*/
bool TransportStream::LogTransportFailures(int iStatus)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    static steady_clock::duration logInterval = seconds(m_localConfigurator.getTransportErrorLogInterval());
    bool status = false;
    steady_clock::time_point currentTime = steady_clock::now();

    if (iStatus == SV_FAILURE)
    {
        s_TransportFailCnt++;
        if (currentTime >= s_LastTransportErrLogTime + logInterval)
        {
            m_transportErrorLogInterval = (currentTime - s_LastTransportErrLogTime).count() / (1000 * 1000 * 1000); // in secs
            s_LastTransportErrLogTime = currentTime;
            status = true;
        }
    }
    else
    {
        s_TransportSuccCnt++;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void TransportStream::SetDiskId(const std::string & diskId)
{
    m_headers[std::string(HTTP_PARAM_TAG_DISKID)] = diskId;
}

void TransportStream::SetFileType(int filetype)
{
    m_headers[std::string(HTTP_PARAM_TAG_FILETYPE)] = boost::lexical_cast<std::string>(filetype);
}

ResponseData TransportStream::GetResponseData() const
{
    return m_cxTransport->getResponseData();
}