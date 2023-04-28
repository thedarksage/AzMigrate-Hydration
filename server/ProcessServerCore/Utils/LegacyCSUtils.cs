using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilities for Legacy CS components
    /// </summary>
    internal static class LegacyCSUtils
    {
        /// <summary>
        /// Read stored CS passphrase
        /// </summary>
        /// <returns>CS passphrase</returns>
        public static string GetCSPassphrase()
        {
            // TODO-SanKumar-1906: It would be better to make the return value as SecureString
            return File.ReadAllText(Settings.Default.LegacyCS_FullPath_CSPassphrase, Encoding.UTF8).TrimEnd();
        }

        /// <summary>
        /// Get location of CS fingerprint file
        /// </summary>
        /// <returns>CS fingerprint file name</returns>
        public static string GetCSFingerprintFileName()
        {
            switch (PSInstallationInfo.Default.GetCSType())
            {
                case CXType.CSOnly:
                case CXType.CSAndPS:
                    return "cs.fingerprint";
                case CXType.PSOnly:
                    return string.Format(
                        CultureInfo.InvariantCulture,
                        "{0}_{1}.fingerprint",
                        AmethystConfig.Default.GetConfigurationServerIP(),
                        AmethystConfig.Default.GetConfigurationServerPort());
                default:
                    throw new NotImplementedException();
            }
        }

        /// <summary>
        /// Get PS build version string
        /// Ex: RELEASE_9.25.0.0_GA_5242_May_10_2019
        /// </summary>
        /// <returns>PS build version string</returns>
        public static string GetPSBuildVersionString()
        {
            var versionFilePath = Path.Combine(
                PSInstallationInfo.Default.GetRootFolderPath(),
                Settings.Default.LegacyCS_SubPath_Version);

            // First line of this file contains the build version string.
            using (StreamReader sr = new StreamReader(versionFilePath))
                return sr.ReadLine();
        }

        /// <summary>
        /// Get location of CS certificate file
        /// </summary>
        /// <returns>CS certificate file path</returns>
        public static string GetCSCertificateFilePath()
        {
            string certFileName;

            switch (PSInstallationInfo.Default.GetCSType())
            {
                case CXType.CSOnly:
                case CXType.CSAndPS:
                    certFileName = "cs.crt";
                    break;
                case CXType.PSOnly:
                    certFileName = string.Format(
                        CultureInfo.InvariantCulture,
                        "{0}_{1}.crt",
                        AmethystConfig.Default.GetConfigurationServerIP(),
                        AmethystConfig.Default.GetConfigurationServerPort());
                    break;
                default:
                    throw new NotImplementedException();
            }

            return Path.Combine(Settings.Default.LegacyCS_FullPath_Certs, certFileName);
        }

        public static string GetPSInstallationDate()
        {
            string psInstallDate = string.Empty;

            TaskUtils.RunAndIgnoreException(() =>
            {
                var versionFilePath = FSUtils.CanonicalizePath(Path.Combine(
                    PSInstallationInfo.Default.GetRootFolderPath(),
                    Settings.Default.LegacyCS_SubPath_Version));

                if (FSUtils.IsNonEmptyFile(versionFilePath))
                {
                    string[] lines = File.ReadAllLines(versionFilePath);

                    psInstallDate = lines[4]?.Split('=')[1].Trim();
                }
            }, Tracers.Misc);

            return psInstallDate;
        }

        public static (string, string) GetPSPatchDetails()
        {
            const string patchLog = "patch.log";

            string psPatchVersion = string.Empty;
            string psPatchInstallTime = string.Empty;

            TaskUtils.RunAndIgnoreException(() =>
            {
                var patchLogFile = FSUtils.CanonicalizePath(Path.Combine(
                    PSInstallationInfo.Default.GetRootFolderPath(),
                    patchLog));

                if (File.Exists(patchLogFile))
                {
                    string[] lines = File.ReadAllLines(patchLogFile);

                    var patchVersionList = new List<string>();
                    var patchInstallTimeList = new List<string>();
                    foreach (var linetoParse in lines)
                    {
                        TaskUtils.RunAndIgnoreException(() =>
                        {
                            if (!string.IsNullOrWhiteSpace(linetoParse))
                            {
                                var lineSplit = linetoParse.Split(',');

                                if (!string.IsNullOrWhiteSpace(lineSplit[0]))
                                {
                                    patchVersionList.Add(lineSplit[0]);
                                }

                                if (!string.IsNullOrWhiteSpace(lineSplit[1]))
                                {
                                    patchInstallTimeList.Add(lineSplit[1]);
                                }
                            }
                        }, Tracers.Misc);
                    }

                    if (patchVersionList.Count > 0)
                    {
                        psPatchVersion = string.Join(",", patchVersionList);
                    }

                    if (patchInstallTimeList.Count > 0)
                    {
                        psPatchInstallTime = string.Join(",", patchInstallTimeList);
                    }
                }
            }, Tracers.Misc);

            return (psPatchVersion, psPatchInstallTime);
        }

        /// <summary>
        /// Compute signature for component authentication at CS
        /// </summary>
        /// <param name="passphrase">Passphrase of CS</param>
        /// <param name="requestMethod">Request HTTP method</param>
        /// <param name="resource">PHP resource</param>
        /// <param name="function">PHP function</param>
        /// <param name="requestId">Request ID</param>
        /// <param name="version">Component auth version</param>
        /// <param name="fingerprint">Fingerprint of CS cert</param>
        /// <returns>HmacSha256 hash of the signature</returns>
        public static string ComputeSignatureComponentAuth(
            string passphrase,
            string requestMethod,
            string resource,
            string function,
            string requestId,
            string version,
            string fingerprint)
        {
            const char DELIMITER = ':';

            var passphraseHash = CryptoUtils.GetSha256Hash(passphrase);

            var fingerprintHmac = string.IsNullOrEmpty(fingerprint) ?
                string.Empty : CryptoUtils.GetHmacSha256Hash(fingerprint, passphraseHash);

            var stringToSignSB = new StringBuilder()
                .Append(requestMethod).Append(DELIMITER)
                .Append(resource).Append(DELIMITER)
                .Append(function).Append(DELIMITER)
                .Append(requestId).Append(DELIMITER)
                .Append(version);

            var stringToSignHmac = CryptoUtils.GetHmacSha256Hash(
                stringToSignSB.ToString(), passphraseHash);

            var signature = string.Concat(fingerprintHmac, DELIMITER, stringToSignHmac);

            return CryptoUtils.GetHmacSha256Hash(signature, passphraseHash);
        }
    }
}
