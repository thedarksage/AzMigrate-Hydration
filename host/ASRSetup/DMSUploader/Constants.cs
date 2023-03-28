using System;
using System.Collections.Generic;

namespace DMSUploader
{
    /// <summary>
    ///     Constants.
    /// </summary>
    public static class Constants
    {
        /// <summary>
        ///     Primary product.
        /// </summary>
        public const string PrimaryProduct = "Microsoft System Center";

        /// <summary>
        ///     Download type.
        /// </summary>
        public const string DownloadType = "Tool";

        /// <summary>
        ///     MS release type.
        /// </summary>
        public const string MSReleaseType = "tools";

        /// <summary>
        ///     Audience type.
        /// </summary>
        public const string AudienceType_BusinesDsdecisionMakers = "business decision makers";

        /// <summary>
        ///     Audience type.
        /// </summary>
        public const string AudienceType_ITProfessionals = "IT professionals";

        /// <summary>
        ///     Servers.
        /// </summary>
        public const string PublishingGroup = "Servers";

        /// <summary>
        ///     Server applications.
        /// </summary>
        public const string Categories = "Server Applications";

        /// <summary>
        ///     Download culture.
        /// </summary>
        public const string DownloadCulture = "en-us";

        /// <summary>
        ///     Manufacturer name.
        /// </summary>
        public const string ManufacturerName = "OSS";

        /// <summary>
        ///     Download file path.
        /// </summary>
        public const string DownloadFilePath = "DownloadFilePath";

        /// <summary>
        ///     DMS uploader log name.
        /// </summary>
        public const string DMSUploaderLog = "DMSUploader";

        /// <summary>
        ///     CreateDownload.
        /// </summary>
        public const string CreateDownload = "CREATEDOWNLOAD";

        /// <summary>
        ///     DeleteDownload.
        /// </summary>
        public const string DeleteDownload = "DELETEDOWNLOAD";

        /// <summary>
        ///     AadLoginEndpoint.
        /// </summary>
        public const string AadLoginEndpoint = "AadLoginEndpoint";

        /// <summary>
        ///     DeviceAuthEndpoint.
        /// </summary>
        public const string DeviceAuthEndpoint = "DeviceAuthEndpoint";

        /// <summary>
        ///     ArmEndpoint.
        /// </summary>
        public const string ArmEndpoint = "ArmEndpoint";

        /// <summary>
        ///     TenantId.
        /// </summary>
        public const string TenantId = "TenantId";

        /// <summary>
        ///     KeyVaultURL.
        /// </summary>
        public const string KeyVaultURL = "KeyVaultURL";

        /// <summary>
        ///     Retry time.
        /// </summary>
        public const int RetryTimeoutInMinutes = 300;

        /// <summary>
        /// Well-known client Id for Azure PowerShell.
        /// </summary>
        public const string PowerShellClientId = "1950a258-227b-4e31-a9cf-717495945fc2";

        /// <summary>
        ///Secret name.
        /// </summary>
        public const string SecretName = "DMS-Uploader-Secret";

        /// <summary>
        ///Secret name.
        /// </summary>
        public const string DmsResource = "Resource";

        /// <summary>
        ///Tenant name.
        /// </summary>
        public const string TenantName = "TenantName";

        /// <summary>
        ///DMS Authority.
        /// </summary>
        public const string DmsAuthority = "Authority";

        /// <summary>
        ///Dms Service.
        /// </summary>
        public const string DmsMgmtService = "DmsMgmtService";

        /// <summary>
        ///Client Id.
        /// </summary>
        public const string ClientId = "ClientId";
    }
}
