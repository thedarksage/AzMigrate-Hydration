///
///  \file  filterdrivernotifier.h
///
///  \brief contains FilterDriverNotifier implementation
///

#ifndef FILTER_DRIVER_NOTIFIER_H
#define FILTER_DRIVER_NOTIFIER_H

#include <string>
#include <stdexcept>
#include <boost/shared_ptr.hpp>
#include "devicestream.h"

/// \brief Filter driver notifier exception utility macro
#define FilterDriverNotifierEx(driverName, code, message) Exception(__FILE__, __LINE__, driverName, code, message)
#define TEST_FLAG(_a,_b) (_b == (_a & _b))

/// \brief notifier class to issue service shutdown notify and process start notify ioctl to filter driver
class FilterDriverNotifier
{
public:
	/// \brief pointer type
	typedef boost::shared_ptr<FilterDriverNotifier> Ptr;

	/// \brief exception thrown by this notifier
	class Exception : public std::exception
	{
	public:
		///\ brief constructor
		Exception(const char *fileName,    ///< source filename
			const int &lineNo,             ///< line number
			const std::string &driverName, ///< driver name
			const int &code,               ///< code
			const std::string &message     ///< message
			); 

		/// \brief destructor
		~Exception() throw();

		/// \brief message including driver name and code
		const char* what() const throw();

		/// \brief code
		///
		/// \note For device stream operations failure, it returns device stream's GetErrorCode()
		int code() const throw();

	private:
		int m_Code;            ///< code

		std::string m_Message; ///< message
	};

	/// \brief notification type
	enum NotificationType
	{
		E_SERVICE_SHUTDOWN_NOTIFICATION, ///< replication service issues this notification

		E_PROCESS_START_NOTIFICATION,    ///< draining process issues this notification

		//Must be always last
		E_UNKNOWN_NOTIFICATION_TYPE      ///< for error handling
	};

	/// \brief gets notification type in readable form
	///
	/// \return
	/// \li \c "service shutdown notification" or "process start notification"
	/// \li \c "unknown": on invalid notification type
	static std::string GetStrNotificationType(const NotificationType &type); ///< notification type

	/// \brief constructor
	FilterDriverNotifier(const std::string &driverName); ///< driver name to which notification is to be issued
	
	/// \brief destructor
	~FilterDriverNotifier();

	/// \brief issues notification
	void Notify(const NotificationType &notificationType);

#ifdef SV_WINDOWS
	/// \brief configures data paged pool for driver
    void ConfigureDataPagedPool(SV_ULONG ulDataPoolSizeInMB);
#endif

	/// \brief returns device stream that can be used by applications who have done notify successfully
	DeviceStream * GetDeviceStream(void);

private:
	std::string m_DriverName;          ///< driver name

	DeviceStream::Ptr m_pDeviceStream; ///< driver stream
};

#endif /* FILTER_DRIVER_NOTIFIER_H */