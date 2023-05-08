using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace InMage.Test.API
{
    public class ConfigurationWriter
    {
        public static void Write(string configFilePath, List<Section> sections)
        {
            string serializedString = null;
            foreach (Section section in sections)
            {
                serializedString += section.Serialize();
            }
            if (String.IsNullOrEmpty(serializedString))
            {
                Logger.Error("Found empty configuration data to save into a file.");
            }
            else
            {
                // Write serialized configuration data into the file.
                using(StreamWriter sw = File.CreateText(configFilePath))
                {
                    sw.Write(serializedString);
                }
            }
        }
    }
}
