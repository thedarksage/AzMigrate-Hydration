using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace CSAuthModuleNS
{
    public class CSConfigData
    {
        public String[] blockedFolders { get; set; }
        public String[] blockedUrls { get; set; }
        public String[] foldersAllowingAccessWithoutAnyChecks { get; set; }
        public String[] urlsAllowingAccessWithoutAnyChecks { get; set; }
        public String[] extensionsAllowingAccessWithoutAnyChecks { get; set; }
        public String client_authentication { get; set; }

        public CSConfigData()
        {


            // access checks are performed by application (php)
            foldersAllowingAccessWithoutAnyChecks = new String[] { "/lang/", "/help/", "/ui/", "/inmages/", "/trends/", "/debugger/" };
            urlsAllowingAccessWithoutAnyChecks = new String[] { "/", "/index.php", "/index.html", "/ScoutAPI/CXAPI.php" };
            extensionsAllowingAccessWithoutAnyChecks = new String[] { ".ico" };

            // Access to these folders/urls are not allowed
            blockedFolders = new String[] { "/app/", "/config/", "/cs/", "/SourceMBR/", "/cgi-bin/" };
            blockedUrls = new String[] { };

            // access to rest of folders/urls will have to pass signature validation by IIS module
            client_authentication = "1";
        }

    }

    internal static class CSConfig
    {
        static Object lockObject = new Object();


        /// <summary>
        /// This function will read amethyst.conf and fill up the needed configuration data
        /// </summary>
        /// 
        public static void ReadCSConfigData(ref CSConfigData csConfData)
        {
            string csInstallationPath = GetCSInstallationPath();
            String csConfFilePath = Environment.ExpandEnvironmentVariables(csInstallationPath+"\\home\\svsystems\\etc\\amethyst.conf");
            lock (lockObject)
            {
                String[] csConf;
                try
                {
                    csConf = File.ReadAllLines(csConfFilePath);
                }
                catch (Exception ex)
                {
                    throw new Exception(String.Format("unable to read {0}", csConfFilePath), ex);
                }
                ParseCSConfigData(csConf, ref csConfData);
            }
        }

        public static string GetCSInstallationPath()
        {
            String csInstallPath = null;
            String[] appConf;
            String appConfFilePath = Environment.ExpandEnvironmentVariables("%ProgramData%\\Microsoft Azure Site Recovery\\Config\\app.conf");

            if (!File.Exists(appConfFilePath))
            {
                throw new Exception(String.Format("file not found at {0}", appConfFilePath));
            }

            try
            {
                appConf = File.ReadAllLines(appConfFilePath);
            }
            catch (Exception ex)
            {
                throw new Exception(String.Format("unable to read {0}", appConfFilePath), ex);
            }

            String key;
            foreach (String linetoParse in appConf)
            {
                if (!String.IsNullOrEmpty(linetoParse))
                {
                    String[] lineSplit = linetoParse.Split('=');
                    if (lineSplit.Length == 2)
                    {
                        key = lineSplit[0].Trim();
                        if (key == "INSTALLATION_PATH")
                        {
                            csInstallPath = lineSplit[1].Trim(new char[] { ' ', '\"' });
                            if (String.IsNullOrEmpty(csInstallPath))
                            {
                                throw new Exception(String.Format("{0} contains empty value for INSTALLATION_PATH key", appConfFilePath));
                            }
                            break;
                        }
                    }
                }
            }
            
            if (String.IsNullOrEmpty(csInstallPath))
            {
                throw new Exception(String.Format("{0} does not contains value for INSTALLATION_PATH key", appConfFilePath));
            }

            return csInstallPath;
        }

        private static void ParseCSConfigData(String[] csConf, ref CSConfigData csConfData)
        {
            String key;
            object value;
            Dictionary<String, object> csConfDic = new Dictionary<String, object>();
            foreach (String linetoParse in csConf)
            {
                key = "";
                value = new object();
                if (!String.IsNullOrEmpty(linetoParse))
                {
                    String[] lineSplit = linetoParse.Split('=');
                    if (lineSplit.Length == 2)
                    {
                        key = lineSplit[0].Trim();
                        value = lineSplit[1].Trim();
                        if (value != null)
                        {
                            int nonEmptyValues = 0;

                            if (value.ToString().Contains(','))
                            {
                                String[] commaSplit = value.ToString().Split(',');
                                                               
                                for (int i = 0; i < commaSplit.Length; i++)
                                {
                                    String trimmedValue = commaSplit[i].Trim(new char[] { ' ', '\"' });
                                    if (!String.IsNullOrEmpty(trimmedValue))
                                        ++nonEmptyValues;
                                }

                                if (0 != nonEmptyValues)
                                {
                                    String[] valueStrArray = new String[nonEmptyValues];

                                    for (int i = 0; i < commaSplit.Length; i++)
                                    {
                                        String trimmedValue = commaSplit[i].Trim(new char[] { ' ', '\"' });
                                        if (!String.IsNullOrEmpty(trimmedValue))
                                            valueStrArray[i] = trimmedValue;
                                    }

                                    value = valueStrArray;
                                }
                            }
                            else
                            {
                                value = value.ToString().Trim(new char[] { ' ', '\"' });
                            }

                            if (!String.IsNullOrEmpty(key))
                                csConfDic.Add(key, value);
                        }
                    }
                }
            }

            if (csConfDic.Count() > 0)
            {
                csConfData = ConvertDictionaryToObject<CSConfigData>(csConfDic);
            }

        }

        static T ConvertDictionaryToObject<T>(Dictionary<String, object> dictionary)
        {
            Type objType = typeof(T);
            var objHandle = Activator.CreateInstance(objType);

            foreach (var keyvalue in dictionary)
            {
                System.Reflection.PropertyInfo pi = objType.GetProperty(keyvalue.Key, System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.IgnoreCase | System.Reflection.BindingFlags.Public);
                if (pi != null)
                {
                    pi.SetValue(objHandle, keyvalue.Value);
                }
            }

            return (T)objHandle;
        }
    }
}
