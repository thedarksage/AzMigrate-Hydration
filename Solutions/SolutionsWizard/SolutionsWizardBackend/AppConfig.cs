using System;
using System.Collections.Generic;
using System.Text;
using System.Configuration;
using System.Xml;
using System.Collections;
using System.IO;
using System.Diagnostics;
using System.Windows.Forms;


namespace Com.Inmage
{
    class AppConfig
    {// 01_007_XXX

        private static string ConfigFile = "scout.xml";
        private Hashtable configHash;



        internal Hashtable GetConfigInfo(string appName)
        {

            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
            //based on above comment we have added xmlresolver as null
                XmlDocument document = new XmlDocument();
                document.XmlResolver = null;
                XmlNodeList appNodes = null;
                Hashtable configHash = new Hashtable();
                string key, value;
                XmlNodeReader reader = null;
                string configFile = "";

                try
                {
                    Debug.WriteLine("\n Entered: AppConfig.GetConfigInfo");

                    System.Configuration.Configuration config = null;

                    try
                    {
                        config = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);



                        if (config == null)
                        {
                            Console.WriteLine(
                              "The configuration file does not exist.");
                            Console.WriteLine(
                             "Use OpenExeConfiguration to create the file.");
                            return null;
                        }
                        else
                        {
                            configFile = config.FilePath;
                            Debug.WriteLine("\t AppConfig.GetConfigInfo: Configuration file is " + config.FilePath);

                        }
                    }
                    catch (Exception e)
                    {

                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + e.Message);
                        Trace.WriteLine("ERROR___________________________________________");


                    }

                    // currentDir = _installPath;
                    //  configFile = currentDir + "\\" + CONFIG_FILE;





                    if (!File.Exists(configFile))
                    {

                        //Trace.WriteLine("01_002_001: The file " + inStrFilename + "could not be located");
                        Debug.WriteLine("01_007_001:  The file " + configFile + "could not be located");

                        Debug.WriteLine("01_007_001: The file " + configFile + "could not be located");
                        Trace.WriteLine("01_007_001: The file " + configFile + "could not be located");
                        return null;


                    }

                    try
                    {
                        //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                        XmlReaderSettings settings = new XmlReaderSettings();
                        settings.ProhibitDtd = true;
                        settings.XmlResolver = null;
                        using (XmlReader reader1 = XmlReader.Create(configFile, settings))
                        {

                            document.Load(reader1);
                            //reader1.Close();
                        }
                    }
                    catch (XmlException xmle)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(xmle, true);

                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + xmle.Message);
                        Trace.WriteLine("ERROR___________________________________________");

                        Console.WriteLine(xmle.Message);
                        return null;
                    }




                    Debug.WriteLine("\t AppConfig.GetConfigInfo: Looking for  " + appName + " in the configuration file ");
                    appNodes = document.GetElementsByTagName(appName);



                    foreach (XmlNode appNode in appNodes)
                    {
                        Debug.WriteLine("\t AppConfig.GetConfigInfo: Found " + appName + " in the configuration file ");
                        if (appNode.HasChildNodes)
                        {



                            XmlNodeList appSettingsList = appNode.ChildNodes;


                            foreach (XmlNode node in appSettingsList)
                            {


                                reader = new XmlNodeReader(node);
                                reader.Read();

                                if (reader.GetAttribute("key") != null)
                                {
                                    if (reader.GetAttribute("value") != null)
                                    {
                                        key = reader.GetAttribute("key").ToString();
                                        value = reader.GetAttribute("value").ToString();

                                        configHash[key] = value;

                                        // Debug.WriteLine("Adding values" + key + "  :  " + value );

                                    }
                                    else
                                    {
                                        Debug.WriteLine("01_007_002: " + reader.GetAttribute("key") + " has no value in the config file");

                                    }

                                }
                            }

                        }
                    }



                    configHash = configHash;

                    Debug.WriteLine("\n Exiting: AppConfig.GetConfigInfo");
                }
                catch (Exception ex)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");


                }

            return configHash;

        }



        internal void GetTracingFilter()
        {
           

        }




        internal void PrintConfigValues(Hashtable inHash)
        {
            try
            {
                Debug.WriteLine("\n Entered: PrintConfigValues Config values--------------------");

                foreach (string key in inHash.Keys)
                {
                    
                    Debug.WriteLine("\t" + key + "\t" +  configHash[key]);

                }
            }
            catch (Exception e)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }

            Debug.WriteLine("\n Exiting: PrintConfigValues ");
        }


            
    
    }
}
