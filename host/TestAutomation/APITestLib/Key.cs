using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.Test.API
{
    /// <summary>
    /// Implementation of nested keys
    /// </summary>
    public class Key
    {
        public string Name { get; set; }
        public string Value { get; set; }
        public List<Key> SubKeys { get; private set; }
        public string RootName { get; private set; }
        public bool HasSubKeys { get; private set; }

        public Key(string name, string value, List<Key> subKeys)
        {
            Name = name;
            Value = value;
            if (subKeys != null)
            {
                SubKeys = new List<Key>();
                SubKeys.AddRange(subKeys);
            }
        }

        public Key(string rawData, string sectionName)
        {
            Parse(rawData.Trim(), sectionName);
        }

        public void AddSubKey(Key subKey)
        {
            if (SubKeys == null)
            {
                SubKeys = new List<Key>();
                HasSubKeys = true;
            }
            SubKeys.Add(subKey);
        }

        public void SetSubKeyValue(string subKeyName, string subKeyValue)
        {
            if (SubKeys != null)
            {
                foreach (Key subKey in SubKeys)
                {
                    if (String.Equals(subKey.Name, subKeyName, StringComparison.OrdinalIgnoreCase))
                    {
                        subKey.Value = subKeyValue;
                        break;
                    }
                }
            }            
        }

        public void RemoveSubKey(string subKeyName)
        {
            if (SubKeys != null)
            {
                foreach (Key subKey in SubKeys)
                {
                    if (String.Equals(subKey.Name, subKeyName, StringComparison.OrdinalIgnoreCase))
                    {
                        SubKeys.Remove(subKey);
                        if(SubKeys.Count == 0)
                        {
                            HasSubKeys = false;
                        }
                        break;
                    }
                }                
            }            
        }

        public string GetSubKeyValue(string subKeyName)
        {
            string value = null;

            if (SubKeys != null)
            {
                foreach (Key subKey in SubKeys)
                {
                    if (String.Equals(subKey.Name, subKeyName, StringComparison.OrdinalIgnoreCase))
                    {
                        value = subKey.Value;
                        break;
                    }
                }
            }
            return value;
        }

        /// <summary>
        /// Parses the raw data to Keys (Name Value pairs)
        /// </summary>
        /// Note: Ensure that Key Name or Value doesn't contain special characters like semicolon (;) and equals (=)
        protected void Parse(string rawData, string sectionName)
        {
            int index = rawData.IndexOf('=');
            if (index > 0)
            {
                Name = rawData.Substring(0, index).Trim();
                //Console.WriteLine("Name: " + Name);
                if (!String.IsNullOrEmpty(Name) && Name.Contains("."))
                {
                    int rootIndex = Name.LastIndexOf('.');
                    if (rootIndex > 0)
                    {
                        RootName = Name.Substring(0, rootIndex);
                        //Console.WriteLine("Root=" + RootName);
                    }
                }
                if ((index + 1) < rawData.Length)
                {
                    string valueStr = rawData.Substring(index + 1);
                    if (valueStr.Contains("="))
                    {
                        string[] subKeyTokens = valueStr.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries);
                        if (subKeyTokens.Length == 1)
                        {
                            AddSubKey(new Key(valueStr, sectionName));
                        }
                        else
                        {
                            foreach (string subKeyToken in subKeyTokens)
                            {
                                AddSubKey(new Key(subKeyToken, sectionName));
                            }
                        }
                    }
                    else
                    {
                        //Console.WriteLine("Value: " + valueStr);
                        Value = valueStr.Trim(new char[] { ' ', '"' });
                    }
                }
            }
            else
            {
                throw new ConfigurationReaderException(String.Format("The format of Key: {0} under Section: {1} is incorrect.", rawData, sectionName));
            }
        }

        /// <summary>
        /// Replaces placeholders with its original values with in a key and its subkey values.
        /// </summary>
        /// <param name="placeholders">Map of placeholder and its value.</param>
        public void ReplacePlaceholders(Dictionary<string, string> placeholders)
        {
            if (!String.IsNullOrEmpty(this.Value) && this.Value.StartsWith("{") && this.Value.EndsWith("}"))
            {
                string placeholder = this.Value.Trim(new char[] { '{', '}' });
                if (placeholders.ContainsKey(placeholder) && !String.IsNullOrEmpty(placeholders[placeholder]))
                {
                    this.Value = placeholders[placeholder];
                }
            }
            if (this.SubKeys != null)
            {
                foreach (Key subKey in this.SubKeys)
                {
                    subKey.ReplacePlaceholders(placeholders);
                }
            }
        }

        public string Serialize()
        {
            string serializedString = String.Format("\r\n{0}={1}", Name, Value);
            if (SubKeys != null)
            {
                foreach (Key subKey in SubKeys)
                {
                    serializedString += String.Format("{0}={1};", subKey.Name, subKey.Value);
                }
            }
            return serializedString;
        }
    }
}
