using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

using IniParserOutput = System.Collections.Generic.IReadOnlyDictionary<
    string,
    System.Collections.Generic.IReadOnlyDictionary<string, string>>;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    /// <summary>
    /// Additional options for IniParser
    /// </summary>
    [Flags]
    internal enum IniParserOptions
    {
        /// <summary>
        /// No additional option
        /// </summary>
        None = 0,
        /// <summary>
        /// Remove double quotes from value, while parsing the file.
        /// </summary>
        RemoveDoubleQuotesInValues = 1,
        /// <summary>
        /// Be careful in allowing Debug logging. Although the log will be only
        /// written in the Debug build, consider the following cases:
        /// 1. If the ini file contains any secret, avoid logging, as the logged
        /// value might end up reaching the persisted log storages, such as Kusto.
        /// 2. If the logging component is dependent on a value from the ini file
        /// to get initialized, we will incur a deadlock.
        /// </summary>
        AllowDebugLogging = 2
    }

    /// <summary>
    /// Type of Ini Row
    /// </summary>
    [Flags]
    internal enum IniRowType
    {
        CommentOrWhitespace = 1 << 0,
        KeyValue = 1 << 1,
        Section = 1 << 2,
        Unrecognized = 1 << 3
    }

    /// <summary>
    /// Parser for configuration files stored in INI format
    /// </summary>
    internal static class IniParser
    {
        /// <summary>
        /// Parse the Ini file
        /// </summary>
        /// <param name="filePath">Path of the file</param>
        /// <returns>Parsed data</returns>
        public static async Task<IniParserOutput> ParseAsync(string filePath)
        {
            using (FileStream fs = new FileStream(
                filePath, FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete))
            {
                return await ParseAsync(fs).ConfigureAwait(false);
            }
        }

        /// <summary>
        /// Parse the Ini file
        /// </summary>
        /// <param name="filePath">Path of the file</param>
        /// <param name="parserOptions">Additonal parser options</param>
        /// <returns>Parsed data</returns>
        public static async Task<IniParserOutput> ParseAsync(string filePath, IniParserOptions parserOptions)
        {
            using (FileStream fs = new FileStream(
                filePath, FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete))
            {
                return await ParseAsync(fs, parserOptions).ConfigureAwait(false);
            }
        }

        /// <summary>
        /// Parse the Ini file
        /// </summary>
        /// <param name="inputStream">Stream containing input text</param>
        /// <returns>Parsed data</returns>
        public static Task<IniParserOutput> ParseAsync(Stream inputStream)
        {
            return ParseAsync(inputStream, IniParserOptions.None);
        }

        /// <summary>
        /// Parse the Ini file
        /// </summary>
        /// <param name="inputStream">Stream containing input text</param>
        /// <param name="parserOptions">Additonal parser options</param>
        /// <returns>Parsed data</returns>
        public static async Task<IniParserOutput> ParseAsync(
            Stream inputStream, IniParserOptions parserOptions)
        {
            return
                (IReadOnlyDictionary<string, IReadOnlyDictionary<string, string>>)
                await ParseInternal(inputStream, parserOptions, tokenize: false).ConfigureAwait(false);
        }

        /// <summary>
        /// Tokenize the Ini file
        /// </summary>
        /// <param name="inputStream">Stream containing input text</param>
        /// <param name="parserOptions">Additonal parser options</param>
        /// <returns>Asynchronous task with tokenized output</returns>
        public static async
            Task<List<(IniRow, LinkedList<IniRow>)>>
            TokenizeAsync(Stream inputStream, IniParserOptions parserOptions)
        {
            return
                (List<(IniRow, LinkedList<IniRow>)>)
                await ParseInternal(
                    inputStream, parserOptions, tokenize: true).ConfigureAwait(false);
        }

        private static async Task<object> ParseInternal(
            Stream inputStream, IniParserOptions parserOptions, bool tokenize)
        {
            Dictionary<string, Dictionary<string, string>> kvDict = null;
            List<(IniRow section, LinkedList<IniRow> keyValues)> tokenizedRows = null;

            if (tokenize)
            {
                tokenizedRows = new List<(IniRow section, LinkedList<IniRow> keyValues)>();
            }
            else
            {
                kvDict = new Dictionary<string, Dictionary<string, string>>();
            }

            // TODO-SanKumar-1903: Make buffer size as a setting
            using (StreamReader strmReader =
                new StreamReader(inputStream, Encoding.UTF8, true, 1024, leaveOpen: true))
            {
                int currLineNo = 0;
                string currLine;
                string currSectionName = string.Empty;
                LinkedList<IniRow> currTokenizedSectionKVs = null;

                if (tokenize)
                {
                    currTokenizedSectionKVs = new LinkedList<IniRow>();
                    var currTokenizedSection = IniRow.BuildIniRow(
                        IniRowType.Section, rawData: string.Empty, sectionOrKeyName: currSectionName, value: null);
                    tokenizedRows.Add((currTokenizedSection, currTokenizedSectionKVs));
                }
                else
                {
                    kvDict.Add(currSectionName, new Dictionary<string, string>());
                }

                // Anything that follows the # or ; is a comment. Preceding and
                // trailing whitespaces are ignored on tokenizing a comment. An
                // empty comment is fine too.
                const string commentRegex = @"(?<CommentBody>(#|;)\s*(?<Comment>(.*\S+))?)";

                // A section is enclosed between the square brackets [ ]. The
                // preceding and trailing whitespaces are ignored. A section
                // name can have whitespaces within itself but must have at least
                // one non-whitespace character.
                const string sectionRegex = @"\[\s*(?<SectionName>[^\[\]=#;]*[^\[\]=#;\s]+)\s*\]";

                // The value can contain equal symbol(s). In a given line, the
                // key is always the content to the left of the first equal
                // symbol (greedy match). The keys and values can contain
                // whitespaces within them. Any preceding or trailing whitespace
                // will be ignored. A key must have at least one non-whitespace
                // character, which is not the requirement for the value. The
                // value can contain special characters such as =, [ and ].
                const string keyValueRegex = @"(?<Key>[^\[\]=#;]*[^\[\]=#;\s]+)\s*=\s*(?<Value>[^#;]*[^#;\s]+)?";

                // TODO-SanKumar-1903: Is async really needed here?!
                while ((currLine = await strmReader.ReadLineAsync().ConfigureAwait(false)) != null)
                {
                    currLineNo++;

                    // Case-sensitive, single line mode, culture invariant,
                    // pattern whitespaces are explicit, explicit capture
                    // compiled for optimized repeated usage
                    const RegexOptions options =
                        RegexOptions.Singleline | RegexOptions.Compiled | RegexOptions.ExplicitCapture;

                    // Regex explanation:
                    // 1. Anchored from beginning of the line till \n (or) end of line
                    // 2. Can contain leading whitespace
                    // 3. Optionally, there could be a comment after the possible whitespaces
                    // 4. Comment could either start with # or ;
                    MatchCollection matches =
                        Regex.Matches(currLine, @"^\s*" + commentRegex + @"?\s*$", options);

                    if (matches.Count != 0)
                    {
                        var groups = matches[0].Groups;
                        if (matches.Count > 1 || groups.Count != 3)
                        {
                            Debug.Assert(false);
                            // TODO-SanKumar-1903: Add warning log to Kusto for bad parsing
                        }

                        var commentBodyGroup = groups["CommentBody"];
                        var commentGroup = groups["Comment"];

                        if (commentBodyGroup.Success)
                        {
                            Log(parserOptions,
                                "Line no: {0}\t|{1}| - Comment : {2}({3})",
                                currLineNo, currLine, commentGroup.Value, commentGroup.Value.Length);
                        }
                        else
                        {
                            Log(parserOptions,
                                "Line no: {0}\t|{1}| - Whitespace",
                                currLineNo, currLine);
                        }

                        if (tokenize)
                        {
                            currTokenizedSectionKVs.AddLast(
                                IniRow.BuildIniRow(IniRowType.CommentOrWhitespace, currLine, null, null));
                        }

                        continue;
                    }

                    matches = Regex.Matches(
                        currLine, @"^\s*" + sectionRegex + @"\s*" + commentRegex + @"?\s*$", options);

                    if (matches.Count != 0)
                    {
                        var groups = matches[0].Groups;
                        if (matches.Count > 1 || groups.Count != 4)
                        {
                            Debug.Assert(false);
                            // TODO-SanKumar-1903: Add warning log to Kusto for bad parsing
                        }

                        var sectionNameGroup = groups["SectionName"];
                        var commentBodyGroup = groups["CommentBody"];
                        var commentGroup = groups["Comment"];

                        currSectionName = sectionNameGroup.Value;

                        if (string.IsNullOrWhiteSpace(currSectionName))
                        {
                            Debug.Assert(false);
                            // TODO-SanKumar-1903: Add warning log to Kusto for bad parsing
                        }

                        if (tokenize)
                        {
                            currTokenizedSectionKVs = new LinkedList<IniRow>();
                            var currTokenizedSection = IniRow.BuildIniRow(
                                IniRowType.Section, rawData: currLine, sectionOrKeyName: currSectionName, value: null);
                            tokenizedRows.Add((currTokenizedSection, currTokenizedSectionKVs));
                        }
                        else
                        {
                            // TODO-SanKumar-1909: We should fail on detecting repeated sections?
                            if (!kvDict.ContainsKey(currSectionName))
                                kvDict.Add(currSectionName, new Dictionary<string, string>());
                        }

                        if (commentBodyGroup.Success)
                        {
                            Log(parserOptions,
                                "Line no: {0}\t|{1}| - Section: {2}({3}) Comment : {4}({5})",
                                currLineNo, currLine,
                                currSectionName, currSectionName.Length,
                                commentGroup.Value, commentGroup.Value.Length);
                        }
                        else
                        {
                            Log(parserOptions,
                                "Line no: {0}\t|{1}| - Section: {2}({3})",
                                currLineNo, currLine,
                                currSectionName, currSectionName.Length);
                        }

                        continue;
                    }

                    matches = Regex.Matches(
                        currLine, @"^\s*" + keyValueRegex + @"\s*" + commentRegex + @"?\s*$", options);

                    if (matches.Count != 0)
                    {
                        var groups = matches[0].Groups;
                        if (matches.Count > 1 || groups.Count != 5)
                        {
                            Debug.Assert(false);
                            // TODO-SanKumar-1903: Add warning log to Kusto for bad parsing
                        }

                        var keyGroup = groups["Key"];
                        var valueGroup = groups["Value"];
                        var commentBodyGroup = groups["CommentBody"];
                        var commentGroup = groups["Comment"];

                        string currKey = keyGroup.Value;
                        bool hasValue = valueGroup.Success;
                        string currValue = hasValue ? valueGroup.Value : null;

                        if ((string.IsNullOrWhiteSpace(currKey)) ||
                            (hasValue &&
                             string.IsNullOrWhiteSpace(currValue)))
                        {
                            Debug.Assert(false);
                            // TODO-SanKumar-1903: Add warning log to Kusto for bad parsing
                        }

                        if (parserOptions.HasFlag(IniParserOptions.RemoveDoubleQuotesInValues) &&
                            hasValue &&
                            currValue.Length >= 2 &&
                            currValue[0] == '"' && currValue[currValue.Length - 1] == '"')
                        {
                            currValue = currValue.Substring(1, currValue.Length - 2);

                            if (string.IsNullOrWhiteSpace(currValue))
                            {
                                hasValue = false;
                                currValue = null;
                            }
                        }

                        if (tokenize)
                        {
                            currTokenizedSectionKVs.AddLast(
                                IniRow.BuildIniRow(IniRowType.KeyValue, currLine, currKey, currValue));
                        }
                        else
                        {
                            kvDict[currSectionName][currKey] = currValue;
                        }

                        if (commentBodyGroup.Success)
                        {
                            Log(parserOptions,
                                "Line no: {0}\t|{1}| - Key: {2}({3}) Value: {4}({5}) Section: {6}({7}) Comment : {8}({9})",
                                currLineNo, currLine,
                                currKey, currKey.Length,
                                currValue, hasValue ? currValue.Length : 0,
                                currSectionName, currSectionName.Length,
                                commentGroup.Value, commentGroup.Value.Length);
                        }
                        else
                        {
                            Log(parserOptions,
                                "Line no: {0}\t|{1}| - Key: {2}({3}) Value: {4}({5}) Section: {6}({7})",
                                currLineNo, currLine,
                                currKey, currKey.Length,
                                currValue, hasValue ? currValue.Length : 0,
                                currSectionName, currSectionName.Length);
                        }

                        continue;
                    }

                    // if illegible, continue - section name must not be empty.

                    // TODO-SanKumar-1903: Add warning log to Kusto for illegible char

                    Log(parserOptions, "Line no: {0}\t|{1}| - Unknown", currLineNo, currLine);

                    if (tokenize)
                    {
                        currTokenizedSectionKVs.AddLast(
                            IniRow.BuildIniRow(IniRowType.Unrecognized, currLine, null, null));
                    }
                }
            }

            // Recreating the same dictionary, since the value generic type in
            // Dictionary class doesn't support Contravariance.
            if (tokenize)
            {
                return tokenizedRows;
            }
            else
            {
	            return kvDict.ToDictionary(
                    keySelector: currKVPair => currKVPair.Key,
                    elementSelector: currKVPair => (IReadOnlyDictionary<string, string>) currKVPair.Value);
            }
        }

        [Conditional("DEBUG")]
        private static void Log(IniParserOptions options, string format, params object[] args)
        {
            // NOTE-SanKumar-1902: Ensure that this doesn't ship in Release bits,
            // otherwise the configuration values could be uploaded to Kusto.
            // Also, allow debugging must be only performed for configs on which
            // any other object in the call stack is not dependent upon.
            // For example: App config and Amethyst config must never pass this
            // flag, otherwise there's a great chance of a deadlock
            if (options.HasFlag(IniParserOptions.AllowDebugLogging))
            {
                if (Tracers.Misc != null)
                    Tracers.Misc.DebugAdminLogV2Message(format, args);
            }
        }
    }
}
