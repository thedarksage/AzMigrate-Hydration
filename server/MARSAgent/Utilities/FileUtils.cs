using LoggerInterface;
using MarsAgent.LoggerInterface;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MarsAgent.Utilities
{
    public class FileUtils
    {
        /// <summary>
        /// Get version number of the given file
        /// </summary>
        /// <param name="filePath">Path of the file</param>
        /// <returns>Version number of the file</returns>
        public static string GetFileVersion(string filePath)
        {
            string fileVersion = string.Empty;

            try
            {
                if (File.Exists(filePath))
                {
                    fileVersion = FileVersionInfo.GetVersionInfo(
                        filePath).ProductVersion;
                }
            }
            catch (Exception ex)
            {
                Logger.Instance.LogException(
                    CallInfo.Site(), ex, $"Failed to fetch '{filePath}' version.");
            }

            return fileVersion;
        }
    }
}
