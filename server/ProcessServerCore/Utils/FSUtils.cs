using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilities for path and file system operations
    /// </summary>
    public static class FSUtils
    {
        /// <summary>
        /// Security identifier for account"Administrators"
        /// </summary>
        public static readonly SecurityIdentifier AdministratorsSid = new SecurityIdentifier("S-1-5-32-544");

        /// <summary>
        /// security identifier for account "Local System"
        /// </summary>
        public static readonly SecurityIdentifier LocalSystemSid = new SecurityIdentifier("S-1-5-18");

        /// <summary>
        /// Build long path notation of the file system entity
        /// </summary>
        /// <param name="path">Input path</param>
        /// <param name="isFile">
        /// True, if File.
        /// False, if Directory.
        /// </param>
        /// <param name="optimalPath">
        /// If true, convert to long path, if needed.
        /// If false, always force convert to long path.
        /// </param>
        /// <returns>
        /// Long path, if it applies.
        /// Input path, if it doesn't apply.
        /// </returns>
        public static string GetLongPath(string path, bool isFile, bool optimalPath)
        {
            if (path == null)
                throw new ArgumentNullException(nameof(path));

            if (!Path.IsPathRooted(path))
                throw new NotSupportedException("Long paths aren't supported for relative paths");

            // Canonicalize the path, since long paths with / aren't working
            // Ex: in DirectoryInfo - returns Path not found
            path = CanonicalizePath(path);

            return optimalPath ? EnsureExtendedPrefixIfNeeded(path, isFile) : EnsureExtendedPrefix(path);
        }

        /// <summary>
        /// Get size of files matching the given filters under a directory.
        /// Throws Aurgument exception in case directory doesn't exist.
        /// </summary>
        /// <param name="dir">Directory to enumerate</param>
        /// <param name="filters">Pattern of files to include</param>
        /// <param name="option">Search option</param>
        /// <returns>Total size of files matching the filters, in bytes.
        /// 0 in case some other exception occurs.</returns>
        public static long GetFilesSize(string dir, SearchOption option, params string[] filters)
        {
            if (dir == null)
            {
                throw new ArgumentException(
                    string.Format(CultureInfo.InvariantCulture,
                    "Directory path can't be null"));
            }

            if (filters == null)
            {
                throw new ArgumentException(
                    string.Format(CultureInfo.InvariantCulture,
                    "filters can't be null, Provide '*' if no filter is required."));
            }

            if (!Directory.Exists(dir))
            {
                throw new DirectoryNotFoundException(
                        string.Format(CultureInfo.InvariantCulture,
                                "Directory : {0} is not found",
                                dir));
            }

            long totalSize = 0;
            try
            {
                DirectoryInfo dInfo = new DirectoryInfo(dir);
                totalSize = filters.
                            SelectMany(
                                filter => dInfo.EnumerateFiles(filter, option))
                            .Sum(file => file.Length);
            }
            catch (Exception ex)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Could not retrieve total file size with pattern {0} in directory {1} exception: {2}{3}",
                    String.Join(",", filters),
                    dir,
                    Environment.NewLine,
                    ex);
            }

            return totalSize;
        }

        /// <summary>
        /// Canonicalize the path in an operating system dependant manner
        /// </summary>
        /// <param name="path">Input path</param>
        /// <returns>Canonicalized (if needed) path</returns>
        public static string CanonicalizePath(string path)
        {
            if (path == null)
                return path;

            // In case of Linux, both are same, since only / is supported
            if (Path.DirectorySeparatorChar == Path.AltDirectorySeparatorChar)
                return path;

            // In case of Windows, both are \ and / are supported in .Net but
            // in certain cases, only \ is preferred/expected.
            return path.Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);
        }

        /// <summary>
        /// Canonicalize the path in an operating system dependant manner
        /// </summary>
        /// <param name="path">Input path</param>
        /// <returns>Canonicalized (if needed) path</returns>
        public static StringBuilder CanonicalizePath(StringBuilder path)
        {
            // In case of Linux, both are same, since only / is supported
            if (Path.DirectorySeparatorChar == Path.AltDirectorySeparatorChar)
                return path;

            // In case of Windows, both are \ and / are supported in .Net but
            // in certain cases, only \ is preferred/expected.
            return path.Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);
        }

        /// <summary>
        /// Formats the device names received in the settings
        /// so that they can be used directly in mysql queries
        /// </summary>
        /// <param name="deviceName">Device name</param>
        /// <returns>Formatted device name</returns>
        public static string FormatDeviceName(string deviceName)
        {
            if (string.IsNullOrWhiteSpace(deviceName))
                return deviceName;
            StringBuilder strDeviceName = new StringBuilder(deviceName);
            strDeviceName.Replace("\\\\", "\\");
            strDeviceName.Replace("\\", "\\\\");

            return strDeviceName.ToString();
        }

        /// <summary>
        /// Gets access set only to System and Administrators group along with
        /// disabled inheritance and any inherited ACLs removed.
        /// </summary>
        /// <returns>FileSecurity object</returns>
        internal static FileSecurity GetAdminAndSystemFileSecurity()
        {
            var security = new FileSecurity();

            // Remove the inherited ACLs and prevent future inheritance
            security.SetAccessRuleProtection(isProtected: true, preserveInheritance: false);

            security.AddAccessRule(
                new FileSystemAccessRule(
                    AdministratorsSid,           // sid for account "Administrators"
                    FileSystemRights.FullControl,
                    AccessControlType.Allow));

            security.AddAccessRule(
                new FileSystemAccessRule(
                    LocalSystemSid,               // sid for account "Local System"
                    FileSystemRights.FullControl,
                    AccessControlType.Allow));

            return security;
        }

        /// <summary>
        /// Creates or overwrites a file with admin and system account permissions
        /// </summary>
        /// <param name="path">The name of file to create</param>
        public static void CreateAdminsAndSystemOnlyFile(string path)
        {
            File.Create(path, 1024, FileOptions.None, GetAdminAndSystemFileSecurity());
        }

        /// <summary>
        /// Gets access set only to System and Administrators group along with
        /// disabled inheritance and any inherited ACLs removed. All the contents
        /// inside the folder would inherit these access rules.
        /// </summary>
        /// <returns>DirectorySecurity object</returns>
        internal static DirectorySecurity GetAdminAndSystemDirSecurity()
        {
            var security = new DirectorySecurity();

            security.AddAccessRule(
                new FileSystemAccessRule(
                    AdministratorsSid,           // sid for account "Administrators"
                    FileSystemRights.FullControl,
                    InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit,
                    PropagationFlags.None,
                    AccessControlType.Allow));

            security.AddAccessRule(
                new FileSystemAccessRule(
                    LocalSystemSid,               // sid for account "Local System"
                    FileSystemRights.FullControl,
                    InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit,
                    PropagationFlags.None,
                    AccessControlType.Allow));

            // Remove the inherited ACLs and prevent future inheritance
            security.SetAccessRuleProtection(isProtected: true, preserveInheritance: false);

            return security;
        }

        /// <summary>
        /// Create a directory, which itself and its contents that will be created
        /// in future will only be accessible to Administrators and SYSTEM accounts.
        /// </summary>
        /// <param name="path">Path of the directory</param>
        /// <exception cref="ArgumentException">Throws if the directory is already present</exception>
        public static void CreateAdminsAndSystemOnlyDir(string path)
        {
            if (Directory.Exists(path))
            {
                throw new ArgumentException(
                    "Directory is already present in the given path", nameof(path));
            }

            // NOTE-SanKumar-1904: This is stricter than the inno setup PS installation. Everything
            // installed and the replication data would all be accessible only by the Admins and the
            // System account.
            Directory.CreateDirectory(path, directorySecurity: GetAdminAndSystemDirSecurity());
        }

        /// <summary>
        /// Ensure that the given file or folder is a child of the expected parent folder.
        /// This method doesn't resolve junctions, hardlinks and symlinks but only does literal
        /// path matching.
        /// </summary>
        /// <param name="itemPath">Path of the child item.</param>
        /// <param name="isFile">True, if file. False, if directory.</param>
        /// <param name="expectedParentFolder">Path of the expected parent folder.</param>
        public static void EnsureItemUnderParent(string itemPath, bool isFile, string expectedParentFolder)
        {
            if (string.IsNullOrWhiteSpace(itemPath))
                throw new ArgumentNullException(nameof(itemPath));

            if (string.IsNullOrWhiteSpace(expectedParentFolder))
                throw new ArgumentNullException(nameof(expectedParentFolder));

            if (FSUtils.IsPartiallyQualified(itemPath))
                throw new ArgumentException("Absolute path is expected", nameof(itemPath));

            if (FSUtils.IsPartiallyQualified(expectedParentFolder))
                throw new ArgumentException("Absolute path is expected", nameof(expectedParentFolder));

            if (isFile)
            {
                // File name can't end with volume or dir seperator.
                if (FSUtils.IsDirectorySeparator(itemPath[itemPath.Length - 1]) ||
                    itemPath[itemPath.Length - 1] == Path.VolumeSeparatorChar)
                {
                    throw new ArgumentException("Invalid file name", nameof(itemPath));
                }
            }
            else
            {
                // Beyond this point, both file and folders are treated the same (a child of the parent).
                // So, remove the trailing \ and / that explicitly indicate that the item is a dir.
                itemPath =
                    itemPath.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            }

            // One of the inputs could be in long path notation, so to be safe, convert both of
            // them explicitly to long path notation.
            itemPath = FSUtils.GetLongPath(itemPath, isFile: isFile, optimalPath: false);
            expectedParentFolder =
                FSUtils.GetLongPath(expectedParentFolder, isFile: isFile, optimalPath: false);

            if (expectedParentFolder == @"\\?\" ||
                string.Equals(expectedParentFolder, @"\\?\UNC\", StringComparison.OrdinalIgnoreCase))
            {
                // This edge case is necessary to be handled If the expected parent folder
                // is given as \\?\, any item path would be returned true. Even though it's a
                // valid scenario for the operation of this method, blocking this to be safe.

                throw new ArgumentException("Invalid long path", nameof(expectedParentFolder));
            }

            if (itemPath.Length == 2 &&
                itemPath[1] == Path.VolumeSeparatorChar &&
                IsValidDriveChar(itemPath[0]))
            {
                // Restore C: to \\?\C: as the previous one is ignored by
                // GetLongPath(), since it's the volume device path.
                // Converting it here for the sake of the comparison logic.
                itemPath = @"\\?\" + itemPath;
            }

            Debug.Assert(itemPath.StartsWith(@"\\?\"));
            Debug.Assert(expectedParentFolder.StartsWith(@"\\?\"));

            // https://docs.microsoft.com/en-us/dotnet/standard/io/file-path-formats?view=netframework-4.6.2#path-normalization
            // Path.GetFullPath() doesn't work canonicalize and normalize, if the long path
            // notation \\?\ is used but works, if \\.\ notation is used.
            itemPath = @"\\.\" + itemPath.Substring(4);
            expectedParentFolder = @"\\.\" + expectedParentFolder.Substring(4);

            // Canonicalize paths for string comparison and normalizes the path by removing ..
            // and . notations.
            itemPath = Path.GetFullPath(itemPath);
            expectedParentFolder = Path.GetFullPath(expectedParentFolder);

            // Recursively find the parent of the item until root path is hit
            string currParent = Directory.GetParent(itemPath)?.FullName;
            for (; currParent != null; currParent = Directory.GetParent(currParent)?.FullName)
            {
                // Comparing two folder paths by always ensuring there's a trailing slash.
                if (string.Equals(
                    Path.GetFullPath(expectedParentFolder + '\\'),
                    Path.GetFullPath(currParent + '\\'),
                    StringComparison.OrdinalIgnoreCase))
                {
                    // Success
                    return;
                }
            }

            throw new InvalidOperationException(
                $"Item : {itemPath} not present under expected parent folder : {expectedParentFolder}");
        }

#region From .Net Core

        // Code in this region are copied from https://github.com/dotnet/runtime repository.
        // https://github.com/dotnet/runtime/blob/master/src/libraries/System.Private.CoreLib/src/System/IO/PathInternal.Windows.cs

        private const string ExtendedPathPrefix = @"\\?\";
        private const string UncPathPrefix = @"\\";
        private const string UncExtendedPrefixToInsert = @"?\UNC\";

        private const int MaxShortFilePath = 260;
        private const int MaxShortDirectoryPath = 248;

        // \\?\, \\.\, \??\
        private const int DevicePrefixLength = 4;

        /// <summary>
        /// Returns true if the given character is a valid drive letter
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static bool IsValidDriveChar(char value)
        {
            return ((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z'));
        }

        private static bool EndsWithPeriodOrSpace(string path)
        {
            if (string.IsNullOrEmpty(path))
                return false;

            char c = path[path.Length - 1];
            return c == ' ' || c == '.';
        }

        /// <summary>
        /// Adds the extended path prefix (\\?\) if not already a device path, IF the path is not relative,
        /// AND the path is more than 259 characters. (> MAX_PATH + null). This will also insert the extended
        /// prefix if the path ends with a period or a space. Trailing periods and spaces are normally eaten
        /// away from paths during normalization, but if we see such a path at this point it should be
        /// normalized and has retained the final characters. (Typically from one of the *Info classes)
        /// </summary>
        private static string EnsureExtendedPrefixIfNeeded(string path, bool isFile)
        {
            // Custom added variation to handle difference between dir and file in max size of names.
            bool exceedsMaxPath = isFile ?
                path.Length >= MaxShortFilePath :
                path.Length >= MaxShortDirectoryPath;

            if (path != null && (exceedsMaxPath || EndsWithPeriodOrSpace(path)))
            {
                return EnsureExtendedPrefix(path);
            }
            else
            {
                return path;
            }
        }

        /// <summary>
        /// Adds the extended path prefix (\\?\) if not relative or already a device path.
        /// </summary>
        private static string EnsureExtendedPrefix(string path)
        {
            // Putting the extended prefix on the path changes the processing of the path. It won't get normalized, which
            // means adding to relative paths will prevent them from getting the appropriate current directory inserted.

            // If it already has some variant of a device path (\??\, \\?\, \\.\, //./, etc.) we don't need to change it
            // as it is either correct or we will be changing the behavior. When/if Windows supports long paths implicitly
            // in the future we wouldn't want normalization to come back and break existing code.

            // In any case, all internal usages should be hitting normalize path (Path.GetFullPath) before they hit this
            // shimming method. (Or making a change that doesn't impact normalization, such as adding a filename to a
            // normalized base path.)
            if (IsPartiallyQualified(path) || IsDevice(path))
                return path;

            // Given \\server\share in longpath becomes \\?\UNC\server\share
            if (path.StartsWith(UncPathPrefix, StringComparison.OrdinalIgnoreCase))
                return path.Insert(2, UncExtendedPrefixToInsert);

            return ExtendedPathPrefix + path;
        }

        /// <summary>
        /// Returns true if the path uses any of the DOS device path syntaxes. ("\\.\", "\\?\", or "\??\")
        /// </summary>
        private static bool IsDevice(string path)
        {
            // If the path begins with any two separators is will be recognized and normalized and prepped with
            // "\??\" for internal usage correctly. "\??\" is recognized and handled, "/??/" is not.
            return IsExtended(path)
                ||
                (
                    path.Length >= DevicePrefixLength
                    && IsDirectorySeparator(path[0])
                    && IsDirectorySeparator(path[1])
                    && (path[2] == '.' || path[2] == '?')
                    && IsDirectorySeparator(path[3])
                );
        }

        /// <summary>
        /// Returns true if the path uses the canonical form of extended syntax ("\\?\" or "\??\"). If the
        /// path matches exactly (cannot use alternate directory separators) Windows will skip normalization
        /// and path length checks.
        /// </summary>
        private static bool IsExtended(string path)
        {
            // While paths like "//?/C:/" will work, they're treated the same as "\\.\" paths.
            // Skipping of normalization will *only* occur if back slashes ('\') are used.
            return path.Length >= DevicePrefixLength
                && path[0] == '\\'
                && (path[1] == '\\' || path[1] == '?')
                && path[2] == '?'
                && path[3] == '\\';
        }

        /// <summary>
        /// Returns true if the path specified is relative to the current drive or working directory.
        /// Returns false if the path is fixed to a specific drive or UNC path.  This method does no
        /// validation of the path (URIs will be returned as relative as a result).
        /// </summary>
        /// <remarks>
        /// Handles paths that use the alternate directory separator.  It is a frequent mistake to
        /// assume that rooted paths (Path.IsPathRooted) are not relative.  This isn't the case.
        /// "C:a" is drive relative- meaning that it will be resolved against the current directory
        /// for C: (rooted, but relative). "C:\a" is rooted and not relative (the current directory
        /// will not be used to modify the path).
        /// </remarks>
        public static bool IsPartiallyQualified(string path)
        {
            if (path.Length < 2)
            {
                // It isn't fixed, it must be relative.  There is no way to specify a fixed
                // path with one character (or less).
                return true;
            }

            if (IsDirectorySeparator(path[0]))
            {
                // There is no valid way to specify a relative path with two initial slashes or
                // \? as ? isn't valid for drive relative paths and \??\ is equivalent to \\?\
                return !(path[1] == '?' || IsDirectorySeparator(path[1]));
            }

            // The only way to specify a fixed path that doesn't begin with two slashes
            // is the drive, colon, slash format- i.e. C:\
            return !((path.Length >= 3)
                && (path[1] == Path.VolumeSeparatorChar)
                && IsDirectorySeparator(path[2])
                // To match old behavior we'll check the drive character for validity as the path is technically
                // not qualified if you don't have a valid drive. "=:\" is the "=" file's default data stream.
                && IsValidDriveChar(path[0]));
        }

        /// <summary>
        /// True if the given character is a directory separator.
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static bool IsDirectorySeparator(char c)
        {
            return c == Path.DirectorySeparatorChar || c == Path.AltDirectorySeparatorChar;
        }

#endregion From .Net Core

        /// <summary>
        /// Get the latest file mathcing the given pattern.
        /// Throws Aurgument exception in case directory doesn't exist.
        /// </summary>
        /// <param name="dir">Directory to enumerate.</param>
        /// <param name="filters">Pattern of files to include.</param>
        /// <param name="option">Search option.</param>
        /// <returns>File info for matching file.</returns>
        public static FileInfo GetLatestFile(string dir, SearchOption option, params string[] filters)
        {
            if (dir == null)
            {
                throw new ArgumentException(
                    string.Format(CultureInfo.InvariantCulture,
                    "Directory path can't be null"));
            }

            if (filters == null)
            {
                throw new ArgumentException(
                    string.Format(CultureInfo.InvariantCulture,
                    "pattern can't be null, Provide '*' if no pattern is to be matched."));
            }

            if (!Directory.Exists(dir))
            {
                throw new DirectoryNotFoundException(
                        string.Format(CultureInfo.InvariantCulture,
                                "Directory : {0} is not found",
                                dir));
            }

            try
            {
                return GetTopFile(dInfo: new DirectoryInfo(dir), OrderByAscending: false, option: option, filters: filters);
            }
            catch (Exception ex)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Could not find files with pattern {0} in directory {1} exception: {2}{3}",
                    String.Join(",", filters),
                    dir,
                    Environment.NewLine,
                    ex);
                return null;
            }
        }

        /// <summary>
        /// Get the oldest file mathcing the given pattern.
        /// Throws Aurgument exception in case directory doesn't exist.
        /// </summary>
        /// <param name="dir">Directory to enumerate.</param>
        /// <param name="filters">Pattern of files to inlcude.</param>
        /// <param name="option">Search option.</param>
        /// <returns>File info for matching file.</returns>
        public static FileInfo GetOldestFile(string dir, SearchOption option, params string[] filters)
        {
            if (dir == null)
            {
                throw new ArgumentException(
                    string.Format(CultureInfo.InvariantCulture,
                    "Directory path can't be null"));
            }

            if (filters == null)
            {
                throw new ArgumentException(
                    string.Format(CultureInfo.InvariantCulture,
                    "filters can't be null, Provide '*' if no pattern is to be matched."));
            }

            if (!Directory.Exists(dir))
            {
                throw new DirectoryNotFoundException(
                        string.Format(CultureInfo.InvariantCulture,
                                "Directory : {0} is not found",
                                dir));
            }

            try
            {
                return GetTopFile(dInfo: new DirectoryInfo(dir), OrderByAscending: true, option: option, filters: filters);
            }
            catch (Exception ex)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "[GetOldestfile]Could not find files with pattern {0} in directory {1} exception: {2}{3}",
                    String.Join(",", filters),
                    dir,
                    Environment.NewLine,
                    ex);
                return null;
            }
        }

        /// <summary>
        /// Gets newest or oldest file based on the orderBy flag
        /// </summary>
        /// <param name="dInfo">Directory info flag, make sure directory exists.</param>
        /// <param name="OrderByAscending">if true, returns newest file vice versa.</param>
        /// <param name="option">search option.</param>
        /// <param name="filters">Array of filters to be applied while searching the file.</param>
        /// <returns></returns>
        private static FileInfo GetTopFile(DirectoryInfo dInfo,
                                    bool OrderByAscending,
                                    SearchOption option,
                                    params string[] filters)
        {
            FileInfo resultFile = null;
            foreach (string filter in filters)
            {
                foreach (FileInfo file in dInfo.GetFiles(filter, option))
                {
                    if (resultFile == null ||
                        (OrderByAscending && file.LastWriteTimeUtc < resultFile.LastWriteTimeUtc) ||
                        (!OrderByAscending && file.LastWriteTimeUtc > resultFile.LastWriteTimeUtc))
                    {
                        resultFile = file;
                    }
                }
            }

            return resultFile;
        }

        /// <summary>
        /// Get all the file mathcing the given pattern.
        /// Throws Aurgument exception in case directory doesn't exist.
        /// </summary>
        /// <param name="dir">Directory to enumerate.</param>
        /// <param name="pattern">Pattern of files to inlcude.</param>
        /// <param name="option">Search option.</param>
        /// <returns>File info for all matching file.</returns>
        public static IEnumerable<FileInfo> GetFileList(string dir, string pattern, SearchOption option)
        {
            if (dir == null)
            {
                throw new ArgumentException(
                    string.Format(CultureInfo.InvariantCulture,
                    "Directory path can't be null"));
            }

            if (string.IsNullOrWhiteSpace(pattern))
            {
                throw new ArgumentException(
                    string.Format(CultureInfo.InvariantCulture,
                    "pattern can't be null, Provide '*' if no pattern is to be matched."));
            }

            if (!Directory.Exists(dir))
            {
                throw new DirectoryNotFoundException(
                        string.Format(CultureInfo.InvariantCulture,
                                "Directory : {0} is not found",
                                dir));
            }

            try
            {
                return new DirectoryInfo(dir).GetFiles(pattern, option);
            }
            catch (Exception ex)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Could not find files with pattern {0} in directory {1}, exception: {2}{3}",
                    pattern,
                    dir,
                    Environment.NewLine,
                    ex);
                return null;
            }
        }

        /// <summary>
        /// Get all the files in the dir matching the given pattern.
        /// In case of any failure or exception return empty Enumerable.
        /// Never throws any exception and always returns a value.
        /// </summary>
        /// <param name="dir">The directory to search</param>
        /// <param name="pattern">Pattern of the files to search for</param>
        /// <param name="option">Filter option</param>
        /// <returns>File Info for matching files
        /// Empty in case of failures</returns>
        public static IEnumerable<FileInfo> GetFileListIgnoreException(string dir, string pattern, SearchOption option)
        {
            IEnumerable<FileInfo> result = null;

            try
            {
                result = GetFileList(dir, pattern, option);
            }
            catch(Exception ex)
            {
                // Keeping the log level as verbose as Mars folders are frequently not present
                // for the low churning pairs, and in that case it follds the logs.
                Tracers.Misc.TraceAdminLogV2Message(
                    TraceEventType.Verbose,
                    "Ignoring exception: {0} function failed with exception {1}{2}",
                    nameof(GetFileList),
                    Environment.NewLine,
                    ex);
            }

            if(result == null)
            {
                result = Enumerable.Empty<FileInfo>();
            }

            return result;
        }

        /// <summary>
        /// Delete all the files in the directory matching the specific pattern
        /// </summary>
        /// <param name="dir">Directory name</param>
        /// <param name="pattern">Pattern of the files to delete</param>
        /// <param name="option">Search Option</param>
        public static void DeleteFileList(string dir, string pattern, SearchOption option)
        {
            GetFileList(dir, pattern, option)?.ForEach(file => File.Delete(file.FullName));
        }

        /// <summary>
        /// This routine is used to get the used and total free space of the given directory.
        /// Throws Aurgument exception in case directory doesn't exist.
        /// </summary>
        /// <param name="dir">Directory path.</param>
        /// <param name="freeBytes">Free space of cache volume in bytes.</param>
        /// <param name="totalSpace">Total Space of cache volume in bytes.</param>
        /// <returns>Free Space Percentage.
        /// -1 in case directory doesn't exist or some exception occurs.</returns>
        public static double GetFreeSpace(string dir, out ulong freeBytes, out ulong totalSpace)
        {
            double freeSpacePercent = -1;
            freeBytes = 0;
            totalSpace = 0;

            if (dir == null)
            {
                throw new ArgumentException(
                    string.Format(CultureInfo.InvariantCulture,
                    "Directory path can't be null"));
            }

            if (!Directory.Exists(dir))
            {
                throw new DirectoryNotFoundException(
                    string.Format(CultureInfo.InvariantCulture,
                    "Directory : {0} is not found", dir));
            }

            try
            {
                if (!NativeMethods.GetDiskFreeSpaceEx(
                    dir, out freeBytes, out totalSpace, out ulong totalNumOfFreeBytes))
                {
                    int lastError = Marshal.GetLastWin32Error();
                    var w = new Win32Exception(lastError);
                    Tracers.Misc.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to fetch free space of {0} folder with error {1} - {2}",
                        dir,
                        lastError,
                        w.Message);
                }
                else
                {
                    if (totalSpace != 0)
                    {
                        freeSpacePercent =
                            Math.Round(((double)freeBytes / (double)totalSpace) * 100, 2);
                    }
                }
            }
            catch (Exception ex)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to get disk freespace of {0} folder with error : {1}{2}",
                        dir,
                        Environment.NewLine,
                        ex);
            }

            return freeSpacePercent;
        }

        /// <summary>
        /// Gets the original file size for a compressed gzip file
        /// The original file size should be less than 4 GB
        /// </summary>
        /// <param name="filePath">Compressed gzip file</param>
        /// <returns>Uncompressed size of the file</returns>
        public static long GetUncompressedFileSize(string filePath)
        {
            // Todo: Check if we can perform any validations to check if
            // the file is a valid gzip file before uncompression
            if (!File.Exists(filePath))
                throw new FileNotFoundException("Given path is not a file", filePath);

            long fileSize = 0;
            try
            {
                using (FileStream fs = new FileInfo(filePath).OpenRead())
                {
                    if(fs.Length < 4)
                    {
                        throw new Exception($"The file size {fs.Length} is less than min. file size");
                    }

                    fs.Seek(-4, SeekOrigin.End);
                    int b4 = fs.ReadByte();
                    int b3 = fs.ReadByte();
                    int b2 = fs.ReadByte();
                    int b1 = fs.ReadByte();
                    fileSize = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
                }
            }
            catch(Exception ex)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to get uncompressed file size for file {0} with exception {1}",
                    filePath, ex);
            }
            return fileSize;
        }

        /// <summary>
        /// Compresses a file to gzip format
        /// </summary>
        /// <param name="originalFile">The file to be compressed</param>
        /// <param name="compressedFile">Compressed filename</param>
        /// <returns>true in case of sucess
        /// false for failures</returns>
        public static bool CompressFile(string originalFile, string compressedFile)
        {
            // compress the old file to new file
            if(!File.Exists(originalFile))
            {
                throw new FileNotFoundException("The file is not present", originalFile);
            }

            try
            {
                using (var istream = new FileInfo(originalFile).OpenRead())
                {
                    using (var ostream = File.Create(compressedFile))
                    {
                        using (GZipStream gstream = new GZipStream(ostream, CompressionMode.Compress))
                        {
                            istream.CopyTo(gstream);
                        }
                    }
                }
                // No need to flush the streams manually, its done automatically
            }
            catch(Exception ex)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to compress the file {0} to {1} with exception {2}",
                    originalFile, compressedFile, ex);
                return false;
            }
            return true;
        }

        /// <summary>
        /// Extracts a compressed gzip file
        /// </summary>
        /// <param name="compressedFile">File compressed in Gzip format</param>
        /// <param name="originalFile">Extracted file</param>
        /// <returns>true in case of success
        /// false for failures</returns>
        public static bool UncompressFile(string compressedFile, string originalFile)
        {
            if (!File.Exists(compressedFile))
            {
                throw new FileNotFoundException("The file is not present", compressedFile);
            }

            // Should there be a gzip test to ensure that the compressed file is a valid gzip file?

            try
            {
                using (var istream = new FileInfo(compressedFile).OpenRead())
                {
                    using (var ostream = File.Create(originalFile))
                    {
                        using (GZipStream gstream = new GZipStream(istream, CompressionMode.Decompress))
                        {
                            gstream.CopyTo(ostream);
                        }
                    }
                }
                // No need to flush the streams manually, its done automatically
            }
            catch (Exception ex)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to extract the file {0} to {1} with exception {2}",
                    originalFile, compressedFile, ex);
                return false;
            }
            return true;
        }

        /// <summary>
        /// Moves a file from source path to target path
        /// </summary>
        /// <param name="sourcePath">The file to be moved</param>
        /// <param name="destPath">Destination file name</param>
        /// <returns>True if the function succeedes,
        /// false in case of failures</returns>
        public static bool MoveFile(string sourcePath, string destPath)
        {
            //create hardlink from source to dest
            return TaskUtils.RunAndIgnoreException(() =>
            {
                if (string.IsNullOrWhiteSpace(sourcePath))
                    throw new ArgumentNullException(nameof(sourcePath));

                if (string.IsNullOrWhiteSpace(destPath))
                    throw new ArgumentNullException(nameof(destPath));

                if (!File.Exists(sourcePath))
                {
                    throw new FileNotFoundException("File does not exist.", sourcePath);
                }

                File.Move(sourcePath, destPath);
            });
        }

        /// <summary>
        /// Deletes a file
        /// </summary>
        /// <param name="filePath">The file to be deleted</param>
        /// <returns>True if the file is deleted</returns>
        public static bool DeleteFile(string filePath)
        {
            if (string.IsNullOrWhiteSpace(filePath))
                throw new ArgumentNullException(nameof(filePath));

            if (!File.Exists(filePath))
            {
                Tracers.Misc.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "File {0} is not found",
                        filePath);
                return true;
            }

            return TaskUtils.RunAndIgnoreException(() =>
            File.Delete(filePath), Tracers.Misc);
        }

        public static bool IsNonEmptyFile(string fileName, bool isLoggingRequired=true)
        {
            bool isValid = false;

            TaskUtils.RunAndIgnoreException(() =>
            {
                FileInfo fileInfo = new FileInfo(fileName);

                if (!fileInfo.Exists && isLoggingRequired)
                {
                    Tracers.Misc.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "File {0} is not found",
                        fileName);
                }
                else if (fileInfo.Length == 0)
                {
                    Tracers.Misc.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "File {0} is empty",
                        fileName);
                }
                else
                {
                    isValid = true;
                }
            }, Tracers.Misc);

            return isValid;
        }

        /// <summary>
        /// Deletes the files from the given collection
        /// </summary>
        /// <param name="filesToDelete">Files collection to delete</param>
        /// <param name="maxRetryCount">Maximum number of retries to attempt on failure</param>
        /// <param name="retryInterval">Interval between retries</param>
        /// <param name="traceSource">Trace source to use for logging. Pass null, if logging is not required.</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>Task of the asynchronous operation</returns>
        public static async Task<bool> DeleteFilesAsync(
            IEnumerable<FileInfo> filesToDelete,
            int maxRetryCount,
            TimeSpan retryInterval,
            int maxErrorCountToLog,
            TraceSource traceSource,
            CancellationToken ct)
        {
            if (filesToDelete == null)
            {
                throw new ArgumentNullException(nameof(filesToDelete));
            }

            var fileDelErrors = new List<string>();
            // TODO: Add parallelism to process quickly
            foreach (var file in filesToDelete)
            {
                for (int retryCnt = 0; ; retryCnt++)
                {
                    try
                    {
                        if (fileDelErrors.Count >= maxErrorCountToLog)
                        {
                            // Storing only latest maxErrorCountToLog errors and removing others.
                            fileDelErrors.RemoveRange(
                                0, (fileDelErrors.Count - maxErrorCountToLog) + 1);
                        }

                        if (file != null && file.Exists)
                        {
                            file?.Delete();
                        }

                        break;
                    }
                    catch (Exception ex)
                    {
                        if (retryCnt >= maxRetryCount)
                        {
                            traceSource?.DebugAdminLogV2Message(
                                "Failed to delete file {0} with error {1}{2}",
                                file.FullName,
                                Environment.NewLine,
                                ex);

                            fileDelErrors.Add(
                                String.Format("{0} : {1}", file.FullName, ex.Message));

                            break;
                        }

                        await Task.Delay(retryInterval, ct).ConfigureAwait(false);
                    }
                }
            }

            if (fileDelErrors.Count > 0)
            {
                traceSource?.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to delete the files even after {0} retries:" +
                    " Below are the latest error(s) {1}{2}",
                    maxRetryCount,
                    Environment.NewLine,
                    string.Join(Environment.NewLine, fileDelErrors));

                return false;
            }

            return true;
        }

        public static async Task<bool> MoveFilesMatchingPatternAsync(
            string dir,
            string pattern,
            SearchOption option,
            string originalFileName,
            int maxRetryCount,
            TimeSpan retryInterval,
            TraceSource traceSource,
            CancellationToken ct)
        {
            bool status = false;

            traceSource?.TraceAdminLogV2Message(
                TraceEventType.Information,
                "Moving {0} files under folder : {1}",
                pattern,
                dir);

            var filesToMove = GetFileList(
                dir: dir,
                pattern: pattern,
                option: option);

            foreach (var file in filesToMove)
            {
                for (int retryCnt = 0; ; retryCnt++)
                {
                    try
                    {
                        var origFile = CanonicalizePath(Path.Combine(dir, originalFileName));
                        if (file != null && File.Exists(file.FullName) && !File.Exists(origFile))
                        {
                            status = MoveFile(file.FullName, origFile);

                            if (!status)
                            {
                                throw new Exception(string.Format("Unable to rename {0}", file.FullName));
                            }

                            traceSource?.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Successfully renamed the file from {0} to {1} under the dir {2}",
                                    file.Name, originalFileName, dir);
                        }

                        break;
                    }
                    catch (Exception ex)
                    {
                        if (retryCnt >= maxRetryCount)
                            throw;

                        traceSource?.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Waiting for {0} and retrying ({1}/{2}) the rename" +
                            " of the {3} files under {4}. Latest error : {5}{6}",
                            retryInterval,
                            retryCnt + 1,
                            maxRetryCount,
                            pattern,
                            dir,
                            Environment.NewLine,
                            ex);

                        await Task.Delay(retryInterval, ct).ConfigureAwait(false);
                    }
                }
            }

            return status;
        }

        /// <summary>
        /// Converts memory in bytes to highest possible units till EB
        /// </summary>
        /// <param name="memoryInBytes">Memory in bytes</param>
        /// <returns>Memory as a string representation</returns>
        public static string GetMemoryWithHighestUnits(ulong memoryInBytes)
        {
            string[] memoryUnits = new string[] { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
            int counter = 0;
            while (memoryInBytes > 1024)
            {
                memoryInBytes /= 1024;
                counter++;
                if (counter == memoryUnits.Length - 1) break;
            }
            return memoryInBytes + " " + memoryUnits[counter];
        }

        public static void ParseFilePath(string filePath, out string parentDir, out string fileNameWithoutExt, out string fileExt)
        {
            parentDir = Path.GetDirectoryName(filePath);
            fileNameWithoutExt = Path.GetFileNameWithoutExtension(filePath);
            fileExt = Path.GetExtension(filePath);
        }

        /// <summary>
        /// Get Used, FreeSpace and TotalSpace of PS Cache Volume
        /// </summary>
        /// <param name="traceSource">TraceSource</param>
        /// <returns>Retuns UsedSpaceInBytes, FreeSpaceInKB and TotalSpaceInKB</returns>
        public static (long, ulong, ulong) GetCacheVolUsageStats(
            string rootFolderPath,
            TraceSource traceSource)
        {
            long totCacheFolSizeInBytes = 0;
            ulong freeVolSizeInKB = 0, usedVolSizeInKB = 0;

            TaskUtils.RunAndIgnoreException(() =>
            {
                double freeSpacePerct = FSUtils.GetFreeSpace(
                    rootFolderPath,
                    out ulong freeSpace,
                    out ulong totalSpace);

                var totalSpaceInKB = totalSpace / 1024;
                freeVolSizeInKB = freeSpace / 1024;
                usedVolSizeInKB = totalSpaceInKB - freeVolSizeInKB;

                totCacheFolSizeInBytes = FSUtils.GetFilesSize(
                    rootFolderPath,
                    SearchOption.AllDirectories,
                    "*.*");
            }, traceSource);

            traceSource?.TraceAdminLogV2Message(
                TraceEventType.Information,
                "Cache Folder : {0}, totalSize : {1}," +
                " freeSizeInKB : {2}, usedSpaceInKB : {3}",
                rootFolderPath, totCacheFolSizeInBytes,
                freeVolSizeInKB, usedVolSizeInKB);

            return (totCacheFolSizeInBytes, freeVolSizeInKB, usedVolSizeInKB);
        }
    }
}