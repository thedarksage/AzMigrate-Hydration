using System;
using System.IO;

namespace ASRSetupFramework
{
    public class GrepUtils
    {
        /// <summary>
        /// Gets the value of a keyfrom the given file.
        /// </summary>
        /// <param name="filePath">File Path</param>
        /// <param name="key">Key</param>
        /// <returns>Value of the Key</returns>
        public static string GetKeyValueFromFile(String filePath, String key)
        {
            String returnValue = "";
            try
            {
                char[] delimiterChars = { '=' };

                if (File.Exists(filePath))
                {
                    Trc.Log(LogLevel.Always, "{0} file exists.", filePath);
                    // Read contents of the file to a string array
                    String[] lines = File.ReadAllLines(filePath);
                    foreach (String line in lines)
                    {
                        //When match is found, split the line with = and remove " characters. Break the loop after first match.
                        if (line.StartsWith(key))
                        {
                            String[] words = line.Split(delimiterChars);
                            returnValue = words[1].Replace("\"", "").Trim();
                            break;
                        }
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Always, "{0} file doesn't exists.", filePath);
                }
                return returnValue;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Error, "Exception occurred while GetKeyValueFromFile: {0}", e.ToString());
                return returnValue;
            }
        }
    }
}