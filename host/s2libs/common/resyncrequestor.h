/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : resyncrequestor.h
 *
 * Description: 
 */

#ifndef RESYNCREQUESTOR__H
#define RESYNCREQUESTOR__H

#include <string>
#include <map>
#include <iterator>

#include "svtypes.h"

#ifdef SV_WINDOWS
#define E_RESYNC_REQUIRED_REASON_FOR_RESIZE 0xB0014E34L
#else
#define E_RESYNC_REQUIRED_REASON_FOR_RESIZE 0x00000007
#endif

class ResyncRequiredError
{
public:
    typedef std::map<SV_ULONG, const char *> ErrorCodeToMsg_t;
    typedef ErrorCodeToMsg_t::const_iterator ConstErrorCodeToMsgIter_t;

    typedef enum eErrorCodes
    { 
        UNKNOWN = 0x200, 
        IO_OUT_OF_TRACKING_SIZE

    } ErrorCodes_t;

    ResyncRequiredError();
    bool IsUserSpaceErrorCode(const SV_ULONG &errorcode) const;
    std::string GetErrorMessage(const SV_ULONG &errorcode);
    std::string GetResyncReasonCode(const SV_ULONG &errorcode);

private:
    static const SV_ULONG USER_SPACE_MIN_ERROR;
    static const SV_ULONG USER_SPACE_MAX_ERROR;

private:
    ErrorCodeToMsg_t m_ErrorCodeToMsg;
    std::map<SV_ULONG, std::string> m_DriverErrorToResyncReasonHealthIssueCode;
};

struct Configurator;
class ResyncRequestor {
public: 
    // \brief reports resync required to CS
    bool ReportResyncRequired(
            Configurator* cxConfigurator, ///< configurator
            std::string volumeName,       ///< source device
            const SV_ULONGLONG & ts,      ///< resync required time stamp
            const SV_ULONG resyncErrVal,  ///< resync required error code
            char const* reasonTxt,        ///< resync required message
            const std::string &driverName///< filter driver name
            );

    bool ReportResyncRequired(
            Configurator* cxConfigurator,
            std::string volumeName,
            const SV_ULONG resyncErrVal,
            char const* reasonTxt);

    bool ReportPrismResyncRequired(
            Configurator* cxConfigurator,
            const std::string &sourceLunId,
            const SV_ULONGLONG ts,
            const SV_ULONGLONG resyncErrVal,
            char const* reasonTxt);

    bool ReportResyncRequired(
            std::string volumeName,       ///< source device
            const SV_ULONGLONG & ts,      ///< resync required time stamp
            const SV_ULONG resyncErrVal,  ///< resync required error code
            char const* reasonTxt,        ///< resync required message
            const std::string &driverName ///< filter driver name
        );

protected:
    void GetErrorMsg(
            unsigned long       resyncErrVal,
            char const*         reasonTxt,
            std::string         &volumeName,
            const std::string   &driverName,
            std::string         &errMsg
        );

    bool GetDriverErrorMsg(
            unsigned long       errCode,    ///< resync required error code
            char const* const*  volumes,    ///< insert values in the formatted message coming from filter driver
            char*               errTxt,     ///< resync required message
            int                 errTxtLen,  ///< resync required message length
            const std::string   &driverName ///< driver name
            );

private:
    ResyncRequiredError m_ResyncRequiredError;
};


#endif // ifndef RESYNCREQUESTOR__H
