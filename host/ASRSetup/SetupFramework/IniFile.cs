//---------------------------------------------------------------
//  <copyright file="IniFile.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Class to allowing interaction with ini files.
//  Picked and enhanced from www.codeproject.com/Articles/1966/An-INI-file-handling-class-using-C.
//  </summary>
//
//  History:     05-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace ASRSetupFramework
{
    /// <summary>
    /// Create a New INI file to store or load data
    /// </summary>
    public class IniFile
    {
        public string path;

        #region External Kernel 32 Imports.
        [DllImport("kernel32")]
        private static extern long WritePrivateProfileString(
            string section,
            string key,
            string val,
            string filePath);

        [DllImport("kernel32")]
        private static extern int GetPrivateProfileString(
            string section,
            string key,
            string def,
            StringBuilder retVal,
            int size,
            string filePath);
        #endregion

        /// <summary>
        /// Initializes a new instance of the <see cref="IniFile"/> class.
        /// </summary>
        /// <PARAM name="filePath">Absolute or relative path.</PARAM>
        public IniFile(string filePath)
        {
            path = Path.GetFullPath(filePath);
        }

        /// <summary>
        /// Writes value to the ini file.
        /// </summary>
        /// <param name="section">Ini file section.</param>
        /// <param name="key">Key under the section.</param>
        /// <param name="value">Value to be set for key.</param>
        public void WriteValue(string section, string key, string value)
        {
            Trc.Log(
                LogLevel.Always,
                string.Format(
                    "Writing to INI file Section '{0}', Key '{1}', Value '{2}'. File Path '{3}'",
                    section,
                    key,
                    value,
                    this.path));
            WritePrivateProfileString(section, key, value, this.path);
        }

        /// <summary>
        /// Reads data from given key under a section.
        /// </summary>
        /// <param name="section">Ini file section.</param>
        /// <param name="key">Ini file key.</param>
        /// <returns>Value for the key under section, returns empty string if section or key did not exist.</returns>
        public string ReadValue(string section, string key)
        {
            Trc.Log(
                LogLevel.Always,
                string.Format(
                    "Readering from INI file Section '{0}', Key '{1}'. File Path '{2}'",
                    section,
                    key,
                    this.path));

            StringBuilder temp = new StringBuilder(255);
            int i = GetPrivateProfileString(
                section, 
                key, 
                "", 
                temp,
                255, 
                this.path);
            return temp.ToString();
        }

        /// <summary>
        /// Removes a given key from a section.
        /// </summary>
        /// <param name="section">Section to remove key from.</param>
        /// <param name="key">Key to remove.</param>
        public void RemoveKey(string section, string key)
        {
            Trc.Log(
                LogLevel.Always,
                string.Format(
                    "Removing key from INI file Section '{0}', Key '{1}'. File Path '{2}'",
                    section,
                    key,
                    this.path));
            WritePrivateProfileString(section, key, null, this.path);
        }

        /// <summary>
        /// Removes a given section from ini file.
        /// </summary>
        /// <param name="section">Section to remove.</param>
        public void RemoveSection(string section)
        {
            Trc.Log(
                LogLevel.Always,
                string.Format(
                    "Removing Section from INI file, Section '{0}'. File Path '{1}'",
                    section,
                    this.path));
            WritePrivateProfileString(section, null, null, this.path);
        }
    }
}
