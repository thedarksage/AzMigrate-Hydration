///
///  \file WmiErrorCode.h
///
///  \brief exception type of the wmi error exception thrown
///

#ifndef WMIERRORCODE_H
#define WMIERRORCODE_H

///  \brief exception type of the wmi error exception thrown
enum WmiErrorCode
{
	/// <summary>
	/// The specified network name or path is not found or no longer available.
	/// </summary>
	NetworkNotFound,

	/// <summary>
	/// Access to perform the operation is denied.
	/// </summary>
	AccessDenied,

	/// <summary>
	/// Account used to logon has been currently locked out.
	/// </summary>
	LoginAccountLockedOut,

	/// <summary>
	/// The network logon service was not started.
	/// </summary>
	LogonServiceNotStarted,

	/// <summary>
	/// There are no logon servers to process the login request.
	/// Either logon service is not running or it is listening on a different port.
	/// </summary>
	LogonServersNotAvailable,

	/// <summary>
	/// Trust relationship between the workstation and primary domain failed.
	/// </summary>
	DomainTrustRelationshipFailed,

	/// <summary>
	/// The user account provided to login has been currently disabled.
	/// </summary>
	LoginAccountDisabled,

	/// <summary>
	/// Target doesn't have enough available memory to complete the operation.
	/// </summary>
	SpaceNotAvailableOnTarget

};

#endif // WMIERRORCODE_H
