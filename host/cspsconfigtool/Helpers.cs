using System;
using System.IO;
using System.Globalization;
using System.Net;
using System.Resources;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Runtime.InteropServices;
using cspsconfigtool;
using Microsoft.Win32;
using System.Windows.Forms;
using Logging;
using System.Text.RegularExpressions;

namespace Services
{
    public class Helpers
    {
        const int MIN_INTERNET_PORT = 1;
        const int MAX_INTERNET_PORT = 65535;

        public const string MicrosoftNCSIDotCom = "http://www.msftncsi.com/ncsi.txt";

        [DllImport("configtool.dll", EntryPoint = "freeAllocatedBuf")]
        public static extern void freeAllocatedBuf(out IntPtr allocatedBuff);

        [DllImport("configtool.dll")]
        public static extern int StopServices( out IntPtr err);

        [DllImport("configtool.dll")]
        public static extern int StartServices(out IntPtr err);

        [DllImport("configtool.dll")]
        public static extern int UpdateHostGUID(out IntPtr err);

        [DllImport("configtool.dll")]
        public static extern int UpdateIPs(out IntPtr err);

        [DllImport("configtool.dll")]
        public static extern int DoDRRegister([MarshalAs(UnmanagedType.LPStr)] string cmdinput,out IntPtr processexitcode, out IntPtr err);
        
        [DllImport("configtool.dll")]
        public static extern int UpdateConfFilesInCS([MarshalAs(UnmanagedType.LPStr)] string amethystinput, [MarshalAs(UnmanagedType.LPStr)] string cxpsinput, [MarshalAs(UnmanagedType.LPStr)] string azurecsinput, out IntPtr err);

        [DllImport("configtool.dll", EntryPoint = "GetValueFromAmethyst")]
        public static extern int GetValueFromAmethyst([MarshalAs(UnmanagedType.LPStr)] string key,out IntPtr value, out IntPtr err);

        [DllImport("configtool.dll", EntryPoint = "ProcessPassphrase")]
        public static extern int ProcessPassphrase([MarshalAs(UnmanagedType.LPStr)] string req, out IntPtr err);

        [DllImport("configtool.dll", EntryPoint = "GetPassphrase")]
        public static extern int GetPassphrase(out IntPtr pptr, out IntPtr err);

        [DllImport("configtool.dll", EntryPoint = "GetValueFromCXPSConf")]
        public static extern int GetValueFromCXPSConf([MarshalAs(UnmanagedType.LPStr)] string key,out IntPtr value, out IntPtr err);


        public bool stopservices(ref string errmsg)
        {
            Logger.Debug("Entered Helpers::stopservices.");
            int ret = psform.SV_OK;
            bool rv = true;
            try
            {

                IntPtr pt = IntPtr.Zero;
                ret = StopServices(out pt);
                if (ret != psform.SV_OK)
                {
                    Logger.Error("Failed to stop services.");
                    errmsg = Marshal.PtrToStringAnsi(pt);
                    freeAllocatedBuf(out pt );
                    ret = psform.SV_FAIL;
                }
            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                errmsg = trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message;
                Logger.Error("Caught Exception... ExcMsg: " + errmsg);
                ret = psform.SV_FAIL;
            }
            if (ret != psform.SV_OK)
                rv = false;
            Logger.Debug("Exited Helpers::stopservices.");
            return rv;
        }
        public bool processinitUpdates(ref string errmsg)
        {
            Logger.Debug("Entered Helpers::processinitUpdates.");
            int ret = psform.SV_OK;
            bool rv = true;
            try
            {
                IntPtr guidErrPtr = IntPtr.Zero;
                ret = UpdateHostGUID(out guidErrPtr);
                if (ret == psform.SV_OK)
                {
                    IntPtr errpt = IntPtr.Zero;
                    ret = UpdateIPs(out errpt);
                    if (ret != psform.SV_OK)
                    {
                        Logger.Error("Failed to update ips.");
                        errmsg = Marshal.PtrToStringAnsi(errpt);
                        freeAllocatedBuf(out errpt);
                        ret = psform.SV_FAIL;
                    }
                }
                else
                {
                    Logger.Error("Failed to update Host Guid.");
                    errmsg = Marshal.PtrToStringAnsi(guidErrPtr);
                    freeAllocatedBuf(out guidErrPtr);
                    ret = psform.SV_FAIL;
                }

            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                errmsg = trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message;
                Logger.Error("Caught Exception... ExcMsg: " + errmsg);
                ret = psform.SV_FAIL;
            }
            if (ret != psform.SV_OK)
                rv = false;

            Logger.Debug("Exited Helpers::processinitUpdates.");
            return rv;

        }

