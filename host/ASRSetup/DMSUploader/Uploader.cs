using System;
using System.Collections.Generic;
using System.Configuration;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Logger;
using Microsoft.Downloads.Services.Client;
using Microsoft.Downloads.Services.Client.ViewModels;
using Download = Microsoft.Downloads.Services.Client.Download;
using Release = Microsoft.Downloads.Services.Client.Release;
using Configuration = Microsoft.Downloads.Services.Client.Configuration;
using UST.Library.Dms.Contracts;
using AD = Microsoft.IdentityModel.Clients.ActiveDirectory;
using Azure.Core;
using Azure.Identity;
using Azure.Security.KeyVault.Secrets;
using Microsoft.WindowsAzure.Management;

namespace DMSUploader
{
    /// <summary>
    ///     Abstract class for uploading files.
    /// </summary>
    public class Uploader
    {
        /// <summary>
        /// Azure authentication url.
        /// </summary>
        private static string aadLoginEndpoint =
            ConfigurationManager.AppSettings.Get(Constants.AadLoginEndpoint);

        /// <summary>
        /// Azure login url.
        /// </summary>
        private static string deviceAuthEndpoint =
            ConfigurationManager.AppSettings.Get(Constants.DeviceAuthEndpoint);

        /// <summary>
        /// Azure key vault url.
        /// </summary>
        private static string armEndpoint =
            ConfigurationManager.AppSettings.Get(Constants.ArmEndpoint);

        /// <summary>
        /// Azure tenant id.
        /// </summary>
        private static string tenantId =
            ConfigurationManager.AppSettings.Get(Constants.TenantId);

        /// <summary>
        /// Azure key vault url.
        /// </summary>
        private static string keyVaultURL =
            ConfigurationManager.AppSettings.Get(Constants.KeyVaultURL);

        /// <summary>
        /// DMS resource url.
        /// </summary>
        private static string dmsResourceUrl =
            ConfigurationManager.AppSettings.Get(Constants.DmsResource);

        /// <summary>
        /// Tenant name.
        /// </summary>
        private static string tenantName =
            ConfigurationManager.AppSettings.Get(Constants.TenantName);

        /// <summary>
        /// DMS Authority.
        /// </summary>
        private static string dmsAuthority =
            ConfigurationManager.AppSettings.Get(Constants.DmsAuthority);

        /// <summary>
        /// DMS Serive.
        /// </summary>
        private static string dmsMgmtService =
            ConfigurationManager.AppSettings.Get(Constants.DmsMgmtService);

        /// <summary>
        /// Client Id.
        /// </summary>
        private static string clientId =
            ConfigurationManager.AppSettings.Get(Constants.ClientId);

