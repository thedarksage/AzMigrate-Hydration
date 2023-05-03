
///
/// \file targetupdater.h
///
/// \brief TargetUpdater interface
///
/// \example hosttotargettst.cpp
///
/// \mainpage
///
/// \section hostToTargetPage Host to Target Library
///
/// The Host to Target library allows you to recover (clone) a protected host
/// to a different target. It currently assumes you want to recover the entire
/// host to a new target and not just a subset of volumes.
///
/// Users of the library should read \ref hostToTargetUsers. Developers should
/// read \ref hostToTargetDevelopers
///
/// <hr>
/// \subsection hostToTargetUsers Using Host to Target library
///
/// To use the Host to Target library you will need to include targetupdater.h
/// It is under the host/hosttotarget module (directory).
///
/// HostToTargetInfo and TargetUpdater are all that is needed. You fill in a
/// HostToTargetInfo structure and pass that in when instantiating a TargetUpdater.
///
/// The HostToTargetInfo memebers  m_targetType, m_hostOsName, and m_installPath
/// must be set. If the host OS is windows then m_systemVolume must also be set to
/// the host's system volume (i.e. the volume that has the WINDOWS directory).
///
/// The library is implemented in the HostToTarget namespace. So you will need
/// to either use fully qualified names
///
/// \code
/// 
///   HostToTarget::TargetUpdater(info);
/// \endcode
///
/// or add one of the using options
///
/// directive
///
/// \code
/// 
///   using namespace HostToTarget;
/// \endcode
///
/// decleration
///
/// \code
/// 
///   using HostToTarget::TargetUpdater;
///   using HostToTarget::HostToTargetInfo;
/// \endcode
///
/// Remember to add those only in source (.cpp) files (no header files) and after
/// all includes.
///
/// Using the library is straight forward.
///
/// You will need to add include and link directories and library file names
/// to the modules project and Makefile.
///
/// \b windows (make sure to do this for all configurations and architectures:both debug and release, 32 and 64 bit)
///
/// add these to Additional Include Directories
/// \code
/// 
///  ../errorexception
///  ../scopeguard
///  ../hosttotarget
///  ../hosttotarget/win
///  $(BOOST_INCLUDES)
/// \endcode
///
/// for linking on windows check hosttotarget in the Project Dependencies dialog
/// and add boost to Additional Library Directories
///
/// \code
/// 
/// $(BOOST_LIB_DIR)
/// \endcode
///
/// \b unix
///
/// add these to the module Makefile
/// \code
/// 
/// X_FLAGS = \
///     -Ierrorexception \
///     -Iscopeguard \
///     -Ihosttotarget \
///     -Ihosttotarget/$(X_MAJOR_ARCH) \
///     $(BOOST_INCLUDE)
///
/// X_LIBS = \
///     hosttotarget
///
/// X_THIRDPARTY_LIBS = \
///     $(BOOST_LIBS)
///
/// \endcode
///
/// Here is a snippet from hosttotargettst.cpp
/// (an example test program that expects all the HostToTargetInfo paramaters to be
///  passed in on the command)
/// \code
/// 
/// #include "targetupdater.h"
///
/// ...
///
/// int main(int argc, char* argv[])
/// {
///     if (!parseCmd(argc, argv)) {
///        return -1;
///     }
///     try {
///         // need the try block as TargetUpdater constructor will throw on error
///         HostToTarget::HostToTargetInfo info(g_targetType, g_hostOsName, g_installDir, g_systemVolume);
///         HostToTarget::TargetUpdater targetUpdater(info);
///         // use one or more update options, (see UPDATE_OPTIONS for details
///         if (!targetUpdater.update(HostToTarget::UPDATE_OTPION_MANUAL_START_SERVICES
///             | HostToTarget::UPDATE_OPTION_CLEAR_CONFIG_SETTINGS)) {
///             std::cout  << targetUpdater.getErrorAsString() << '\n';
///         }
///         return (targetUpdater.ok() ? 0 : -1);
///     } catch (ErrorException const& e) {
///         std::cout << e.what() << '\n';
///     }
///     return -1;
/// }
///
/// \endcode
///
/// <hr>
/// \subsection hostToTargetDevelopers Developing Host to Target library
///
/// The library is located under the host/hosttotraget module (directory).
///
/// The library is implemented in the HostToTarget namespace. When adding new files
/// make sure to put everything under that namespace
///
/// \code
/// 
/// // all includes should be outside the namespace
///
/// namespace HostToTarget {
///
///     // all code goes here
///
/// }
///  \endcode
///
/// It is designed with both extensibility and portablilty in mind.
///
/// (FIXME: need to add actual details)
///

#ifndef TARGETUPDATER_H
#define TARGETUPDATER_H

#include <string>

#include "hosttotargetmajor.h"
#include "targetupdaterimpmajor.h"

namespace HostToTarget {

    /// \brief class uded to update a target when performing host-to-target
    ///
    class TargetUpdater {
    public:

        /// \brief constructor
        ///
        /// \exception throws ERROR_EXCEPTION on failure to construct
        explicit TargetUpdater(HostToTargetInfo const& info)
            : m_imp(info)
            {
            }

        /// \brief destructor
        virtual ~TargetUpdater()
            {
            }

        /// \brief validates that everything needed is available
        ///
        /// \return bool where
        /// \li true indicates passed validation
        /// \li false indicates did not pass validation. getError or getErrorAsString for specific failure
        bool validate()
            {
                return m_imp.validate();
            }

        /// \brief performs the needed updates. Must be implemented by subclass
        ///
        /// \return bool where
        /// \li true indicates success
        /// \li false indicates error. Use getError or getErrorAsString for specific failure
        bool update(int opts)
            {
                return m_imp.update(opts);
            }

        /// \brief gets the current system error code
        ///
        /// \note this is system specific. probably want to use getErrorAsString
        ///       or to see if there was an error use ok()
        /// \return
        /// \li ERR_TYPE which is the current error code
        ERR_TYPE getSystemError()
            {
                return m_imp.getSystemError();
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
                return m_imp.getInternalError();
            }

        /// \brief returns the error message for the given error
        ///
        /// \return
        /// \li std::string holding the error message for current error
        ///  if the error message can not be found, returns error message stating
        ///  "failed to get message for error code: err"
        std::string getErrorAsString()
            {
                return m_imp.getErrorAsString();
            }

        /// \brief clears current error setting it success
        void clearError()
            {
                m_imp.clearError();
            }

        /// \brief checks if the curren error is OK or failurer
        ///
        /// \return bool where
        /// \li true indicates success
        /// \li false indicates error. Use getError or getErrorAsString for specific failure
        bool ok()
            {
                return m_imp.ok();
            }

    protected:

    private:
        TargetUpdaterImpMajor m_imp; ///< actual implementation used for TargetUpdater
    };

} // namesapce HostToTarget


#endif // TARGETUPDATER_H
