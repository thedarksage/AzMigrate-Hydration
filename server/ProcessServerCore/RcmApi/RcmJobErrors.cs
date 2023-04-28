// DO NOT EDIT! Auto-generated file.
// Find instructions in RcmJobErrors.xml to make modifications to this file.
// Ensure to check-in the changes to this file without ignoring.

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi
{
    public enum PSRcmJobErrorEnum
    {
        /// <Summary>
        /// Unknown job type %JobType; seen by Process Server of version %Version;.
        /// </Summary>
        UnknownJob = 520700,

        /// <Summary>
        /// Fail to parse the input for the %JobType; job seen by Process Server of version %Version;.
        /// </Summary>
        JobInputParseFailure = 520701,

        /// <Summary>
        /// Folder path %FolderPath; in the %JobType; job isn't under the configured Replication log folder path - %ReplicationLogFolderPath;.
        /// </Summary>
        JobFolderNotUnderReplicationLogFolderPath = 520702,

        /// <Summary>
        /// Failed to create folder %FolderPath; in the %JobType; job.
        /// </Summary>
        JobFolderCreationFailed = 520703,

        /// <Summary>
        /// Failed to delete folder %FolderPath; in the %JobType; job.
        /// </Summary>
        JobFolderDeletionFailed = 520704,

        /// <Summary>
        /// Unimplemented job type %JobType; seen by Process Server of version %Version;.
        /// </Summary>
        UnimplementedJob = 520705,

        /// <Summary>
        /// The %JobType; job failed due to an unexpected error.
        /// </Summary>
        JobUnexpectedError = 520706,

        /// <Summary>
        /// The %JobType; job failed as replicating VM settings change is not reflected at appliance.
        /// </Summary>
        JobProcessServerPrepareForSwitchApplianceFailed = 520707,

    }
}

// DO NOT EDIT! Auto-generated file.
// Find instructions in RcmJobErrors.xml to make modifications to this file.
// Ensure to check-in the changes to this file without ignoring.
