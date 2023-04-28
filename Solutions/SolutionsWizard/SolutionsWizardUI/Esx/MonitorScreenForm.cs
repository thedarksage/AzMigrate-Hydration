using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using com.InMage.Tools;
using System.Net;
using System.Web.SessionState;

using System.Web;


using System.Runtime.InteropServices;

using System.Web.UI;



namespace com.InMage.Wizard
{
    public partial class MonitorScreenForm : Form
    {
        private System.ComponentModel.Container components = null;
        public static Guid CLSID_InternetSecurityManager = new Guid("7b8a2d94-0ac9-11d1-896c-00c04fb6bfc4");
        public static Guid IID_IInternetSecurityManager = new Guid("79eac9ee-baf9-11ce-8c82-00aa004ba90b");

       // private IInternetSecurityManager _ism; 
        public MonitorScreenForm()
        {
            InitializeComponent();
            bool JavaScriptEnabled;
     
            
    
   
           
           
            webBrowserForCx.Navigate("http://" + WinTools.getCxIpWithPortNumber() + "/ui/");
            
            
        }
       
    }
}
