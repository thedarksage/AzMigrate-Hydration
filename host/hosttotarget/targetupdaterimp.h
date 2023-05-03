
///
/// \file targetupdaterimp.h
///
/// \brief
///

#ifndef TARGETUPDATERIMP_H
#define TARGETUPDATERIMP_H


#include <string>

#include "hosttotargetmajor.h"

namespace HostToTarget {
    typedef std::pair<ERR_TYPE, ERR_TYPE> error_t; ///< used by support routines to return errors so TargetUpdater can set it

    /// \brief class uded to update a target when performing host-to-target
    ///
    class TargetUpdaterImp {
    public:

        /// \brief constructor
        ///
        /// \exception throws ERROR_EXCEPTION on failure to construct
        explicit TargetUpdaterImp(HostToTargetInfo const& info)
            : m_info(info),
              m_systemError(ERROR_OK),
              m_internalError(ERROR_OK)
            {
            }

        /// \brief destructor
        virtual ~TargetUpdaterImp()
            {
            }

        /// \brief validates that everything needed is available
        ///
        /// \return bool where
        /// \li true indicates passed validation
        /// \li false indicates did not pass validation. getError or getErrorAsString for specific failure
        virtual bool validate() = 0;

        /// \brief performs the needed updates. Must be implemented by subclass
        ///
        /// \return bool where
        /// \li true indicates success
        /// \li false indicates error. Use getError or getErrorAsString for specific failure
        virtual bool update(int opts) = 0;

        /// \brief gets the current system error code
        ///
        /// \note this is system specific. probably want to use getErrorAsString
        ///       or to see if there was an error use ok()
        /// \return
        /// \li ERR_TYPE which is the current error code
        ERR_TYPE getSystemError()
            {
                return m_systemError;
            }

        /// \brief gets the current internal error code
        ///
        /// \note probably want to use getErrorAsString
        ///       or to see if there was an error use ok()
        ///
        /// \return
        /// \li ERR_TYPE which is the current error code
        ERR_TYPE getInternalError()
            {
                return m_internalError;
            }

        /// \brief returns the error message for the given error
        ///
        /// \return
        /// \li std::string holding the error message for current error
        ///  if the error message can not be found, returns error message stating
        ///  "failed to get message for error code: err"
        std::string getErrorAsString();

        /// \brief clears current error setting it success
        void clearError()
            {
                m_internalError = ERROR_OK;
                m_systemError = ERROR_OK;
                m_errMsg.clear();
            }

        /// \brief checks if the curren error is OK or failurer
        ///
        /// \return bool where
        /// \li true indicates success
        /// \li false indicates error. Use getError or getErrorAsString for specific failure
        bool ok()
            {
                return (ERROR_OK == m_internalError && ERROR_OK == m_systemError);
            }

    protected:
        /// \brief sets current internal error and optional system error
        void setError(ERR_TYPE internalErr,           ///< internal error
                      ERR_TYPE systemErr = ERROR_OK   ///< optional system error
                      )
            {
                m_internalError = internalErr;
                m_systemError = systemErr;
            }

        void setError(ERR_TYPE internalErr,      ///< internal error
                      ERR_TYPE systemErr,        ///< optional system error
                      std::string const& errMsg  ///< optional additional error message
                      )
            {
                setError(internalErr, systemErr);
                m_errMsg = errMsg;
            }

        void setError(ERR_TYPE internalErr,      ///< internal error
                      ERR_TYPE systemErr,        ///< optional system error
                      std::wstring const& errMsg  ///< optional additional error message
                      )
            {
                setError(internalErr, systemErr);
                m_errMsg.clear();
                std::locale loc;                
                std::wstring::const_iterator it(errMsg.begin());
                std::wstring::const_iterator endIt(errMsg.end());
                for (/* empty */ ; it != endIt; ++it) {
                    m_errMsg += std::use_facet<std::ctype<wchar_t> >(loc).narrow(*it, '.');
                }
            }

        void setError(ERR_TYPE internalErr,       ///< internal error
                      ERR_TYPE systemErr,         ///< optional system error
                      wchar_t const*& errMsg  ///< optional additional error message
                      )
            {
                setError(internalErr, systemErr);
                m_errMsg.clear();
                std::locale loc;
                wchar_t const* wCh = errMsg;
                for (/* empty */ ; L'\0' != *wCh; ++wCh) {
                     m_errMsg += std::use_facet<std::ctype<wchar_t> >(loc).narrow(*wCh, '.');
                }
            }

        HostToTargetInfo& getHostToTargetInfo()
            {
                return m_info;
            }

    protected:

    private:
        HostToTargetInfo m_info; ///< HostToTargteInfo

        ERR_TYPE m_systemError;   //< current system error. can be "success"
        ERR_TYPE m_internalError; //< set to one of the internal errors. can be "success"
        std::string m_errMsg;     ///< additional error info when available. can be empty even if there are errors.

    };

} // namesapce HostToTarget



#endif // TARGETUPDATERIMP_H
