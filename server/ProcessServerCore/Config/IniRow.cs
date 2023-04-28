using System;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    /// <summary>
    /// Represent a tokenized row in the IniParser
    /// </summary>
    internal class IniRow
    {
        /// <summary>
        /// Row type
        /// </summary>
        public IniRowType IniRowType { get; private set; }

        /// <summary>
        /// Raw text that's tokenized
        /// </summary>
        public string RawData { get; private set; }

        /// <summary>
        /// If <see cref="IniRowType"/> is <see cref="IniRowType.Section"/>, contains section name.
        /// If <see cref="IniRowType"/> is <see cref="IniRowType.KeyValue"/>, contains key name.
        /// Otherwise, null.
        /// </summary>
        public string SectionOrKeyName { get; private set; }

        /// <summary>
        /// If <see cref="IniRowType"/> is <see cref="IniRowType.KeyValue"/>, contains the value.
        /// Otherwise, null.
        /// </summary>
        public string Value { get; private set; }

        /// <summary>
        /// If true, this object has been edited after parsed / it's newly created.
        /// </summary>
        public bool Dirty { get; private set; }

        // TODO-SanKumar-1904: Should we include Line number? But on editing the
        // Ini file, it becomes tricky and it's better, if it's not there.


        private IniRow() { }

        /// <summary>
        /// Build an Ini row, while tokenizing.
        /// </summary>
        /// <param name="iniRowType">Row type</param>
        /// <param name="rawData">Raw string of this row</param>
        /// <param name="sectionOrKeyName">
        /// If <paramref name="iniRowType"/> is <see cref="IniRowType.Section"/>, should contain section name.
        /// If <paramref name="iniRowType"/> is <see cref="IniRowType.KeyValue"/>, should contain key name.
        /// Otherwise, unused.
        /// </param>
        /// <param name="value">
        /// If <paramref name="iniRowType"/> is <see cref="IniRowType.KeyValue"/>, should contain the value.
        /// Otherwise, unused.
        /// </param>
        /// <returns>Built IniRow object</returns>
        public static IniRow BuildIniRow(
            IniRowType iniRowType, string rawData, string sectionOrKeyName, string value)
        {
            var toRet = new IniRow
            {
                IniRowType = iniRowType,
                RawData = rawData,
                Dirty = false
            };

            switch (iniRowType)
            {
                case IniRowType.CommentOrWhitespace:
                case IniRowType.Unrecognized:
                case IniRowType.KeyValue:
                    toRet.SectionOrKeyName = sectionOrKeyName;
                    toRet.Value = value;
                    break;
                case IniRowType.Section:
                    toRet.SectionOrKeyName = sectionOrKeyName;
                    break;
                default:
                    throw new NotImplementedException($"{iniRowType}");
            }

            return toRet;
        }

        /// <summary>
        /// Create a section
        /// </summary>
        /// <param name="sectionName">Name of the section</param>
        /// <returns>Built IniRow object</returns>
        public static IniRow CreateIniSection(string sectionName)
        {
            return new IniRow
            {
                IniRowType = IniRowType.Section,
                RawData = $"[{sectionName}]",
                SectionOrKeyName = sectionName,
                Value = null,
                Dirty = true
            };
        }

        /// <summary>
        /// Create a key-value pair
        /// </summary>
        /// <param name="key">Key</param>
        /// <param name="value">Value</param>
        /// <param name="includeDoubleQuotes">If true, wrap value with double quotes in the raw text</param>
        /// <returns>Built IniRow object</returns>
        public static IniRow CreateIniKeyValue(string key, string value, bool includeDoubleQuotes)
        {
            return new IniRow
            {
                IniRowType = IniRowType.KeyValue,
                RawData = $"{key} = {(includeDoubleQuotes ? "\"" : "")}{value}{(includeDoubleQuotes ? "\"" : "")}",
                SectionOrKeyName = key,
                Value = value,
                Dirty = true
            };
        }

        /// <summary>
        /// Edit value of this object
        /// </summary>
        /// <param name="value">New value</param>
        /// <param name="includeDoubleQuotes">If true, wrap value with double quotes in the raw text</param>
        /// <returns>Old value</returns>
        public string EditValue(string value, bool includeDoubleQuotes)
        {
            if (this.IniRowType != IniRowType.KeyValue)
            {
                throw new InvalidOperationException(
                    $"{nameof(EditValue)} method can't be invoked on {this.IniRowType}");
            }

            this.RawData = $"{this.SectionOrKeyName} = {(includeDoubleQuotes ? "\"" : "")}{value}{(includeDoubleQuotes ? "\"" : "")}";

            var oldValue = this.Value;
            this.Value = value;

            this.Dirty = true;

            return oldValue;
        }
    }
}
