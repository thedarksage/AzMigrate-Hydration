using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net;
using System.IO;
using System.Runtime.InteropServices;
using InMage.Logging;

namespace InMage.APIInterfaces
{
    /*
     * Class: LocalAPIInterface provides concerete implementation of executing API using Local DLL
     */
    internal class LocalAPIInterface : APIInterface
    {
        [DllImport("InMageSDK.dll", EntryPoint = "NoComInit")]
        public static extern void noComInit();

        [DllImport("InMageSDK.dll",EntryPoint = "processRequestWithFile")]
        public static extern int processRequestWithFile([MarshalAs(UnmanagedType.LPStr)]string requestFile, [MarshalAs(UnmanagedType.LPStr)]string responseFile);

        [DllImport("InMageSDK.dll", EntryPoint = "processRequestWithSimpleCStream")]
        public static extern int processRequestWithSimpleCStream([MarshalAs(UnmanagedType.LPStr)] string input, out IntPtr responsePtr);

        [DllImport("InMageSDK.dll", EntryPoint = "Inm_cleanUp")]
        public static extern void Inm_cleanUp(out IntPtr strBuffer);

        Dictionary<String, String> dictionary;

        /*
         *  Initialize the CXAPIInterface
         */
        public void init(Dictionary<string, string> dictionary)
        {
            this.dictionary = dictionary;
        }

        /*
         *  Post the RequestXML to Local DLL and Return the ResponseXML to caller program
         */
        public string post(string requestXML)
        {
            String responseXML = String.Empty;
            String requestIs = dictionary["RequestIs"];
            if (String.Compare(requestIs, "File", true)==0) 
            {
                responseXML=postWithFile(requestXML);
            }
            else
            {
                IntPtr responsePtr = IntPtr.Zero;
                try
                {
                    Logger.Info("Posting the Request to the DLL");
                    noComInit();
                    int errorCode = processRequestWithSimpleCStream(requestXML, out responsePtr);
                    Logger.Info("The return value of processRequestWithSimpleCStream() is " + errorCode);
                    responseXML = Marshal.PtrToStringAnsi(responsePtr);
                }
                catch (Exception ex)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                    Logger.Error("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + ex.Message);
                    Logger.Error("Failed to post the request to the DLL.");
                }
                finally
                {
                    Logger.Debug("Entering Inm_cleanUp()");
                    Inm_cleanUp(out responsePtr);
                    Logger.Debug("Exited Inm_cleanUp()");
                }
            }

            return responseXML;
        }

        public string postWithFile(string requestXML)
        {            
            string responseXML = "";
            string requestFilePath ;
            string responseFilePath;
            const int minVal = 1000;
            const int maxVal = 9999;
            int randomNumber;
            Random random = new Random();
            randomNumber = random.Next(minVal, maxVal);
            requestFilePath = String.Format("dataRequest_{0}.xml", randomNumber);
            responseFilePath = String.Format("dataResponse_{0}.xml", randomNumber);
           
            StreamWriter tw = new StreamWriter(requestFilePath);
            tw.WriteLine(requestXML);
            tw.Close();

            Logger.Info("Posting the Request to the DLL");
            noComInit();
            int errorCode = processRequestWithFile(requestFilePath, responseFilePath);
            Logger.Info("The return value of processRequestWithFile() is " + errorCode);

            StreamReader tr = new StreamReader(responseFilePath);
            responseXML = tr.ReadToEnd();
            tr.Close();
      
            if (File.Exists(requestFilePath))
                File.Delete(requestFilePath);
            if(File.Exists(responseFilePath))
                File.Delete(responseFilePath);

            return responseXML;

        }
    }
}
