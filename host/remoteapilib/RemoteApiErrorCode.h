///
///  \file RemoteApiErrorCode.h
///
///  \brief exception type of the remote api script error code thrown.
///

#ifndef REMOTEAPIERRORCODE_H
#define REMOTEAPIERRORCODE_H

///  \brief exception type of the remote api error exception thrown
enum RemoteApiErrorCode
{
	/// <summary>
	/// Target doesn't have enough available memory to complete the operation.
	/// </summary>
	StorageNotAvailableOnTarget,

	/// <summary>
	/// Copy to target machine failed.
	/// </summary>
	CopyFailed,

	/// <summary>
	/// Run on target machine failed.
	/// </summary>
	RunFailed
};

#endif // REMOTEAPIERRORCODE_H
