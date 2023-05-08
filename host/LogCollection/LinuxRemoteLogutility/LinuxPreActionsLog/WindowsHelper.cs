/*++

Copyright (c) 2014  Microsoft Corporation

Module Name:

WindowsHelper.cs

Abstract:

This file provides the basic helper function to related to Windows Operating System

Author:

Dharmendra Modi(dmodi) 01-03-2014

--*/

using System;
using System.Globalization;
using System.IO;

namespace LinuxCommunicationFramework
{
    #region Windows Helper
    internal static class WindowsHelper
    {
        /// <summary>
        /// Checks whether a File or Directory exists at path
        /// </summary>
        /// <param name="path">Path to check fil/directory existence</param>
        /// <returns>true if exists else false</returns>
        public static bool DoesFileOrDirectoryExists(string path)
        {
            if (DoesFileExists(path) || DoesDirExists(path))
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        /// <summary>
        /// Creates all directories in the specified path
        /// </summary>
        /// <param name="path">path to create directory</param>
        public static void CreateDirectory(string path)
        {
            if (!DoesFileOrDirectoryExists(path))
            {
                Directory.CreateDirectory(path);
            }
        }

        /// <summary>
        /// Validates file existence in the filePath
        /// </summary>
        /// <param name="filePath"></param>
        public static void ValidateFilePath(string filePath)
        {
            if (!File.Exists(filePath))
            {
                throw new FileNotFoundException(string.Format(CultureInfo.InvariantCulture,
                    "File {0} Not available on host {1}", filePath, Environment.MachineName));
            }
        }

        /// <summary>
        /// Validates directory existence in the dirPath
        /// </summary>
        /// <param name="dirPath"></param>
        public static void ValidateDirPath(string dirPath)
        {
            if (!Directory.Exists(dirPath))
            {
                throw new DirectoryNotFoundException(string.Format(CultureInfo.InvariantCulture,
                    "Directory {0} Not available on host {1}", dirPath, Environment.MachineName));
            }
        }

        /// <summary>
        /// Checks if source is a file
        /// </summary>
        /// <param name="source"></param>
        public static bool DoesFileExists(string source)
        {
            return File.Exists(source);
        }

        /// <summary>
        /// Checks if source is directory
        /// </summary>
        /// <param name="source"></param>
        public static bool DoesDirExists(string source)
        {
            return Directory.Exists(source);
        }

        /// <summary>
        /// Returns the parent directory of source
        /// </summary>
        /// <param name="source"></param>
        /// <returns></returns>
        public static string GetParentDir(string source)
        {
            return Directory.GetParent(source).ToString();
        }
    }
    #endregion Windows Helper
}
