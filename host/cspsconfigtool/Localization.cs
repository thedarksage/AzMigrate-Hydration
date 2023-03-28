using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Win32;
using Logging;

namespace cspsconfigtool
{
    public class Localization
    {
        private const string RegSubKeyDRA = "SOFTWARE\\Microsoft\\Azure Site Recovery";
        private const string RegValueNameLocale = "FabricAdapterLocale";

        public static Dictionary<string, string> LanguageCodes;

        static Localization()
        {
            LanguageCodes = new Dictionary<string, string>()
            {
                { "Portuguese", "cs-CZ" },
                { "German", "de-DE" },
                { "English", "en-US" },
                { "Spanish", "es-ES" },
                { "French", "fr-FR" },
                { "Hungarian (Hungary)", "hu-HU" },
                { "Italian", "it-IT" },
                { "Japanese", "ja-JP" },
                { "Korean",  "ko-KR" },
                { "Russian (Russia)", "ru-RU" },
                { "Polish (Poland)", "pl-PL" },
                { "Portuguese (Brazil)", "pt-BR" },
                { "Simplified_Chinese", "zh-CN" },
                { "Traditional_Chinese", "zh-Hant" }
            };
        }

        public static bool UpdateLocale(string language, out string errorMessage)
        {
            bool status = false;
            errorMessage = null;
            RegistryKey regBaseKey = null;
            RegistryKey regKey = null;
            try
            {
                if (LanguageCodes.ContainsKey(language))
                {
                    regBaseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
                    if (regBaseKey == null)
                    {
                        regBaseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);
                    }
                    if (regBaseKey != null)
                    {
                        regKey = regBaseKey.OpenSubKey(RegSubKeyDRA, true);
                        if (regKey == null)
                        {
                            regKey = Registry.LocalMachine.CreateSubKey(RegSubKeyDRA);
                        }
                        if (regKey != null)
                        {
                            regKey.SetValue(RegValueNameLocale, LanguageCodes[language]);
                            status = true;
                            Logger.Debug("Successfully updated locale setting.");
                        }
                    }
                    if (!status)                    
                    {
                        errorMessage = "Failed to update localization setting with internal error.";
                        Logger.Error(errorMessage + " Error: Unable to open registry key.");
                    }
                }
                else
                {
                    errorMessage = String.Format("Couldn't update localization setting as the language {0} is not supported.", language);
                    Logger.Error(errorMessage);
                }
            }
            catch(Exception ex)
            {
                Logger.Error("Failed to update localization setting. Error: " + ex);
                if(ex is  System.Security.SecurityException)
                {
                    errorMessage = "Failed to update localization setting. Please ensure that the user has the permissions required to create or modify registry keys.";
                }
                else
                {
                    errorMessage = "Failed to update localization setting with internal error.";
                }
            }
            finally
            {
                if (regKey != null)
                {
                    regKey.Close();
                }
            }
            return status;
        }

        public static string GetLanguage(string languageCode)
        {
            string language = null;
            if (!String.IsNullOrEmpty(languageCode))
            {
                language = LanguageCodes.FirstOrDefault(x => x.Value == languageCode).Key;
            }

            return language;
        }

        public static string ReadLocale(out string errorMessage)
        {
            string locale = null;
            errorMessage = null;
            RegistryKey regBaseKey = null;
            RegistryKey regKey = null;
            try
            {                
                regBaseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
                if (regBaseKey == null)
                {
                    regBaseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);
                }
                if (regBaseKey != null)
                {
                    regKey = regBaseKey.OpenSubKey(RegSubKeyDRA);
                    if (regKey != null)
                    {
                        object value = regKey.GetValue(RegValueNameLocale);
                        if(value != null)
                        {
                            locale = value.ToString();
                            Logger.Debug("Found locale: " + locale);
                        }
                    }
                }
                if (String.IsNullOrEmpty(locale))
                {
                    errorMessage = "Failed to read localization setting with internal error.";
                    Logger.Error(errorMessage + " Error: Unable to open registry key.");
                }
            }
            catch (Exception ex)
            {
                Logger.Error("Failed to read localization setting. Error: " + ex);
                if (ex is System.Security.SecurityException)
                {
                    errorMessage = "Failed to read localization setting. Please ensure that the user has the permissions required to read registry keys.";
                }
                else
                {
                    errorMessage = "Failed to read localization setting with internal error.";
                }
            }
            finally
            {
                if (regKey != null)
                {
                    regKey.Close();
                }
            }
            return locale;
        }
    }
}
