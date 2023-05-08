using System;
using System.Linq;
using System.Collections.Generic;
using System.Text;
using System.IO;

namespace InMage.Test.API
{
    /// <summary>
    /// Loads and parses configuration file
    /// </summary>
    public class ConfigurationReader
    {
        public List<Section> Sections { get; private set; }

        public ConfigurationReader()
        {

        }

        public ConfigurationReader(string filePath)
        {            
            LoadConfiguration(filePath);
        }

        /// <summary>
        /// Loads configuration file
        /// </summary>
        public void LoadConfiguration(string filePath)
        {
            if (!String.IsNullOrEmpty(filePath) && File.Exists(filePath))
            {
                Sections = new List<Section>();
                string[] rawData = File.ReadAllLines(filePath);
                if (rawData != null && rawData.Length > 0)
                {
                    Parse(rawData);
                }
            }
            else
            {
                throw new ConfigurationReaderException("Configuration file not found");
            }
        }

        /// <summary>
        /// Get the section instance of the given section name
        /// </summary>
        public Section GetSection(string sectionName)
        {
            Section section = null;

            if(Sections != null)
            {
                foreach(Section sectionInstance in Sections)
                {
                    if(String.Equals(sectionInstance.Name, sectionName, StringComparison.OrdinalIgnoreCase))
                    {
                        section = sectionInstance;
                        break;
                    }
                }
            }

            return section;
        }

        /// <summary>
        /// Parses the raw data to Sections
        /// </summary>
        protected void Parse(string[] rawData)
        {
            Section section = null;
            string lineFormatted;
            bool sectionFound = false;
            string currentSectionName = String.Empty;
            int lineNumber = 0;
            foreach (string line in rawData)
            {
                ++lineNumber;
                lineFormatted = line.Trim();
                if (!String.IsNullOrEmpty(lineFormatted) && !lineFormatted.StartsWith("#"))
                {
                    if (lineFormatted.StartsWith("["))
                    {
                        if (lineFormatted.Length < 2 || !lineFormatted.EndsWith("]"))
                        {
                            throw new ConfigurationReaderException(String.Format("The format of Section: {0} at line number: {1} is incorrect.", lineFormatted, lineNumber));
                        }
                        currentSectionName = lineFormatted.Substring(1, lineFormatted.Length - 2).Trim();
                        section = new Section(currentSectionName);
                        Sections.Add(section);
                        sectionFound = true;                        
                    }
                    else
                    {
                        if (sectionFound)
                        {
                            section.AddKey(new Key(lineFormatted, currentSectionName));
                        }
                    }
                }
            }
        }
    }
}
