using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    /// <summary>
    /// Edits Ini file
    /// </summary>
    internal sealed class IniEditor : IDisposable
    {
        private readonly List<(IniRow section, LinkedList<IniRow> keyValues)> m_rows;
        private readonly string m_iniFilePath;
        private readonly FileApi.LockFile m_lockFile;

        /// <summary>
        /// IniEditor constructor
        /// </summary>
        /// <param name="iniFilePath">Ini file path</param>
        /// <param name="lockFilePath">Path of lock file to acquire exclusive lock</param>
        /// <param name="parserOptions">Additional parser options</param>
        public IniEditor(string iniFilePath, string lockFilePath, IniParserOptions parserOptions)
        {
            m_iniFilePath = iniFilePath;

            // This lock file would be held, until the IniEditor object is destroyed
            if (lockFilePath != null)
            {
                m_lockFile = FileUtils.AcquireLockFile(lockFilePath, exclusive: true, appendLckExt: false);
            }

            using (var iniStream = new FileStream(m_iniFilePath, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                m_rows = IniParser.TokenizeAsync(iniStream, parserOptions).GetAwaiter().GetResult();

                // TODO-SanKumar-1904: Add logger for this file, only if the
                // Debug logging flag is set. Care must be taken to not to log
                // in production, since there could be secrets in the file.

                //foreach (var currRowColl in m_rows)
                //{
                //    var currSection = currRowColl.section;
                //    Console.WriteLine($"{currSection.RawData} | {currSection.Dirty} | {currSection.IniRowType} | {currSection.SectionOrKeyName} | {currSection.Value}");

                //    foreach (var currKv in currRowColl.keyValues)
                //    {
                //        Console.WriteLine($"{currKv.RawData} | {currKv.Dirty} | {currKv.IniRowType} | {currKv.SectionOrKeyName} | {currKv.Value}");
                //    }
                //}
            }
        }

        /// <summary>
        /// Add or edit a key-value pair in the Ini file
        /// </summary>
        /// <param name="section">Section name</param>
        /// <param name="key">Key name</param>
        /// <param name="value">Value</param>
        /// <param name="includeDoubleQuotes">If yes, wraps value with double quotes</param>
        /// <returns>Old value, if the key-value pair already existed. Otherwise, null.</returns>
        public string AddOrEditValue(
            string section, string key, string value, bool includeDoubleQuotes)
        {
            // TODO-SanKumar-1909: If duplicate keys are present, we will edit the last row in the
            // Ini file. Should we rather fail?

            // TODO-SanKumar-1904: Get a flag to honor case sensitivity

            var matchingSections = m_rows.
                Where(currSection => currSection.section.SectionOrKeyName == section).ToList();

            var lastMatchingRow =
                matchingSections.
                    SelectMany(tuple => tuple.keyValues).
                    Where(currRow => currRow.IniRowType == IniRowType.KeyValue && currRow.SectionOrKeyName == key).
                    LastOrDefault();

            if (lastMatchingRow != null)
            {
                return lastMatchingRow.EditValue(value, includeDoubleQuotes);
            }

            var newKVRow = IniRow.CreateIniKeyValue(key, value, includeDoubleQuotes);
            LinkedList<IniRow> rowListToAppend;

            if (matchingSections.Count > 0)
            {
                rowListToAppend = matchingSections[matchingSections.Count - 1].keyValues;
            }
            else
            {
                Debug.Assert(section != string.Empty);

                var newSectionRow = IniRow.CreateIniSection(section);
                var emptyKVRowList = new LinkedList<IniRow>();
                m_rows.Add((newSectionRow, emptyKVRowList));

                rowListToAppend = emptyKVRowList;
            }

            rowListToAppend.AddLast(newKVRow);

            return null; // No old value, since this key is newly introduced
        }

        /// <summary>
        /// Save the edited Ini file
        /// </summary>
        /// <param name="flush">If true, persists the file with flush</param>
        /// <param name="maintainAllBackup">If true, past version of the file is stored permanently in the Backup folder</param>
        /// <returns>Asynchronous task</returns>
        public Task SaveIniFileAsync(bool flush, bool maintainAllBackup)
        {
            async Task WriteDataToIniAsync(StreamWriter sw)
            {
                foreach (var (section, keyValues) in m_rows)
                {
                    // Empty section is implicit. Don't print it.
                    if (section.SectionOrKeyName != string.Empty)
                    {
                        // TODO-SanKumar-1904: Empty section may or may not come
                        // but only in the beginning. So, assert that it doesn't
                        // come for the second time (or) after another section
                        // for the first time.

                        await sw.WriteLineAsync(section.RawData).ConfigureAwait(false);
                    }

                    foreach (IniRow currRow in keyValues)
                    {
                        await sw.WriteLineAsync(currRow.RawData).ConfigureAwait(false);
                    }
                }
            }

            if (maintainAllBackup)
            {
                return FileUtils.ReliableSaveFileAsync(
                    filePath: m_iniFilePath,
                    backupFolderPath: Path.Combine(Path.GetDirectoryName(m_iniFilePath), "Backup"),
                    multipleBackups: true,
                    writeDataAsync: WriteDataToIniAsync,
                    flush: flush);
            }
            else
            {
                return FileUtils.ReliableSaveFileAsync(
                    filePath: m_iniFilePath,
                    backupFolderPath: null,
                    multipleBackups: false,
                    writeDataAsync: WriteDataToIniAsync,
                    flush: flush);
            }
        }

        public void Dispose()
        {
            // TODO-SanKumar-1909: Implement full dispose pattern? But why??

            m_lockFile?.Dispose();

            GC.SuppressFinalize(this);
        }
    }
}
