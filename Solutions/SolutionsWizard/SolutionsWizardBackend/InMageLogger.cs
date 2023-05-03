using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Reflection;
using System.Configuration;
using System.Security;
using System.Collections;
using Com.Inmage.Tools;

namespace Com.Inmage
{
     public  enum LogLevel { Off=0, Error, Info, Debug };
   
    internal class InMageLogger
    {
        internal static DirectoryInfo directoryInfo;
        internal static FileStream _fileStream;
        internal static StreamWriter _streamWriter = null;
        internal static StackTrace stackTrace;
        internal static MethodBase methodBase;

        internal static LogLevel _currentLogLevel;
        internal static string _folderName;
        internal static string _fileName;
        internal static string _installPath;
        


        static InMageLogger()
        {
            try
            {
                string logLevelString = "ERROR";
                AppConfig app = new AppConfig();


                Debug.WriteLine("Calling GetConfigInfo in InMageLogger ");
                Hashtable configInfo = app.GetConfigInfo("ERROR_LOG");
                _installPath = WinTools.FxAgentPath() + "\\vContinuum";


                if (configInfo == null)
                {
                    _folderName = _installPath;
                    _fileName = "scout.log";
                    _fileName = _folderName + "\\" + _fileName;
                    _currentLogLevel = (LogLevel)Enum.Parse(typeof(LogLevel), "ERROR");
                }
                else
                {
                    try
                    {

                        if (configInfo["LogFolder"] != null)
                        {
                            _folderName = "c:\\temp";
                        }
                        else
                        {
                            _folderName = configInfo["LogFolder"].ToString();
                        }


                        if (configInfo["FileName"] != null)
                        {
                            _fileName = "scout.log";
                        }
                        else
                        {
                            _fileName = configInfo["FileName"].ToString();
                        }



                        if (configInfo["DebugLevel"] == string.Empty)
                        {
                            Debug.WriteLine(" DebugLevel is empty");
                            logLevelString = "ERROR";
                        }
                        else
                        {
                            logLevelString = configInfo["DebugLevel"].ToString();
                        }

                        Debug.WriteLine("Debug level = " + logLevelString);


                        _fileName = _folderName + "\\" + _fileName;

                        _currentLogLevel = (LogLevel)Enum.Parse(typeof(LogLevel), logLevelString);

                    }
                    catch (Exception e)
                    {
                        Trace.WriteLine("InMage Logger exception: " + e.ToString());
                    }
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
               

            }

        }

        internal static void DebugMessage(string message)
        {
            if (_currentLogLevel == LogLevel.Debug )
            {
                Trace.WriteLine(message);
            }


        }
       

    

     

        

       



            
          
    

        
    }
   
}
