using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.Test.API
{
    /// <summary>
    /// Implementation of Section
    /// </summary>
    public class Section
    {
        public string Name { get; private set; }
        public List<Key> Keys { get; private set; }

        public Section(string name)
        {
            Name = name;
        }

        public Section(string name, List<Key> keys)
        {
            Name = name;
            if (keys != null)
            {
                Keys = new List<Key>();
                Keys.AddRange(keys);
            }
        }

        /// <summary>
        /// Adds key to the section
        /// </summary>
        /// <param name="key">key object</param>
        public void AddKey(Key key)
        {
            if (Keys == null)
            {
                Keys = new List<Key>();
            }
            Keys.Add(key);
        }

        public void RemoveKey(string keyName)
        {
            if (Keys != null)
            {
                foreach(Key key in Keys)
                {
                    if(String.Compare(key.Name, keyName, true) == 0)
                    {
                        Keys.Remove(key);
                        break;
                    }
                }
            }            
        }

        public void SetKeyValue(string keyName, string value)
        {
            Key key = GetKey(keyName);
            if(key != null)
            {
                key.Value = value;
            }
        }

        public void SetSubKeyValue(string keyName, string subKeyName, string subKeyValue)
        {
            Key key = GetKey(keyName);
            if (key != null)
            {
                key.SetSubKeyValue(subKeyName, subKeyValue);
            }
        }

        /// <summary>
        /// Get the value of the specified key
        /// </summary>
        /// <param name="keyName">Name of the key</param>
        /// <returns>Key Value</returns>
        public string GetKeyValue(string keyName)
        {
            if (Keys != null)
            {
                foreach (Key key in Keys)
                {
                    if (!String.IsNullOrEmpty(keyName) && String.Equals(key.Name, keyName, StringComparison.OrdinalIgnoreCase))
                    {
                        return key.Value;
                    }
                }
            }
            return null;
        }

        /// <summary>
        /// Get key instance for the given key name
        /// </summary>
        public Key GetKey(string keyName)
        {
            if (Keys != null)
            {
                foreach (Key key in Keys)
                {
                    if (!String.IsNullOrEmpty(keyName) && String.Equals(key.Name, keyName, StringComparison.OrdinalIgnoreCase))
                    {
                        return key;
                    }
                }
            }
            return null;
        }

        /// <summary>
        /// Get all the keys having the same root in a section
        /// </summary>
        public List<Key> GetKeyArray(string root)
        {
            if (String.IsNullOrEmpty(root))
            {
                return null;
            }
            List<Key> keyArray = new List<Key>();
            if (Keys != null)
            {
                foreach (Key key in Keys)
                {
                    if (!String.IsNullOrEmpty(key.RootName) && String.Equals(key.RootName, root, StringComparison.OrdinalIgnoreCase))
                    {
                        keyArray.Add(key);
                    }
                }
            }
            return keyArray;
        }

        /// <summary>
        /// Replaces placeholders with its original values of the entire section.
        /// </summary>
        /// <param name="placeholders">Map of placeholder and its value.</param>
        public void ReplacePlaceholders(Dictionary<string, string> placeholders)
        {
            if (this.Keys != null)
            {
                foreach (Key key in this.Keys)
                {
                    key.ReplacePlaceholders(placeholders);
                }
            }
        }

        public string Serialize()
        {
            string serializedString = String.Format("\r\n\r\n[{0}]", Name);
            if(Keys != null)
            {
                foreach(Key key in Keys)
                {
                    serializedString += key.Serialize();
                }
            }
            return serializedString;
        }
    }
}