        public bool startservices(ref string errmsg)
        {
            Logger.Debug("Entered Helpers::startservices.");
            int ret = psform.SV_OK;
            bool rv = true;
            try
            {

                IntPtr pt = IntPtr.Zero;
                ret = StartServices( out pt);
                if (ret != psform.SV_OK)
                {
                    Logger.Error("Failed to start services.");
                    errmsg = Marshal.PtrToStringAnsi(pt);
                    freeAllocatedBuf( out pt);
                    ret = psform.SV_FAIL;
                }
            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                errmsg = trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message;
                Logger.Error("Caught Exception... ExcMsg: " + errmsg);
                ret = psform.SV_FAIL;
            }
            if (ret != psform.SV_OK)
                rv = false;
            Logger.Debug("Exited Helpers::startservices.");
            return rv;
        }

        public bool processpassphrase(string req,ref string errmsg)
        {

            Logger.Debug("Entered Helpers::processpassphrase.");
            int ret = psform.SV_OK;
            bool rv = true;
            try
            {

                IntPtr pt = IntPtr.Zero;
                ret = ProcessPassphrase(req, out pt);
                if (ret != psform.SV_OK)
                {
                    Logger.Error("Failed to process passphrase.");
                    errmsg = Marshal.PtrToStringAnsi(pt);
                    freeAllocatedBuf(out pt);
                    ret = psform.SV_FAIL;
                }
            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                errmsg = trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message;
                Logger.Error("Caught Exception... ExcMsg: " + errmsg);
                ret = psform.SV_FAIL;
            }
            if (ret != psform.SV_OK)
                rv = false;
            Logger.Debug("Exited Helpers::processpassphrase.");
            return rv;
        }
        public bool drregister(ref string cmdinput, ref string processexitcode, ref string errmsg)
        {
            Logger.Debug("Entered Helpers::dodrregister.");
            int ret = csform.SV_OK;
            bool rv = true;
            try
            {

                IntPtr errResPtr = IntPtr.Zero;
                IntPtr exitCodePtr = IntPtr.Zero;
                ret = DoDRRegister(cmdinput, out exitCodePtr, out errResPtr);
                if (ret != csform.SV_OK)
                {
                    Logger.Error("Failed to do DrRegister.");
                    errmsg = Marshal.PtrToStringAnsi(errResPtr);
                    freeAllocatedBuf(out errResPtr);
                    ret = csform.SV_FAIL;
                }
                else
                {
                    processexitcode = Marshal.PtrToStringAnsi(exitCodePtr);
                    freeAllocatedBuf(out exitCodePtr);
                }
            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                MessageBox.Show(trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message,
                         "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                ret = csform.SV_FAIL;
            }
            if (ret != csform.SV_OK)
                rv = false;
            Logger.Debug("Exited Helpers::dodrregister.");
            return rv;
        }

        public bool UpdateConfFilesInCS(ref string amethystinput, ref string cxpsinput, ref string azurecsinput, ref string errmsg)
        {
            bool rv = true;
            Logger.Debug("Entered Helpers::UpdateConfFilesInCS.");
            int ret = csform.SV_OK;

            try
            {
                IntPtr errResPtr = IntPtr.Zero;
                ret = UpdateConfFilesInCS(amethystinput, cxpsinput, azurecsinput, out errResPtr);
                if (ret != csform.SV_OK)
                {
                    Logger.Error("Failed to update UpdateConfFilesInCS.");
                    errmsg = Marshal.PtrToStringAnsi(errResPtr);
                    freeAllocatedBuf(out errResPtr);
                    ret = csform.SV_FAIL;
                }
            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                errmsg = trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message;
                Logger.Error("Caught Exception... ExcMsg: " + errmsg);
                ret = psform.SV_FAIL;
            }
            if (ret != csform.SV_OK)
                rv = false;

            Logger.Debug("Exited Helpers::UpdateConfFilesInCS.");
            return rv;
        }

        //bool validateip(string ip)
        //{
        //    System.Net.IPAddress ipAddress = null;
        //    return System.Net.IPAddress.TryParse(ip, out ipAddress);
        //}
        public static bool validateport(string portValue)
        {
            if (string.IsNullOrEmpty(portValue))
                return false;

            Regex foundNum = new Regex(@"^[0-9]+$", RegexOptions.Compiled | RegexOptions.IgnoreCase);

            if (foundNum.IsMatch(portValue))
            {
                try
                {
                    int port = Convert.ToInt32(portValue);
                    if (port < 65536)
                        return true;
                }
                catch (OverflowException)
                {
                    Logger.Error("Caught exception while convertin to Int32 port");
                    return false;
                }
            }

            return false;
        }
        public bool ValidateUserInput(Object obj)
        {
            bool rv = true;
            psform ps = (psform)obj;

            //if (!validateip(ps.getEnteredIp()))
            //{
            //    Logger.Error("Invalid cs ip.");
            //    MessageBox.Show("Invalid cs ip.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
            //    return false;
            //}
            if (!validateport(ps.getEnteredPort()))
            {
                Logger.Error("Invalid cs_ps port.");
                MessageBox.Show("Invalid cs_ps port.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                return false;
            }
            return rv;
        }
        public bool initprocessfunction(object obj, ref string pscsip,ref string pscsport,ref string pphrase, ref string sslport, ref string dvacpport, ref string err, ref bool isCS, ref string azurecsip)
        {
            Logger.Debug("Entered initprocessfunction");
            bool rv = true;
            //psform ps = (psform)obj;

            IntPtr valuePtr = IntPtr.Zero;
            IntPtr errPtr = IntPtr.Zero;
 
            int ret = psform.SV_OK;
            try
            {
                ret = GetValueFromAmethyst("PS_CS_IP", out valuePtr, out errPtr);
                if (ret != psform.SV_OK)
                {
                    Logger.Debug("Failed to get the PS_CS_IP");
                    err = "Failed to get the PS_CS_IP" + Marshal.PtrToStringAnsi(errPtr);
                    freeAllocatedBuf(out errPtr);
                    rv = false;
                }
                else
                {
                    pscsip = Marshal.PtrToStringAnsi(valuePtr);
                    freeAllocatedBuf(out valuePtr);
                    Logger.Debug("pscsip GetValue :" + pscsip);

                    IntPtr vPtr = IntPtr.Zero;
                    IntPtr ePtr = IntPtr.Zero;
                
                    ret = GetValueFromAmethyst("PS_CS_PORT", out vPtr, out ePtr);
                    if (ret != psform.SV_OK)
                    {
                        Logger.Debug("Failed to get the PS_CS_PORT");
                        err = "Failed to get the PS_CS_PORT" + Marshal.PtrToStringAnsi(ePtr);
                        freeAllocatedBuf(out ePtr);
                        rv = false;
                    }
                    else
                    {
                        pscsport = Marshal.PtrToStringAnsi(vPtr);
                        freeAllocatedBuf(out vPtr);

                        Logger.Debug("pscsport GetValue :" + pscsport);

                        Logger.Debug("Getting the passphrase if exist.");

                        IntPtr pptr = IntPtr.Zero;
                        IntPtr perrPtr = IntPtr.Zero;
                        ret = GetPassphrase(out pptr, out perrPtr);
                        if (ret != psform.SV_OK)
                        {
                            Logger.Debug("Failed to get the passphrase");
                            err = "Failed to get the passphrase" + Marshal.PtrToStringAnsi(perrPtr);
                            freeAllocatedBuf(out perrPtr);
                            rv = false;
                        }
                        else
                        {
                            pphrase = Marshal.PtrToStringAnsi(pptr);
                            freeAllocatedBuf(out pptr);
                            Logger.Debug("Got passphrase.");

                            Logger.Debug("Getting the sslport if exist.");

                            IntPtr sslpptr = IntPtr.Zero;
                            IntPtr sslperrPtr = IntPtr.Zero;

                            ret = GetValueFromCXPSConf("ssl_port", out sslpptr, out sslperrPtr);
                            if (ret != psform.SV_OK)
                            {
                                Logger.Debug("Failed to get the ssl_port");
                                err = "Failed to get the ssl_port" + Marshal.PtrToStringAnsi(sslperrPtr);
                                freeAllocatedBuf(out sslperrPtr);
                                rv = false;
                            }
                            else
                            {
                                sslport = Marshal.PtrToStringAnsi(sslpptr);
                                freeAllocatedBuf(out sslpptr);
                                Logger.Debug("ssl_port GetValue: " + sslport);
                                if (isCS)
                                {
                                    IntPtr dvacpptr = IntPtr.Zero;
                                    IntPtr dvacperrPtr = IntPtr.Zero;
                                    ret = GetValueFromAmethyst("DVACP_PORT", out dvacpptr, out dvacperrPtr);
                                    if (ret != psform.SV_OK)
                                    {
                                        Logger.Debug("Failed to get the DVACP_PORT");
                                        err = "Failed to get the DVACP_PORT" + Marshal.PtrToStringAnsi(dvacperrPtr);
                                        freeAllocatedBuf(out dvacperrPtr);
                                        rv = false;
                                    }
                                    else
                                    {
                                        dvacpport = Marshal.PtrToStringAnsi(dvacpptr);
                                        freeAllocatedBuf(out dvacpptr);
                                        Logger.Debug("DVACP_PORT GetValue: " + dvacpport);
                                    }
                                }
                            }
                        }

                    } 
                }

                if (obj is csform)
                {
                    valuePtr = IntPtr.Zero;
                    errPtr = IntPtr.Zero;
                    ret = GetValueFromAmethyst("IP_ADDRESS_FOR_AZURE_COMPONENTS", out valuePtr, out errPtr);
                    if (ret != psform.SV_OK)
                    {
                        Logger.Debug("Failed to get the IP_ADDRESS_FOR_AZURE_COMPONENTS");
                        err = "Failed to get the IP_ADDRESS_FOR_AZURE_COMPONENTS" + Marshal.PtrToStringAnsi(errPtr);
                        freeAllocatedBuf(out errPtr);
                        rv = false;
                    }
                    else
                    {
                        azurecsip = Marshal.PtrToStringAnsi(valuePtr);
                        azurecsip = azurecsip.Replace("\"", "");
                        freeAllocatedBuf(out valuePtr);
                        Logger.Debug("azurecsip GetValue :" + azurecsip);
                    }
                }
            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
               
                string excmsg = trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message;
                Logger.Error("Caught Exception on Getting PS_CS_IP,PS_CS_PORT..  ExcMsg: " + err);
                err = "Caught Exception on Getting PS_CS_IP,PS_CS_PORT..  ExcMsg: " + excmsg;
                rv = false;
            }
            Logger.Debug("Exited initprocessfunction");
            return rv;
        }

        public static string GetCXAPIErrorMessage(string operation, Exception ex)
        {
            string errorMessage = null;
            string recommendation = "Please make sure that \"Default Web Site\" on IIS is up and running and retry the operation.";
            string timeOutErrorMessage = String.Format("Failed to {0} with time out error. Please retry the operation.", operation);
            if (ex is InMage.APIModel.APIModelException)
            {
                InMage.APIModel.APIModelException apiModelException = ex as InMage.APIModel.APIModelException;
                if (!String.IsNullOrEmpty(apiModelException.HTTPStatusCode))
                {
                    int httpStatusCode;
                    int.TryParse(apiModelException.HTTPStatusCode, out httpStatusCode);
                    if (httpStatusCode == (int)System.Net.HttpStatusCode.GatewayTimeout ||
                        httpStatusCode == (int)System.Net.HttpStatusCode.RequestTimeout)
                    {
                        errorMessage = timeOutErrorMessage;
                    }
                    else
                    {
                        errorMessage = String.Format("Failed to {0}. {1}", operation, recommendation);
                    }
                }
                else
                {
                    errorMessage = String.Format("Failed to {0}. Error: {1}", operation, ex.Message);
                }
            }
            else if (ex is System.Net.Sockets.SocketException)
            {
                System.Net.Sockets.SocketException socketException = ex as System.Net.Sockets.SocketException;
                errorMessage = (socketException.SocketErrorCode == System.Net.Sockets.SocketError.TimedOut) ? timeOutErrorMessage : String.Format("Failed to {0}. {1}", operation, recommendation);
            }
            else if (ex.InnerException != null && (ex.InnerException is System.Net.Sockets.SocketException))
            {
                System.Net.Sockets.SocketException socketException = ex.InnerException as System.Net.Sockets.SocketException;
                errorMessage = (socketException.SocketErrorCode == System.Net.Sockets.SocketError.TimedOut) ? timeOutErrorMessage : String.Format("Failed to {0}. {1}.", operation, recommendation);
            }
            else if (ex is System.Xml.XmlException)
            {
                errorMessage = String.Format("Failed to {0} with the XML parsing error.", operation);
            }
            else
            {
                errorMessage = String.Format("Failed to {0} with an internal error.", operation);
            }
            return errorMessage;
        }

        public static string GetCXAPIErrorMessage(string operation, InMage.APIHelpers.ResponseStatus responseStatus)
        {
            string errorMessage = null;
            InMage.APIHelpers.ErrorDetails errorDetails = responseStatus.ErrorDetails;
            if (errorDetails == null)
            {
                errorMessage = String.Format("Failed to {0}. {1} (Error code: {2})", operation, responseStatus.Message, responseStatus.ReturnCode);
            }
            else
            {
                errorMessage = String.Format("Failed to {0}. {1} (Error code: {2})", operation, errorDetails.ErrorMessage, errorDetails.ErrorCode);
                if(!String.IsNullOrEmpty(errorDetails.PossibleCauses))
                {
                    errorMessage += "\r\n\r\nPossible causes: " + errorDetails.PossibleCauses;
                }
                if(!String.IsNullOrEmpty(errorDetails.Recommendation))
                {
                    errorMessage += "\r\n\r\nRecommendation: " + errorDetails.Recommendation;
                }
            }
            return errorMessage;
        }

        //This Method will get Pushinstall path and modifies Config file. 


        public bool UpdatePushInstallConfigFile(bool disableSignatureVerification)
        {
            try
            {
                Logger.Debug("Entered UpdatePushInstallConfigFile to check signature verification");
                string pushInstallPath = GetPushInstallerDirectory();
                if (pushInstallPath == null)
                {
                    Logger.Debug("Failed to get push installer path");
                    return false;
                }
                string filename = pushInstallPath + "\\pushinstaller.CONF";

                if (File.Exists(filename))
                {

                    var lines = File.ReadAllLines(filename);
                    int lineNumber = 0;
                    string modifiedLine = null;


                    lineNumber = FindStringInLines(lines, "SignatureVerificationChecks");
                    if (lineNumber != 0)
                    {
                        string[] lineEditsplit = lines[lineNumber - 1].Split('=');

                        if (lineEditsplit.Length == 2)
                        {
                            string value = lineEditsplit[1];

                            if (disableSignatureVerification == false && value.Contains("1") )
                            {
                                return true;
                            }
                            else if (disableSignatureVerification == true && value.Contains("0") )
                            {
                                return true;
                            }
                            else
                            {
                                if (disableSignatureVerification == true)
                                {
                                    if (value.Contains("\""))
                                    {
                                        modifiedLine = AddValueToModifiedLine(lineEditsplit[0], "\"0\"");
                                    }
                                    else
                                    {
                                        modifiedLine = AddValueToModifiedLine(lineEditsplit[0], "0");
                                    }

                                }
                                else
                                {
                                    if (value.Contains("\""))
                                    {
                                        modifiedLine = AddValueToModifiedLine(lineEditsplit[0], "\"1\"");
                                    }
                                    else
                                    {
                                        modifiedLine = AddValueToModifiedLine(lineEditsplit[0], "1");
                                    }
                                }
                            }
                        }
                    }
                    if (modifiedLine != null)
                    {
                        BackUpExistingConfig(filename, pushInstallPath);
                        lines[lineNumber - 1] = modifiedLine;

                        try
                        {
                            File.WriteAllLines(filename, lines);

                        }
                        catch (Exception ex)
                        {
                            Logger.Debug(" Failed to Write a file " + ex.Message);
                        }
                        Logger.Debug(" Modified line " + modifiedLine);
                    }
                    else
                    {
                        //MessageBox.Show("Need to add singature line");
                        string signatureLine = "SignatureVerificationChecks=";
                        int linenumber = 1;
                        foreach (string linetoEdit in lines)
                        {

                            if (linetoEdit != null && linetoEdit.ToUpper().StartsWith("[PUSHINSTALLER.TRANSPORT]"))
                            {
                                if (disableSignatureVerification == true)
                                {
                                    signatureLine = signatureLine + "0";
                                }
                                else
                                {
                                    signatureLine = signatureLine + "1";

                                }
                                break;
                            }
                            linenumber++;
                        }

                        BackUpExistingConfig(filename, pushInstallPath);
                        List<string> l = new List<string>();
                        l.AddRange(lines);
                        l.Insert(linenumber + 1, signatureLine);
                        File.WriteAllLines(filename, l.ToArray());

                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);

                Logger.Debug("ERROR*******************************************");
                Logger.Debug("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Logger.Debug("Exception  =  " + ex.Message);
                Logger.Debug("ERROR___________________________________________");
                Logger.Close();
            }
            return true;
        }


        private int FindStringInLines(string[] lines, string stringTofind)
        {
            int lineNumber = 0;
            foreach (string linetoEdit in lines)
            {
                lineNumber++;
                if (linetoEdit != null && (linetoEdit.StartsWith(stringTofind + " =") || linetoEdit.StartsWith(stringTofind + "=")))
                {
                    return lineNumber;
                }
            }
            return 0;
        }

        private string AddValueToModifiedLine(string lineEditsplit, string valueToAdd)
        {
            string modifiedLine = null;
            return modifiedLine = lineEditsplit + "=" + valueToAdd;
        }

        private bool BackUpExistingConfig(string filename, string pushInstallPath)
        {
            try
            {
                if (File.Exists(pushInstallPath + "\\pushinstaller_SigCheckModifiy_old.CONF"))
                {
                    File.Delete(pushInstallPath + "\\pushinstaller_SigCheckModifiy_old.CONF");
                }
                File.Move(filename, pushInstallPath + "\\pushinstaller_SigCheckModifiy_old.CONF");
            }
            catch (Exception ex)
            {
                Logger.Debug(" Failed to delete old back up file " + ex.Message);
            }
            return true;
        }

        private string GetPushInstallerDirectory()
        {
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            try
            {

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\PushInstaller");
                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\PushInstaller");

                if (hklm == null && hklm32 == null)
                {
                    return null;
                }
                else
                {
                    if (hklm != null)
                    {

                        obp = hklm.GetValue("InstallDirectory");
                    }
                    else
                    {

                        obp = hklm32.GetValue("InstallDirectory");
                    }
                    string path = null;
                    if (obp != null)
                    {
                        path = obp.ToString();
                        Logger.Debug(DateTime.Now + "\t push install " + path);

                    }
                    else
                    {
                        Logger.Debug(DateTime.Now + "\t push install path is null.");
                    }
                    return path;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);

                Logger.Debug("ERROR*******************************************");
                Logger.Debug("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Logger.Debug("Exception  =  " + ex.Message);
                Logger.Debug("ERROR___________________________________________");
                return null;
            }
        }

        public bool GetSignatureVerificationChecksConfig(ref bool isRequireSignatureVerificationChecks, ref string errmsg)
        {
            bool ret = true;
            try
            {
                Logger.Debug("Entered GetSignatureVerificationChecksValue to check signature verification");
                string pushInstallPath = GetPushInstallerDirectory();
                if (pushInstallPath == null)
                {
                    errmsg = "Failed to get push installer directory. Please check config logs.";
                    Logger.Debug(errmsg);
                    return false;
                }
                string filename = pushInstallPath + "\\pushinstaller.CONF";

                if (File.Exists(filename))
                {

                    var lines = File.ReadAllLines(filename);
                    int lineNumber = 0;

                    lineNumber = FindStringInLines(lines, "SignatureVerificationChecks");
                    if (lineNumber != 0)
                    {
                        string[] lineEditsplit = lines[lineNumber - 1].Split('=');

                        if (lineEditsplit.Length == 2)
                        {
                            string value = lineEditsplit[1];

                            if (value.Contains("1"))
                            {
                                isRequireSignatureVerificationChecks = true;
                                Logger.Debug("SignatureVerificationChecks key has value 1.");
                            }
                            else if (value.Contains("0"))
                            {
                                isRequireSignatureVerificationChecks = false;
                                Logger.Debug("SignatureVerificationChecks key has value 0");
                            }
                            else
                            {
                                errmsg = "Signature verification checks configuration has configured with invalid value in file " + filename;
                                ret = false;
                            }
                        }
                    }
                    else
                    {
                        Logger.Debug("SignatureVerificationChecks key not present in file " + filename);
                        //As pushinstaller by default does signature validation if SignatureVerificationChecks key is not present in pushinstaller.conf file , so returning as signature verification is requied if key is not present.
                        isRequireSignatureVerificationChecks = true;
                    }
                }
                else
                {
                    errmsg = "Could not find file " + filename + " to check the configuration settings if the Mobility Service software signature validation is required before installing on source machines.";
                    ret = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);

                Logger.Debug("ERROR*******************************************");
                Logger.Debug("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Logger.Debug("Exception  =  " + ex.Message);
                Logger.Debug("ERROR___________________________________________");

                errmsg = "Internal error occured while reading signature verification checks configuration settings in pushinstaller configuration file. Please check config logs.";
                ret = false;
                Logger.Close();
            }

            return ret;
        }

        /// <summary>
        /// Checks if Internet connectivity can be established using specified proxy settings.
        /// </summary>
        /// <param name="errorMessage">Out param to hold localized ErrorMessage.</param>
        /// <param name="webProxy">WebProxy object to be used as proxy for WebRequest.</param>
        /// <returns>True is internet connectivity could be established, false otherwise.</returns>
        public static bool IsInternetConnected(out string errorMessage, WebProxy webProxy = null)
        {
            // Set default system proxy
            WebRequest.DefaultWebProxy = WebRequest.GetSystemWebProxy();

            Logger.Debug("Creating web request to " + MicrosoftNCSIDotCom);
            HttpWebRequest webRequest = (HttpWebRequest)HttpWebRequest.Create(MicrosoftNCSIDotCom);
            webRequest.KeepAlive = false;

            if (webProxy != null)
            {
                Logger.Debug("Using webProxy to check internet connectivity");
                webRequest.Proxy = webProxy;
            }
            else
            {
                Logger.Debug("Using default proxy to check internet connectivity");
            }

            try
            {
                WebResponse webResponse = webRequest.GetResponse();
                webResponse.Close();
                webRequest.Abort();
                errorMessage = string.Empty;
                Logger.Debug("Proxy check passed");
                return true;
            }
            catch (WebException exp)
            {
                if (exp.Response != null)
                {
                    Logger.Debug("Failed to connect to MicrosoftNCSI.com - Response status code " + ((HttpWebResponse)exp.Response).StatusCode);
                    switch (((HttpWebResponse)exp.Response).StatusCode)
                    {
                        case HttpStatusCode.ProxyAuthenticationRequired:
                            errorMessage = ResourceHelper.Netfailure_ProxyAuthenticationRequired;
                            break;
                        default:
                            errorMessage = ResourceHelper.Netfailure_GenericFailure;
                            break;
                    }
                }
                else
                {
                    Logger.Debug("Failed to connect to MicrosoftNCSI.com - Request status code " + exp.Status);
                    switch (exp.Status)
                    {
                        case WebExceptionStatus.ConnectFailure:
                            errorMessage = ResourceHelper.Netfailure_GenericFailure;
                            break;
                        case WebExceptionStatus.NameResolutionFailure:
                            errorMessage = ResourceHelper.Netfailure_NameResolutionFailure;
                            break;
                        case WebExceptionStatus.ProxyNameResolutionFailure:
                            errorMessage = ResourceHelper.Netfailure_ProxyNameResolutionFailure;
                            break;
                        case WebExceptionStatus.RequestProhibitedByProxy:
                            errorMessage = ResourceHelper.Netfailure_RequestProhibitedByProxy;
                            break;
                        default:
                            errorMessage = ResourceHelper.Netfailure_GenericFailure;
                            break;
                    }
                }

                return false;
            }
            finally
            {
                // Reset system proxy settings
                WebRequest.DefaultWebProxy = WebRequest.GetSystemWebProxy();
            }
        }

        /// <summary>
        /// Build WebProxy object from values in PropertyBagDictionary.
        /// </summary>
        /// <returns>WebProxy object</returns>
        public static WebProxy BuildWebProxyFromPropertyBag(string proxyAddress, string proxyPort, string proxyUsername = "", string proxyPassword = "")
        {
            WebProxy webProxy = null;
            if (!string.IsNullOrEmpty(proxyAddress))
            {
                Logger.Debug("Using Proxy");
                UriBuilder buildURI;
                try
                {
                    buildURI = new UriBuilder(proxyAddress);
                    if (!string.IsNullOrEmpty(proxyPort))
                    {
                        buildURI.Port = Int32.Parse(proxyPort);
                    }

                    if (!string.IsNullOrEmpty(proxyUsername))
                    {
                        Logger.Debug("Proxy requires Authentication");
                        webProxy = new WebProxy(
                                                buildURI.Uri,
                                                true,
                                                new string[0],
                                                new NetworkCredential(proxyUsername, proxyPassword));
                    }
                    else
                    {
                        Logger.Debug("Proxy does not require Authentication");
                        webProxy = new WebProxy(buildURI.Uri);
                    }
                }
                catch
                {
                    Logger.Debug("Invalid Proxy Settings entered.");
                }
            }

            return webProxy;
        }

    }
}