        /// <summary>
        ///     Delete the download.
        /// </summary>
        /// <param name="downloadname">The Downloadname.</param>
        public static bool DeleteDownload(string downloadname, Guid releaseId)
        {
            Initialize();

            try
            {
                var download = Download.GetDownloadByName(downloadname);
                var detail = DownloadDetail.GetDownloadDetail(download.DownloadId, Constants.DownloadCulture);
                //var releaseInfo = Release.NewRelease(download.DownloadId, new[] { Constants.DownloadCulture });
                var release = Release.GetRelease(releaseId);

                if (detail.ToString() != null)
                {
                    if(detail.IsPublished)
                    {
                        Trc.Log(
                            LogLevel.Always,
                            "Download detail published. Unpublishing it.");

                        detail.Unpublish();

                        Trc.Log(
                            LogLevel.Always,
                            "Download detail unpublished.");
                    }

                    Trc.Log(
                        LogLevel.Always,
                        "Deleting download detail.");

                    //Delete detail
                    detail.Delete();
                    Trc.Log(
                        LogLevel.Always,
                        "Download detail deleted successfully.");
                }

                if (release.ToString() != null)
                {
                    if (release.Status == (int)ReleaseStatus.Live || release.Status == (int)ReleaseStatus.UnpublishError)
                    {
                        Trc.Log(
                            LogLevel.Always,
                            "Release is live. Unpublishing it.");

                        release.Unpublish();
                        UpdateReleaseStatus(release, ReleaseStatus.UnpublishActivated, ActionType.UnpublishRelease);

                        Trc.Log(
                            LogLevel.Always,
                            "Release unpublished.");
                    }

                    Trc.Log(
                        LogLevel.Always,
                        "Deleting release.");

                    //Delete release
                    release.Delete();
                    Trc.Log(
                        LogLevel.Always,
                        "Release deleted successfully.");
                }

                if (download.ToString() != null)
                {
                    Trc.Log(
                        LogLevel.Always,
                        string.Format(
                            "Download with name - '{0}' exists. Deleting...",
                            downloadname));

                    //Delete download
                    download.Delete();
                    UpdateReleaseStatus(release, ReleaseStatus.Deleted, ActionType.DeleteRelease);

                    Trc.Log(
                        LogLevel.Always,
                        string.Format(
                            "Download with name - '{0}' deleted successfully.",
                            downloadname));
                }
            }
            catch (NullReferenceException)
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Download with name - {0} not exist. Please provide valid download name to delete.",
                        downloadname));
                return false;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Delete download - '{0}' failed with exception {1}",
                        downloadname,
                        ex));
                return false;
            }

            return true;
        }

        /// <summary>
        ///     Add signer information to release.
        /// </summary>
        /// <param name="release">The release.</param>
        /// <param name="email">The signer email id.</param>
        /// <param name="role">The signer role.</param>
        public static void AddSignerToRelease(Release release, string email, ApproverRole role)
        {
            var approver = new Approval
            {
                Approver = email,
                Status = ApprovalStatus.Pending,
                StatusDateTime = null,
                Role = role
            };

            Trc.Log(
                LogLevel.Always,
                string.Format(
                    "Adding signer {0} to release.",
                    email));
            release.Signers.Add(new LocalApproval(approver));
        }

        /// <summary>
        ///     Add file to release.
        /// </summary>
        /// <param name="release">The release.</param>
        /// <param name="fileName">The file name.</param>
        /// <param name="display">If to display the file.</param>
        /// <param name="isPrimary">If the file is primary.</param>
        /// <param name="path">The file path.</param>
        private static void AddFileToRelease(Release release, string fileName, bool display, bool isPrimary, string path = "")
        {
            var fileInfo = new FileInfo(fileName);
            var localFile = new LocalFile(fileInfo)
            {
                Display = display,
                IsPrimary = isPrimary
            };

            if (!string.IsNullOrWhiteSpace(path))
            {
                localFile.Path = path;
            }

            Trc.Log(
                 LogLevel.Always,
                 string.Format(
                     "Adding file {0} to release.",
                     fileName));

            release.Files.Add(localFile);
        }

        /// <summary>
        ///     Add file path to release.
        /// </summary>
        /// <param name="release">The release.</param>
        /// <param name="path">The file path.</param>
        public static void AddFolderToRelease(Release release, string path)
        {
            var directory = new DirectoryInfo(path);
            directory.GetFiles("*", SearchOption.AllDirectories).ToList().ForEach(
                file =>
                {
                    var filePath = $@"{directory.Name}\{file.FullName.Substring(directory.FullName.Length + 1)}";
                    AddFileToRelease(release, file.FullName, true, false, filePath);
                });
        }

        /// <summary>
        ///     Creates new download and release.
        /// </summary>
        /// <param name="inputParameters">The inputParameters.</param>
        public static bool ToLive(MandatoryParameters inputParameters)
        {
            string downloadName = inputParameters.DownloadName;
            string owners = inputParameters.Owners;
            string systemRequirements = inputParameters.SystemRequirements;
            string instructions = inputParameters.Instructions;
            string operatingSystem = inputParameters.OperatingSystem;
            string version = inputParameters.Version;
            string thirdPartyBits = inputParameters.ContainThirdPartyBits;
            List<string> signers = inputParameters.Signers;
            List<string> fileNames = inputParameters.FilesPath;
            bool isPrimary = false;

            if (string.IsNullOrWhiteSpace(downloadName) || string.IsNullOrWhiteSpace(owners) || string.IsNullOrWhiteSpace(systemRequirements) || string.IsNullOrWhiteSpace(instructions) ||
                string.IsNullOrWhiteSpace(operatingSystem) || string.IsNullOrWhiteSpace(version))
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                        "Please provide valid downloadName/owners/systemRequirements/instructions/operatingSystem/version details on input json file.");
                return false;
            }

            if (string.IsNullOrWhiteSpace(thirdPartyBits))
            {
                thirdPartyBits = "false";
            }

            if (signers == null || !signers.Any())
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                        "Signers information is null. Please provide valid signers mail id on input json file.");
                return false;
            }
            else
            {
                foreach (string emailId in signers)
                {
                    if (!Validation.IsValidEmail(emailId))
                    {
                        Trc.Log(
                            LogLevel.ErrorToUser,
                            "Please provide valid signers mail id on input json file.");
                        return false;
                    }
                }
            }

            if (fileNames != null)
            {
                foreach(string filename in fileNames)
                {
                    if(!File.Exists(filename))
                    {
                        Trc.Log(
                            LogLevel.ErrorToUser,
                            string.Format(
                                "File - '{0}' not exists. Provide valid file path on input json file.",
                                filename));
                        return false;
                    }
                }
            }

            string currDir =
                    new System.IO.FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location).DirectoryName;
            string downloadFilePath = Path.Combine(currDir, string.Concat(Constants.DownloadFilePath, "_", downloadName, ".txt"));

            bool containThirdPartyBits = thirdPartyBits == "true" ? true : false;

            Initialize();

            try
            {
                //Create download if not exists
                if (Download.GetDownloadByName(downloadName).ToString() != null)
                {
                    Trc.Log(
                        LogLevel.ErrorToUser,
                        string.Format(
                            "Download with name - '{0}' already exists. Choose different name.",
                            downloadName));
                    return false;
                }
            }
            //GetDownloadByName itself returns null when download not exists.
            catch (NullReferenceException)
            {
                Trc.Log(
                    LogLevel.Always,
                    string.Format(
                        "Download with name - '{0}' not exist. Proceeding with creation.",
                        downloadName));
            }

            // Create download
            var download = Download.NewDownload(downloadName);
            download.PrimaryProduct = GetKeyFromValue<Guid>(DMSCache.primaryProductList, Constants.PrimaryProduct);
            download.DCType = GetKeyFromValue<int>(DMSCache.downloadTypeList, Constants.DownloadType);
            download.MSReleaseType = GetKeyFromValue<int>(DMSCache.msReleaseTypeList, Constants.MSReleaseType);
            download.AudienceTypes = GetKeysFromValues<int>(DMSCache.audienceTypesList, new List<string> { Constants.AudienceType_BusinesDsdecisionMakers, Constants.AudienceType_ITProfessionals });
            download.PublishingGroup = GetKeyFromValue<int>(DMSCache.publishGroupList, Constants.PublishingGroup);
            download.DCCategories = GetKeysFromValues<long>(DMSCache.downloadCategoriesList, new List<string> { Constants.Categories });
            download.ExpirationDate = DateTime.UtcNow.AddYears(1);
            //owner emails which was splited by semi-colon
            download.Owners = owners;

            try
            {
                Trc.Log(
                    LogLevel.Always,
                    string.Format(
                        "Creating new download with name - '{0}'.",
                        downloadName));

                // Save download
                download.Persist();

                Trc.Log(
                    LogLevel.Always,
                    string.Format(
                        "Download '{0}' created successfully.",
                        downloadName));
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Create download failed with Exception: {0}",
                        ex));
                return false;
            }


            var release = Release.NewRelease(download.DownloadId, new[] { Constants.DownloadCulture });

            try
            {
                Trc.Log(
                    LogLevel.Always,
                    "Adding release to download.");

                // Basic info
                release.OperatingSystems = GetKeysFromValues(DMSCache.osList, new List<string> { operatingSystem });
                release.Version = version;

                // Thirdparty information
                if (containThirdPartyBits)
                {
                    release.ContainsThirdPartyBits = true;  //if set the value to true, we can specify the release contains ThirdPartyBits
                    release.ManufacturerName = Constants.ManufacturerName;   //set the manufacture name
                }

                // Signers information
                foreach (string signer in signers)
                {
                    AddSignerToRelease(release, signer, ApproverRole.Developer);
                }

                // Add Files
                foreach (string fileName in fileNames)
                {
                    if (fileName == fileNames.First())
                    {
                        isPrimary = true;
                    }
                    else
                    {
                        isPrimary = false;
                    }

                    Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "isPrimary: {0}",
                        isPrimary.ToString()));
                    AddFileToRelease(release, fileName, true, isPrimary);
                }

                // Save release
                release.Persist();
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Add release failed with Exception: {0}",
                        ex));
                return false;
            }

            // wait for release to be ready for compliance
            UpdateReleaseStatus(release, ReleaseStatus.ReadyForCompliance, ActionType.UpdateRelease);

            // wait for release to be activated
            release.Activate();
            UpdateReleaseStatus(release, ReleaseStatus.ComplianceActivated, ActionType.ActivateCompliance);

            // wait for manually approve release
            UpdateReleaseStatus(release, ReleaseStatus.ReadyForPublish, ActionType.ApproveRelease);

            // publish release
            release = GetReleaseInformation(release.ReleaseId);

            try
            {
                release.Publish();
                Trc.Log(
                    LogLevel.Always,
                    "Release published successfully.");
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Publish release failed with Exception: {0}",
                        ex));
            }

            UpdateReleaseStatus(release, ReleaseStatus.Live, ActionType.PublishRelease);


            string[] releasePaths = Release.GetReleaseFilePaths(release.ReleaseId);

            // Write download path to file.
            File.WriteAllLines(downloadFilePath, releasePaths, Encoding.UTF8);

            foreach (string releasePath in releasePaths)
            {
                Console.WriteLine($"Download URL: {releasePath}");
            }

            Trc.Log(
                LogLevel.Always,
                "Adding detail to download.");

            // Add download detail
            DownloadDetail detail = DownloadDetail.NewDownloadDetail(download.DownloadId, Constants.DownloadCulture);
            detail.Name = downloadName;
            detail.ShortName = downloadName;
            detail.ShortDescription = downloadName;
            detail.LongDescription = downloadName;
            detail.Requirements = systemRequirements;
            detail.Instructions = instructions;
            detail.Keywords = downloadName;

            try
            {
                // Save detail
                detail.Persist();

                Trc.Log(
                    LogLevel.Always,
                    string.Format(
                        "Added download detail successfully.",
                        currDir));

                //Publish detail
                detail.Publish();

                Trc.Log(
                    LogLevel.Always,
                    string.Format(
                        "Published download detail successfully.",
                        currDir));
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Add download detail failed with Exception: ",
                        ex));
                return false;
            }

            return true;
        }

        /// <summary>
        ///     Update release status.
        /// </summary>
        /// <param name="release">The release.</param>
        /// <param name="nextState">The next state the release will be.</param>
        /// <param name="lastAction">The last action user made.</param>
        private static void UpdateReleaseStatus(Release release, ReleaseStatus nextState, ActionType lastAction)
        {
            Trc.Log(
                LogLevel.Always,
                string.Format(
                    "Waiting for release state to change to be {0}.",
                    nextState));

            var isReady = WaitForReleaseState(release, nextState);
            if (isReady)
            {
                return;
            }

            var errorMsg = $"Release is not in {nextState} state in {Constants.RetryTimeoutInMinutes} minutes after {lastAction}.";

            Trc.Log(
                LogLevel.ErrorToUser,
                errorMsg);
            throw new InvalidRequestException(errorMsg);
        }

        /// <summary>
        ///     Start timer to wait for the file to change state.
        /// </summary>
        /// <param name="release">The release.</param>
        /// <param name="expectedState">The expected state.</param>
        /// <returns>True if the release is in expected state.</returns>
        private static bool WaitForReleaseState(Release release, ReleaseStatus expectedState)
        {
            Stopwatch sw = new Stopwatch();
            sw.Start();
            //Wait 1 hour
            while (sw.ElapsedMilliseconds < Constants.RetryTimeoutInMinutes * 60 * 1000)
            {
                if (IsExpectedState(release, expectedState))
                {
                    return true;
                }

                Thread.Sleep(10 * 1000);
            }

            sw.Stop();
            return false;
        }

        /// <summary>
        ///     Whether the release state is expected.
        /// </summary>
        /// <param name="release">The release.</param>
        /// <param name="expectedState">The expected release state.</param>
        /// <returns>True if the release state is expected.</returns>
        private static bool IsExpectedState(Release release, ReleaseStatus expectedState)
        {
            var byteActual = GetReleaseInformation(release.ReleaseId).Status;
            var actual = (ReleaseStatus)Enum.Parse(typeof(ReleaseStatus), byteActual.ToString());
            if (expectedState == actual)
            {
                return true;
            }

            return false;
        }

        /// <summary>
        ///     Get value.
        /// </summary>
        public static T GetKeyFromValue<T>(Dictionary<T, string> dictionary, string value)
        {
            if (dictionary == null || !dictionary.Any())
            {
                throw new Exception("Dictionary is empty or null");
            }

            if (!dictionary.ContainsValue(value))
            {
                throw new Exception($"Dictionary does not contain value: {value}");
            }

            return dictionary.FirstOrDefault(x => x.Value.Equals(value, StringComparison.CurrentCultureIgnoreCase)).Key;
        }

        /// <summary>
        ///     Get value.
        /// </summary>
        public static T[] GetKeysFromValues<T>(Dictionary<T, string> dictionary, List<string> values)
        {
            if (dictionary == null || !dictionary.Any())
            {
                throw new Exception("Dictionary is empty or null");
            }

            if (!dictionary.Values.Intersect(values).Any())
            {
                throw new Exception($"Dictionary does not contain value: {string.Join(",", values)}");
            }

            var keys = new T[values.Count];
            for (var i = 0; i < keys.Length; i++)
            {
                var key = dictionary.FirstOrDefault(x => x.Value.Equals(values[i], StringComparison.CurrentCultureIgnoreCase)).Key;
                keys[i] = key;
            }

            return keys;
        }

        /// <summary>
        ///     Get Release information.
        /// </summary>
        /// <param name="releaseId">The release ID.</param>
        /// <returns>Release information</returns>
        public static Release GetReleaseInformation(Guid releaseId)
        {
            int index = 1;
            List<Exception> exceptions = new List<Exception>();

            do
            {
                try
                {
                    Release release;
                    release = Release.GetRelease(releaseId);
                    return release;
                }
                catch (WebException ex)
                {
                    string errorMessage;
                    if (IsRetryableWebException(ex, out errorMessage))
                    {
                        continue;
                    }

                    Trc.Log(
                        LogLevel.ErrorToUser,
                        String.Format(
                            "Couldn't fetch release information. Error message: {0}",
                            errorMessage));
                    exceptions.Add(ex);
                    break;
                }
                catch (Exception ex)
                {
                    Trc.Log(
                        Logger.LogLevel.ErrorToUser,
                        String.Format(
                            "Couldn't fetch release information. Exception: {0}",
                            ex.Message.ToString()));
                    exceptions.Add(ex);
                    break;
                }
            }
            while (++index <= 3);

            throw new AggregateException(exceptions);
        }

        /// <summary>
        /// Parse web exception and return appropriate response.
        /// </summary>
        /// <param name="ex">Webexception object.</param>
        /// <param name="errorMessage">Error message.</param>
        /// <returns>True if exception is retryable.</returns>
        public static bool IsRetryableWebException(WebException ex, out string errorMessage)
        {
            bool retryRequired = false;

            errorMessage = string.Empty;
            using (WebResponse response = ex.Response)
            {
                HttpWebResponse httpResponse = (HttpWebResponse)response;

                if (httpResponse.StatusCode == HttpStatusCode.ServiceUnavailable)
                {
                    retryRequired = true;
                }
                else if (httpResponse.StatusCode == HttpStatusCode.InternalServerError)
                {
                    retryRequired = true;
                }
                else if (httpResponse.StatusCode == HttpStatusCode.Conflict)
                {
                    retryRequired = true;
                }
                else if (httpResponse.StatusCode == HttpStatusCode.RequestTimeout)
                {
                    retryRequired = true;
                }
                else
                {
                    errorMessage = new StreamReader(response.GetResponseStream()).ReadToEnd();
                }
            }

            return retryRequired;
        }

        /// <summary>
        /// Initialize DMS configuration.
        /// </summary>
        public static void Initialize()
        {
            var secret = GetSecret();

            Trc.Log(
                LogLevel.Always,
                "Initializing DMS");

            Configuration.Initialize(tenantName,
                dmsResourceUrl,
                dmsAuthority,
                dmsMgmtService,
                clientId,
                secret);


        }

        /// <summary>
        /// Gets key-value secret in Azure Key Vault.
        /// </summary>
        /// <param name="secretName">The secret name.</param>
        /// <returns>Secret value.</returns>
        public static string GetSecret()
        {
            var secretClient = new SecretClient(new Uri(keyVaultURL), new DefaultAzureCredential());
            KeyVaultSecret secret = secretClient.GetSecret(Constants.SecretName);
            return secret.Value;
        }

        /// <summary>
        /// Gets the access token from AAD.
        /// </summary>
        /// <param name="authority">Address of the authority to issue token.</param>
        /// <param name="resource">Identifier of the target resource that is
        /// the recipient of the requested token.</param>
        /// <param name="scope">The scope of the authentication request.</param>
        /// <returns>The access token.</returns>
        public static async Task<string> GetAccessToken(string authority, string resource, string scope)
        {
            var authContext = new AD.AuthenticationContext(string.Concat(aadLoginEndpoint, tenantId));

            AD.DeviceCodeResult deviceCode = await GetDeviceCode();

            Trc.Log(
                LogLevel.Always,
                string.Format(
                    "Code to enter on logon page: {0}",
                    deviceCode.UserCode));

            System.Diagnostics.Process.Start(deviceAuthEndpoint);

            AD.AuthenticationResult authResult = await GetToken(deviceCode);

            return authResult.AccessToken;
        }

        /// <summary>
        /// Acquires device code from the authority.
        /// </summary>
        /// <returns>Returns Device Code.</returns>
        public static async Task<AD.DeviceCodeResult> GetDeviceCode()
        {
            var authContext = new AD.AuthenticationContext(string.Concat(aadLoginEndpoint, tenantId));

            AD.DeviceCodeResult codeResult =
                await authContext.AcquireDeviceCodeAsync(
                        armEndpoint,
                        Constants.PowerShellClientId);

            return codeResult;
        }

        /// <summary>
        /// Acquires security token from the authority using an device code.
        /// </summary>
        /// /// <param name="deviceCode">The device code result</param>
        /// <returns>Returns Access Token.</returns>
        public static async Task<AD.AuthenticationResult> GetToken(AD.DeviceCodeResult deviceCode)
        {
            var authContext = new AD.AuthenticationContext(string.Concat(aadLoginEndpoint, tenantId));

            AD.AuthenticationResult authResult =
                await authContext.AcquireTokenByDeviceCodeAsync(
                    deviceCode);

            if (authResult == null)
            {
                throw new InvalidOperationException("Failed to obtain the token");
            }

            return authResult;
        }
    }

    /// <summary>
    /// Requirement name and its parameters.
    /// </summary>
    public class MandatoryParameters
    {
        /// <summary>
        ///Download name.
        /// </summary>
        public string DownloadName { get; set; }

        /// <summary>
        ///Download Owners.
        /// </summary>
        public string Owners { get; set; }

        /// <summary>
        ///System Requirements.
        /// </summary>
        public string SystemRequirements { get; set; }

        /// <summary>
        ///Instructions.
        /// </summary>
        public string Instructions { get; set; }

        /// <summary>
        ///Operating System Details.
        /// </summary>
        public string OperatingSystem { get; set; }

        /// <summary>
        ///Version details.
        /// </summary>
        public string Version { get; set; }

        /// <summary>
        ///Thirdparty Information.
        /// </summary>
        public string ContainThirdPartyBits { get; set; }

        /// <summary>
        ///Signers details.
        /// </summary>
        public List<string> Signers { get; set; }

        /// <summary>
        ///File Path.
        /// </summary>
        public List<string> FilesPath { get; set; }
    }
}
