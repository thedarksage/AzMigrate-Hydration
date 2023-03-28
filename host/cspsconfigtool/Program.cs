using System;
using System.Windows.Forms;
using System.Threading;
using Logging;
using System.Runtime.InteropServices;
using System.Security.Principal;
//using System.IdentityModel.Claims;
using System.Security.Claims;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

namespace cspsconfigtool
{
    static class Program
    {
        // Constants
        internal const int SV_OK = 0;
        internal const int SV_FAIL = 1;

        internal const string CX_TYPE = "CX_TYPE";
        internal const string CX_TYPE_FOR_PS_ALONE = "\"2\"";

        [DllImport("configtool.dll", EntryPoint = "freeAllocatedBuf")]
        public static extern void freeAllocatedBuf(out IntPtr allocatedBuff);
        [DllImport("configtool.dll", EntryPoint = "GetValueFromAmethyst")]
        public static extern int GetValueFromAmethyst([MarshalAs(UnmanagedType.LPStr)] string key, out IntPtr value, out IntPtr err);

        // Constants
        internal const int WM_SETREDRAW = 0x0b;

        private static bool GetCSPSInstallPath(ref string cspsInstallPath)
        {
            try
            {
                string programDataPath = Environment.GetEnvironmentVariable("ProgramData");
                string installerConf = programDataPath + "\\Microsoft Azure Site Recovery\\Config\\App.conf";
                var data = new Dictionary<string, string>();
                foreach (var row in File.ReadAllLines(installerConf))
                {
                    if (row.Contains("="))
                    {
                        string key = row.Split('=')[0];
                        string value = row.Split('=')[1];
                        data.Add(key.Trim(), value.Trim(new Char[] { ' ', '"' }));
                    }
                }

                cspsInstallPath = data["INSTALLATION_PATH"];

                if (string.IsNullOrEmpty(cspsInstallPath))
                {
                    string error = "Failed to get the csps install path.Error: either the key INSTALLATION_PATH in  " + installerConf + " doesn't exist or it's value is empty.";
                    MessageBox.Show(error, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    return false;
                }
            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                string err = "Failed to get the csps install path. Error:";
                err += trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message;
                MessageBox.Show(err, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                return false;
            }
            return true;
        }

        private static bool IsAdmin()
        {
            WindowsIdentity id = WindowsIdentity.GetCurrent();
            if (id != null)
            {
                WindowsPrincipal p = new WindowsPrincipal(id);
                List<Claim> list = new List<Claim>(p.UserClaims);
                Claim clm = list.Find(pv => pv.Value.Equals("S-1-5-32-544"));
                if (clm != null)
                    return true;
            }
            return false;
        }

        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            string cspsInstallPath = string.Empty;
            if (!GetCSPSInstallPath(ref cspsInstallPath))
            {
                return;
            }

            TraceListener tl = new System.Diagnostics.TextWriterTraceListener(cspsInstallPath + "\\home\\svsystems\\var\\cspsconfigtool.log");
            Trace.Listeners.Add(tl);
            Trace.AutoFlush = true;

            //Logging.Logger.SetLogger();
            Logger.Debug("Entered main() of Program.cs");

            bool retrievedCxtype = true;
            string err = string.Empty;
            string cxtypevalue = string.Empty;

            IntPtr keyValue = IntPtr.Zero;
            IntPtr errmsg = IntPtr.Zero;
           
            int ret = SV_OK;
            bool enableInstallerMode = false;
            try
            {
                Dictionary<string, object> nameValues = CmdArgsParser.Parse(args);                
                foreach(KeyValuePair<string, object> nameValue in nameValues)
                {
                    if(String.Compare(nameValue.Key, CmdArgsParser.OptionEnableInstallerMode, true, ResourceHelper.DefaultCulture) == 0)
                    {
                        enableInstallerMode = true;
                    }
                    else
                    {
                        Logger.Error("Invalid command line option.");
                        MessageBox.Show("Entered command line option is not valid. \r\n\r\nUsage: \r\ncspsconfigtool.exe -EnableInstallerMode", "Invalid command line option", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        return;
                    }
                }
                
                ret = GetValueFromAmethyst(CX_TYPE, out keyValue, out errmsg);
                if (ret != SV_OK)
                {
                    Logger.Debug("Failed to get the CX_TYPE");
                    MessageBox.Show(Marshal.PtrToStringAnsi(errmsg), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    freeAllocatedBuf(out errmsg);
                    retrievedCxtype = false;
                }
                else
                {
                    cxtypevalue = Marshal.PtrToStringAnsi(keyValue);
                    string ptr = keyValue.ToString();
                    freeAllocatedBuf(out keyValue);
                    Logger.Debug("GetValue :" + cxtypevalue);
                }
            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                err = trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message;
                MessageBox.Show(err, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                Logger.Error("Caught Exception on Getting CX Type... ExcMsg: " + err);
                retrievedCxtype = false;
            }


            if (retrievedCxtype)
            {
                if (cxtypevalue.Equals(CX_TYPE_FOR_PS_ALONE, StringComparison.Ordinal))
                {
                    Logger.Debug("CX_TYPE is 2 : Running ps form.");

                    psform f1 = new psform();
                    f1.StartPosition = FormStartPosition.CenterScreen;
                    f1.initThread.Start(f1);

                    Application.Run(f1);
                }
                else
                {
                    Logger.Debug("CX_TYPE is not equal to" + CX_TYPE_FOR_PS_ALONE + " : Running cs form.");
                    if (IsAdmin())
                    {
                        Logger.Debug("Running cs form as Administrator.");
                        csform c1 = new csform(enableInstallerMode);
                        c1.StartPosition = FormStartPosition.CenterScreen;
                        c1.initThread.Start(c1);
                        Application.Run(c1);
                    }
                    else
                    {
                        Logger.Debug("User does not have Administrator permission.");
                        MessageBox.Show("Need to Run as Administrator.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }

           Logging.Logger.Close();
        }


        public static void InitUpdaterThreadFunction(object obj) {

            Services.Helpers hs = new Services.Helpers();
            string errmsg = string.Empty;
            psform ps = (psform)obj;
            ps.MakeInitialUpdateMsgVisible();

            if (hs.processinitUpdates(ref errmsg))
            {
                Logger.Debug("Successfully Processed Initial Conf Updates.");
                
                ps.MakeInitialUpdateMsgHide();
                ps.MakeReadOnlyFalse();
                ps.checkupdates = true;
            }
            else
            {
                Logger.Error("Failed to Process Initial Conf Updates error: " + errmsg);
                MessageBox.Show("Failed Process Initial Conf Updates error: " + errmsg);

                Logger.Error("Starting the Services");
                Thread svcstartThread = new Thread(svcThreadStartFunction);
                svcstartThread.Start();
                svcstartThread.Join();
                Logger.Close();
                System.Environment.Exit(1);

            }
        }

        private static void svcThreadStartFunction(object obj)
        {
            Services.Helpers hs = new Services.Helpers();
            string errmsg = string.Empty;
            psform ps = (psform)obj;
            ps.MakeSvcStopMsgVisible();

            if (hs.stopservices(ref errmsg))
            {
                Logger.Debug("Successfully stopped services.");
                ps.MakeSvcStopMsgHide();
                ps.stopsvc = true;
            }
            else
            {
                Logger.Error("Failed to stop services error: " + errmsg);
                MessageBox.Show("Failed to stop services error: " + errmsg);
                Logger.Close();
                System.Environment.Exit(1);

            }
            throw new NotImplementedException();
        }        
    }
}
