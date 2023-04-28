using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Web;
using System.Windows.Forms;
using System.Collections;
using System.Diagnostics;
using System.IO;

using System.Text.RegularExpressions;
using System.Xml;
using System.Threading;
using Com.Inmage.Esxcalls;
using Com.Inmage;
using Com.Inmage.Tools;
using System.Management;
using System.Security.AccessControl;


namespace Com.Inmage.Wizard
{
    // Adding enumeration for the selection of the starting combobox....

    public enum AppName { Esx = 1, Bmr = 2, Adddisks = 3, Failback = 4 ,Offlinesync = 5, Pushagent = 6,Recover = 9, Drdrill = 10,Resume = 11, V2P = 12, Resize = 13, Removedisk = 14, Removevolume = 15};
    //public enum Operation {ADDDISKS = 3, FAILBACK = 4, OFFICELINESYNC = 5 };
    public enum Selection { Esxpush = 7, P2vpush = 8 };
    
    public partial class AllServersForm : Form
    {
        // public AppName _appName = new AppName();

        //public AppName _appName;
        public CredentialsList credenList = new CredentialsList();
        //  public Esx _esxInfo;

        internal ArrayList panelListPrepared = new ArrayList();
        internal ArrayList panelHandlerListPrepared = new ArrayList();
        internal ArrayList pictureBoxListPrepared = new ArrayList();
        internal Thread childThread;
        internal Host masterHostAdded = new Host();
        internal int indexPrepared = 0;
        //public int _rowCount;
        internal TreeNode node;
        internal int taskListIndexPrepared = 1;
        internal int rowIndexPrepared;
        internal string cachedDomain = "", cachedUsername = "", cachedPassword = "";
        internal int SelectDiskColumn = 2;
        internal int listIndexPrepared = 0;
        internal string dataStoreCaptureDisplayNamePrepared;
        internal string sourceEsxIPinput = "";
        // public string _convertRdmDiskToVmdk           = "yes";
        internal string planInput = "";
        internal int blockSize;
        internal bool credentialsCaptured = false;
        internal bool dataStoreAvailable;
        internal HostList sourceCredentialsListFromUser = new HostList();
        internal int credentialIndexInput;
        internal int closedCalled = 0;
        internal OStype osTypeSelected = OStype.Windows;
        internal bool slideOpenHelp = false;
        internal bool closeFormForcelyByUser = false;
        internal bool thinProvisionSelected = true;
        internal static string ProtectionEsx = "1";
        internal string cxNatIPbyUser = null;
        internal static string FailbackHelp = "3";
        internal static string OfflinesyncHelp = "4";
        internal static string AddDiskHelp = "6";
        internal static string ProtectionP2vHelp = "10";
        internal static string PushagentHelp = "12";
        internal static string DrdrillHelp = "8";
        internal static bool SuppressMsgBoxes = false;
        internal bool natSetUpByUser = false;
        internal bool calledCancelByUser = false;
        internal static bool v2pRecoverySelected = false;
        internal bool wmiBasedRecvoerySelected = false;
        internal bool wmiWorkedFineForMt = false;
        internal string selectedMachineByUser = null;
        internal static bool arrayBasedDrdrillSelected = false;
        internal bool duplicateEntires = true;
        internal HostList duplicateListOfMachines = new HostList();
        internal Host dummyHost = new Host();
        internal string cxIPwithPortNumber = null;
        /* _selecteTargets is list of machine names used to dissplay in the drop down
         * list of SourceToMasterPanel
         */
        internal ArrayList selectedTargetsByUser = new ArrayList();

        internal HostList initialSourceListFromXml;
        internal HostList initialMasterListFromXml;
        internal Esx targetEsxInfoXml = new Esx();
        internal HostList selectedMasterListbyUser = new HostList();
        internal ArrayList nameCredentialsList = new ArrayList();


        /* List of selected primary machines. This is the main list of primary servers.
         */
        internal HostList selectedSourceListByUser = new HostList();

        internal HostList finalMasterListPrepared;         // = new HostList();
        internal HostList secondaryCredentialsListDefault = new HostList();
        internal string domain = null, username = null, password = null;
        internal string cxIPretrived;
        internal string currentDisplayedPhysicalHostIPselected = null;
        internal AppName appNameSelected;
        internal EsxList esxListRetrived;
        internal Selection selectionByUser;
        internal System.Drawing.Bitmap currentTaskInmage;
        internal System.Drawing.Bitmap completeTaskImage;
        internal System.Drawing.Bitmap openImage;
        internal System.Drawing.Bitmap closeImage;
        internal System.Drawing.Bitmap powerOffImage;
        internal System.Drawing.Bitmap poweronImage;
        internal System.Drawing.Bitmap hostImage;
        internal string installationPath;
        internal string cacheSubnetMaskPrepared = null;
        internal string cacheDnsIPprepared = null;
        internal string cacheNetworkNamePrepared = null;
        internal string cacheGateWayPrepared = null;





        //Copied from the recovery form.....*****
        internal string MasterFile;
        internal string RecoveryHostfile = "recovery_hostfile.txt";
        internal HostList sourceListByUser;
        int rowIndex = 0;
        internal string esxIPSelected;
        internal string tgtEsxUserName;
        internal string tgtEsxPassword;
        internal string currentDispName;
        internal HostList finalSelectedList = new HostList();
        internal HostList vmsSelectedRecovery = new HostList();
        internal HostList recoveryChecksPassedList = new HostList();
        ArrayList tagListSelected = new ArrayList();
        internal Esx esxSelected = new Esx();
        int closeForm = 0;
        internal string planNameSelected = "";
        internal string masterTargetDisplayName = "";
        internal string universalTime = "";
        internal bool someMtUsingFile = false;
        internal TreeNode nodeSelected;
        private delegate void TickerDelegate(string s);
        TickerDelegate tickerDelegate;
        internal TreeView p2VTreeViewWhenselected = new TreeView();
        internal System.Drawing.Bitmap scoutHeading;
        internal bool selectedMachineSuccessfully = false;
        internal int treeNodeIndex = 0;
        internal AllServersForm(string appName, string plan)
        {

            InitializeComponent();
            try
            {
                installationPath = WinTools.FxAgentPath() + "\\vContinuum";
                scoutHeading = Wizard.Properties.Resources.scoutheading;
                if (HelpForcx.Brandingcode == 1)
                {
                    headingPanel.BackgroundImage = scoutHeading;
                }

                planInput = plan;

                // ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler;
                // P2V_PrimaryServerPanelHandler p2v_PrimaryServerPanelHandler;

                rowIndexPrepared = 0;

                // if user select the esx protection from the combobox.. then we are adding the Esx_SourceToMasterPanelHandler to the panel list and panel handler list...

               
                if (appName == "ESX")
                {
                    this.Text = "vContinuum";

                    appNameSelected = AppName.Esx;

                    SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].HeaderText = "Primary Server Name";
                    //SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Visible = false;
                    ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler = new ESX_PrimaryServerPanelHandler();
                    panelHandlerListPrepared.Add(esx_PrimaryServerPanelHandler);
                    panelListPrepared.Add(addSourcePanel);



                }
                // if user select the BMR protection protection from the combobox.. then we are adding the P2V_PrimaryServerPanelHandler to the panel list and panel handler list...

                if (appName == "P2V")
                {
                    appNameSelected = AppName.Bmr;
                    SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].HeaderText = "Primary Server Ip";

                    P2V_PrimaryServerPanelHandler p2v_PrimaryServerPanelHandler = new P2V_PrimaryServerPanelHandler();
                    panelHandlerListPrepared.Add(p2v_PrimaryServerPanelHandler);
                    panelListPrepared.Add(p2v_PrimaryServerPanel);                  
                }
                if (appName == "v2p")
                {
                    appNameSelected = AppName.V2P;
                    p2vHeadingLabel.Text = "Failback protection";
                    this.Text = "Failback";

                    panelListPrepared.Add(addSourcePanel);
                    panelListPrepared.Add(p2v_PrimaryServerPanel);
                    panelListPrepared.Add(mapDiskPanel);
                   
                    P2V_PrimaryServerPanelHandler p2v_PrimaryServerPanelHandler = new P2V_PrimaryServerPanelHandler();
                    ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler = new ESX_PrimaryServerPanelHandler();
                    MapDiskPanelHandler mapDiskPanelHandler = new MapDiskPanelHandler();
                   
                    
                    panelHandlerListPrepared.Add(esx_PrimaryServerPanelHandler);
                    panelHandlerListPrepared.Add(p2v_PrimaryServerPanelHandler);
                    panelHandlerListPrepared.Add(mapDiskPanelHandler);

                    addCredentialsLabel.Text = "VM (s) to Failback";
                    selectPrimarySecondSourceLabel.Text = "Select Physical Server";
                    reviewTargetLabel.Text = "Protect";


                    pushAgentLabel.Visible = false;
                    protectionLabel.Visible = false;
                    protectLabel.Visible = false;
                    pictureBoxListPrepared.Add(credentialPictureBox);
                    pictureBoxListPrepared.Add(sourceTargetPicturceBox);
                    pictureBoxListPrepared.Add(configurePictureBox);

                }
                if (appName == "failBack")
                {
                    // As of now we are not showing the configuration screen for failback protection.
                    appNameSelected = AppName.Failback;
                    p2vHeadingLabel.Text = "Failback protection";
                    this.Text = "Failback";
                    SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].HeaderText = "Primary Server Name";
                    //SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Visible = false;
                    AdditionOfDiskSelectMachinePanelHandler additionOfDiskSelectMachinePanelHandler = new AdditionOfDiskSelectMachinePanelHandler();
                    ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler = new ESX_PrimaryServerPanelHandler();
                    if (ResumeForm.selectedVMListForPlan._hostList.Count == 0)
                    {
                        panelHandlerListPrepared.Add(additionOfDiskSelectMachinePanelHandler);
                    }
                    panelHandlerListPrepared.Add(esx_PrimaryServerPanelHandler);
                    if (ResumeForm.selectedVMListForPlan._hostList.Count == 0)
                    {
                        panelListPrepared.Add(additionOfDiskPanel);
                    }
                    panelListPrepared.Add(addSourcePanel);
                    if (ResumeForm.selectedVMListForPlan._hostList.Count != 0)
                    {
                        addCredentialsLabel.Text = "VM (s) to Failback";
                        selectPrimarySecondSourceLabel.Text = "Select Primary ESX";
                        reviewTargetLabel.Text = "Select Datastores";
                        pushAgentLabel.Text = "VM Configuration";
                        protectionLabel.Text = "Protect";
                        //protectionLabel.Text = "Protect";

                    }
                    else
                    {
                        addCredentialsLabel.Text = "Select Seconday ESX";
                        selectPrimarySecondSourceLabel.Text = "VM (s) to Failback";
                        reviewTargetLabel.Text = "Select Primary ESX";
                        pushAgentLabel.Text = "Select Datastores";
                        protectionLabel.Text = "Protect";
                        //protectLabel.Text = "Protect";

                        //protectLabel.Text = "Protect";
                    }
                    protectionLabel.Visible = false;
                }
                if (appName == "recovery")
                {
                    try
                    {
                        p2vHeadingLabel.Text = "Recover";
                        // reviewTargetLabel.Visible = false;
                        appNameSelected = AppName.Recover;
                        Trace.WriteLine(DateTime.Now + "\t Came here for the recovery");
                        //  _tagList.Add("FS");
                        // _tagList.Add("UserDefined");

                        dateTimePicker.Format = DateTimePickerFormat.Custom;
                        specificTimeDateTimePicker.Format = DateTimePickerFormat.Custom;
                        DateTime dt = DateTime.Now;


                        System.Globalization.CultureInfo culture = System.Globalization.CultureInfo.CurrentCulture;
                        System.Globalization.DateTimeFormatInfo info = culture.DateTimeFormat;

                        string datePattern = info.ShortDatePattern;

                        string timePattern = info.ShortTimePattern;

                        dateTimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
                        dateTimePicker.CustomFormat = datePattern + " " + timePattern;
                       // specificTimeDateTimePicker.CustomFormat = "yyyy/MM/dd H:mm:ss";





                        specificTimeDateTimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
                        specificTimeDateTimePicker.CustomFormat = datePattern + " " + timePattern;
                        if (ResumeForm.selectedActionForPlan == "ESX")
                        {
                            recoveryOsComboBox.Items.Add("Windows");
                            recoveryOsComboBox.Items.Add("Linux");

                        }
                        else
                        {
                            recoveryOsComboBox.Items.Add("Windows");

                        }
                        recoveryOsComboBox.SelectedItem = recoveryOsComboBox.Items[0];
                        // tagComboBox.DataSource = _tagList;

                        installationPath = WinTools.FxAgentPath() + "\\vContinuum";

                        panelListPrepared.Add(recoverPanel);
                        panelListPrepared.Add(configurationPanel);
                        panelListPrepared.Add(reviewPanel);

                        Esx_RecoverPanelHandler recoveryPanelHandler = new Esx_RecoverPanelHandler();
                        ReviewPanelHandler reviewPanelHandler = new ReviewPanelHandler();
                        RecoverConfigurationPanel configurationPanelHandler = new RecoverConfigurationPanel();

                        panelHandlerListPrepared.Add(recoveryPanelHandler);
                        panelHandlerListPrepared.Add(configurationPanelHandler);
                        panelHandlerListPrepared.Add(reviewPanelHandler);

                        pictureBoxListPrepared.Add(credentialPictureBox);
                        pictureBoxListPrepared.Add(sourceTargetPicturceBox);
                        pictureBoxListPrepared.Add(configurePictureBox);

                        currentTaskInmage = Wizard.Properties.Resources.arrow;
                        completeTaskImage = Wizard.Properties.Resources.doneIcon;
                        powerOffImage = Wizard.Properties.Resources.poweroff;
                        poweronImage = Wizard.Properties.Resources.poweron;
                        hostImage = Wizard.Properties.Resources.host;
                        addCredentialsLabel.Text = "Select Machines";
                        selectPrimarySecondSourceLabel.Text = "VM Configuration";
                        reviewTargetLabel.Text = "Recover";
                        pushAgentLabel.Visible = false;
                        protectionLabel.Visible = false;
                        protectLabel.Visible = false;
                        Esx_SourceToMasterPanelHandler._firstTimeSelection = false;
                        startApp();

                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: Initialize " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                }
                
                if (appName == "drdrill")
                {
                    try
                    {
                        p2vHeadingLabel.Text = "DR Drill";
                        // reviewTargetLabel.Visible = false;
                        appNameSelected = AppName.Drdrill;
                        Trace.WriteLine(DateTime.Now + "\t Came here for the recovery");
                        //  _tagList.Add("FS");
                        // _tagList.Add("UserDefined");

                        dateTimePicker.Format = DateTimePickerFormat.Custom;
                        specificTimeDateTimePicker.Format = DateTimePickerFormat.Custom;
                        DateTime dt = DateTime.Now;


                        //dateTimePicker.CustomFormat = "yyyy/MM/dd H:mm:ss";
                        //specificTimeDateTimePicker.CustomFormat = "yyyy/MM/dd H:mm:ss";
                        System.Globalization.CultureInfo culture = System.Globalization.CultureInfo.CurrentCulture;
                        System.Globalization.DateTimeFormatInfo info = culture.DateTimeFormat;

                        string datePattern = info.ShortDatePattern;

                        string timePattern = info.ShortTimePattern;

                        dateTimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
                        dateTimePicker.CustomFormat = datePattern + " " + timePattern;
                        // specificTimeDateTimePicker.CustomFormat = "yyyy/MM/dd H:mm:ss";





                        specificTimeDateTimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
                        specificTimeDateTimePicker.CustomFormat = datePattern + " " + timePattern;
                        if (ResumeForm.selectedActionForPlan == "ESX")
                        {
                            recoveryOsComboBox.Items.Add("Windows");
                            recoveryOsComboBox.Items.Add("Linux");

                        }
                        else
                        {
                            recoveryOsComboBox.Items.Add("Windows");

                        }
                        recoveryOsComboBox.SelectedItem = recoveryOsComboBox.Items[0];
                        // tagComboBox.DataSource = _tagList;

                        installationPath = WinTools.FxAgentPath() + "\\vContinuum";

                        panelListPrepared.Add(recoverPanel);
                        panelListPrepared.Add(configurationPanel);
                        panelListPrepared.Add(drDrillDatastroePanel);
                        panelListPrepared.Add(reviewPanel);

                        Esx_RecoverPanelHandler recoveryPanelHandler = new Esx_RecoverPanelHandler();
                        ReviewPanelHandler reviewPanelHandler = new ReviewPanelHandler();
                        DrDrillPanelHandler drDrillPanelHandlder = new DrDrillPanelHandler();
                        RecoverConfigurationPanel configurationPanelHandler = new RecoverConfigurationPanel();

                        panelHandlerListPrepared.Add(recoveryPanelHandler);
                        panelHandlerListPrepared.Add(configurationPanelHandler);
                        panelHandlerListPrepared.Add(drDrillPanelHandlder);
                        panelHandlerListPrepared.Add(reviewPanelHandler);

                        pictureBoxListPrepared.Add(credentialPictureBox);
                        pictureBoxListPrepared.Add(sourceTargetPicturceBox);
                        pictureBoxListPrepared.Add(configurePictureBox);
                        pictureBoxListPrepared.Add(pushAgentPictureBox);

                        currentTaskInmage = Wizard.Properties.Resources.arrow;
                        completeTaskImage = Wizard.Properties.Resources.doneIcon;
                        powerOffImage = Wizard.Properties.Resources.poweroff;
                        poweronImage = Wizard.Properties.Resources.poweron;
                        hostImage = Wizard.Properties.Resources.host;
                        addCredentialsLabel.Text = "Select Machines";
                        selectPrimarySecondSourceLabel.Text = "VM Configuration";
                        reviewTargetLabel.Text = "Select Datastore";
                        pushAgentLabel.Text = "DR Drill";
                        
                        protectionLabel.Visible = false;
                        protectLabel.Visible = false;
                        Esx_SourceToMasterPanelHandler._firstTimeSelection = false;
                        startApp();

                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: Initialize " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                }
                if (appName == "offlineSync")
                {

                    appNameSelected = AppName.Offlinesync;
                    if (ResumeForm.selectedActionForPlan == "Esx")
                    {
                        SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].HeaderText = "Primary Server Name";
                        //SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Visible = false;
                        ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler = new ESX_PrimaryServerPanelHandler();
                        panelHandlerListPrepared.Add(esx_PrimaryServerPanelHandler);
                        panelListPrepared.Add(addSourcePanel);
                    }
                    else
                    {
                        SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].HeaderText = "Primary Server Ip";

                        P2V_PrimaryServerPanelHandler p2v_PrimaryServerPanelHandler = new P2V_PrimaryServerPanelHandler();
                        panelHandlerListPrepared.Add(p2v_PrimaryServerPanelHandler);
                        panelListPrepared.Add(p2v_PrimaryServerPanel);

                    }


                }
                // if user select the incremental disk from the selection box we are adding only three panel for incremental disk..
                if (appName == "additionOfDisk")
                {

                    Debug.WriteLine("Printing Plan name" + plan + "====================");

                    SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].HeaderText = "Primary Server Name";
                   // SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Visible = false;


                    appNameSelected = AppName.Adddisks;
                    if (ResumeForm.selectedVMListForPlan._hostList.Count == 0)
                    {
                        panelListPrepared.Add(additionOfDiskPanel);
                    }
                    panelListPrepared.Add(addDiskPanel);
                    panelListPrepared.Add(p2v_sourceToMasterTargetPanel);
                    panelListPrepared.Add(p2v_ProtectionPanel);
                    AdditionOfDiskSelectMachinePanelHandler additionOfDiskSelectMachinePanelHandler = new AdditionOfDiskSelectMachinePanelHandler();
                    AddDiskPanelHandler addDiskPanelHandler = new AddDiskPanelHandler();
                    Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = new Esx_SourceToMasterPanelHandler();
                    Esx_ProtectionPanelHandler esx_ProtectionPanelHandler = new Esx_ProtectionPanelHandler();
                    if (ResumeForm.selectedVMListForPlan._hostList.Count == 0)
                    {
                        panelHandlerListPrepared.Add(additionOfDiskSelectMachinePanelHandler);
                    }
                    panelHandlerListPrepared.Add(addDiskPanelHandler);
                    panelHandlerListPrepared.Add(esx_SourceToMasterPanelHandler);
                    panelHandlerListPrepared.Add(esx_ProtectionPanelHandler);

                    pictureBoxListPrepared.Add(credentialPictureBox);

                    pictureBoxListPrepared.Add(sourceTargetPicturceBox);
                    pictureBoxListPrepared.Add(configurePictureBox);
                    if (ResumeForm.selectedVMListForPlan._hostList.Count == 0)
                    {
                        pushAgentPictureBox.Visible = true;
                        pushAgentLabel.Visible = true;
                        pictureBoxListPrepared.Add(pushAgentPictureBox);
                    }
                    else
                    {
                        pushAgentLabel.Visible = false;
                    }


                    //configurePictureBox.Visible = false;

                    protectionPictureBox.Visible = false;
                    //reviewTargetLabel.Visible                 = false;

                    protectionLabel.Visible = false;
                    addCredentialsLabel.Text = "Select Primary VM (s)";

                    selectPrimarySecondSourceLabel.Text = "Select Datastores";
                    reviewTargetLabel.Text = "Protect";
                    //pushAgentLabel.Text = "Protect";


                }
                else if (appName == "ESXPUSH")
                {
                    selectionByUser = Selection.Esxpush;
                    appNameSelected = AppName.Pushagent;
                    ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler = new ESX_PrimaryServerPanelHandler();
                    PushAgentPanelHandler p2v_PushAgentPanelHandler = new PushAgentPanelHandler();
                    // _panelHandlerList.Add(esx_PrimaryServerPanelHandler);
                    panelHandlerListPrepared.Add(p2v_PushAgentPanelHandler);
                    // _panelList.Add(addSourcePanel);
                    panelListPrepared.Add(p2v_PushAgentPanel);
                    pictureBoxListPrepared.Add(credentialPictureBox);
                    // _pictureBoxList.Add(sourceTargetPicturceBox);
                    reviewTargetLabel.Visible = false;
                    pushAgentLabel.Visible = false;
                    protectionLabel.Visible = false;
                    addCredentialsLabel.Text = "Select VM's";
                    selectPrimarySecondSourceLabel.Visible = false;

                }
                else if (appName == "P2VPUSH")
                {
                    appNameSelected = AppName.Pushagent;
                    selectionByUser = Selection.P2vpush;
                    //P2V_PrimaryServerPanelHandler p2v_PrimaryServerPanelHandler = new P2V_PrimaryServerPanelHandler();
                    PushAgentPanelHandler p2v_PushAgentPanelHandler = new PushAgentPanelHandler();
                    //_panelHandlerList.Add(p2v_PrimaryServerPanelHandler);
                    panelHandlerListPrepared.Add(p2v_PushAgentPanelHandler);
                    // _panelList.Add(p2v_PrimaryServerPanel);
                    panelListPrepared.Add(p2v_PushAgentPanel);
                    pictureBoxListPrepared.Add(credentialPictureBox);
                    // _pictureBoxList.Add(sourceTargetPicturceBox);
                    reviewTargetLabel.Visible = false;
                    pushAgentLabel.Visible = false;
                    protectionLabel.Visible = false;
                    addCredentialsLabel.Text = "Select Servers";
                    selectPrimarySecondSourceLabel.Visible = false;

                }
                else if (appNameSelected == AppName.Recover || appNameSelected == AppName.Drdrill || appNameSelected == AppName.V2P)
                {
                }
                else
                {
                    // adding panels to the panellist which is an arraylist...

                    panelListPrepared.Add(p2v_SelectPrimarySecondaryPanel);
                   // _panelList.Add(p2v_PushAgentPanel);
                    panelListPrepared.Add(p2v_sourceToMasterTargetPanel);
                    if ( appNameSelected == AppName.Offlinesync)
                    {

                    }
                    else
                    {
                        panelListPrepared.Add(configurationPanel);
                    }

                    panelListPrepared.Add(p2v_ProtectionPanel);

                    // these are panelhandler using as a seperate class for each panel...

                    Esx_SelectSecondaryPanelHandler esx_SelectSecondaryPanelHandler = new Esx_SelectSecondaryPanelHandler();
                    Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = new Esx_SourceToMasterPanelHandler();
                    //PushAgentPanelHandler p2v_PushAgentPanelHandler = new PushAgentPanelHandler();
                    Esx_ProtectionPanelHandler esx_ProtectionPanelHandler = new Esx_ProtectionPanelHandler();
                    ConfigurationPanelHandler configuration = new ConfigurationPanelHandler();
                    // adding panelnanlers to the array list....

                    panelHandlerListPrepared.Add(esx_SelectSecondaryPanelHandler);
                   // _panelHandlerList.Add(p2v_PushAgentPanelHandler);
                    panelHandlerListPrepared.Add(esx_SourceToMasterPanelHandler);
                    if ( appNameSelected == AppName.Offlinesync)
                    {



                    }
                    else
                    {
                        pushAgentLabel.Text = "VM Configuration";
                        protectionLabel.Text = "Protect";
                        protectionLabel.Visible = true;
                        protectionPictureBox.Visible = true;
                        panelHandlerListPrepared.Add(configuration);

                    }

                    panelHandlerListPrepared.Add(esx_ProtectionPanelHandler);


                    //adding picuture boxe to the array list to display the current position...

                    pictureBoxListPrepared.Add(credentialPictureBox);
                    pictureBoxListPrepared.Add(sourceTargetPicturceBox);
                    pictureBoxListPrepared.Add(configurePictureBox);
                    pictureBoxListPrepared.Add(pushAgentPictureBox);
                    pictureBoxListPrepared.Add(protectionPictureBox);

                    if (appNameSelected != AppName.Offlinesync && appNameSelected != AppName.Recover)
                    {
                       // protectPictureBox.Visible = true;
                       // protectLabel.Visible = true;
                        //_pictureBoxList.Add(protectPictureBox);
                    }
                    if (appNameSelected == AppName.Failback)
                    {
                        if (ResumeForm.selectedVMListForPlan._hostList.Count != 0)
                        {
                           // protectPictureBox.Visible = false;
                           // protectLabel.Visible = false;
                        }
                    }
                }
                // these are the icons which are used as the resources...
                powerOffImage = Wizard.Properties.Resources.poweroff;
                poweronImage = Wizard.Properties.Resources.poweron;
                hostImage = Wizard.Properties.Resources.host;
                currentTaskInmage = Wizard.Properties.Resources.arrow;
                completeTaskImage = Wizard.Properties.Resources.doneIcon;
                openImage = Wizard.Properties.Resources.helpOpen;
                closeImage = Wizard.Properties.Resources.helpClose;

                esxListRetrived = new EsxList();
                startApp();
                System.Windows.Forms.ToolTip toolTip = new System.Windows.Forms.ToolTip();
                toolTip.AutoPopDelay = 5000;
                toolTip.InitialDelay = 1000;
                toolTip.ReshowDelay = 500;
                toolTip.ShowAlways = true;
                toolTip.GetLifetimeService();
                toolTip.IsBalloon = false;
                toolTip.UseAnimation = true;
                toolTip.SetToolTip(versionLabel, HelpForcx.BuildDate);
                versionLabel.Text = HelpForcx.VersionNumber;
                if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum" + "\\patch.log"))
                {
                    patchLabel.Visible = true;
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


        private void startApp()
        {
            if (panelListPrepared.Count != panelHandlerListPrepared.Count)
            {
                Trace.WriteLine(DateTime.Now + "\t Panel list : " + panelListPrepared.Count + "\t handler list " + panelHandlerListPrepared.Count);
                MessageBox.Show("Panel Handler count it not matching with Panels.");
            }
            PanelHandler panelHandler = (PanelHandler)panelHandlerListPrepared[indexPrepared];
            panelHandler.Initialize(this);
            ((System.Windows.Forms.Panel)panelListPrepared[indexPrepared]).BringToFront();
        }

        internal void nextButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (indexPrepared < (panelListPrepared.Count))
                {
                    {
                        PanelHandler panelHandler = (PanelHandler)panelHandlerListPrepared[indexPrepared];
                        if (panelHandler.ValidatePanel(this) == true)
                        {
                            if (panelHandler.ProcessPanel(this) == true)
                            {
                                //Move to next panel if all states of current panel are done
                                if (panelHandler.CanGoToNextPanel(this) == true)
                                {
                                    // helpPanel.Visible = false;
                                    //Update the task progress on left side
                                    ((System.Windows.Forms.PictureBox)pictureBoxListPrepared[taskListIndexPrepared - 1]).Visible = true;
                                    if (indexPrepared < (panelListPrepared.Count - 1))
                                    {
                                        ((System.Windows.Forms.PictureBox)pictureBoxListPrepared[taskListIndexPrepared - 1]).Image = completeTaskImage;
                                        ((System.Windows.Forms.PictureBox)pictureBoxListPrepared[taskListIndexPrepared]).Visible = true;
                                        ((System.Windows.Forms.PictureBox)pictureBoxListPrepared[taskListIndexPrepared]).Image = currentTaskInmage;

                                        ((System.Windows.Forms.Panel)panelListPrepared[indexPrepared]).SendToBack();
                                        taskListIndexPrepared++;
                                        indexPrepared++;
                                        Console.WriteLine(" current index is :" + indexPrepared);
                                        panelHandler = (PanelHandler)panelHandlerListPrepared[indexPrepared];
                                        panelHandler.Initialize(this);
                                        // _slideOpen = false;
                                        ((System.Windows.Forms.Panel)panelListPrepared[indexPrepared]).BringToFront();
                                    }
                                }
                            }
                        }
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


        internal void previousButton_Click(object sender, EventArgs e)
        {
            try
            {
                // While moving to previous screen we need to change the icon of the picture box.
                PanelHandler panelHandler;
                if (indexPrepared > 0)
                {
                    if (taskListIndexPrepared > 0)
                    {
                        panelHandler = (PanelHandler)panelHandlerListPrepared[indexPrepared];
                        if (panelHandler.CanGoToPreviousPanel(this) == true)
                        {
                            ((System.Windows.Forms.Panel)panelListPrepared[indexPrepared]).SendToBack();
                            indexPrepared--;
                            taskListIndexPrepared--;
                            ((System.Windows.Forms.Panel)panelListPrepared[indexPrepared]).BringToFront();
                            panelHandler = (PanelHandler)panelHandlerListPrepared[indexPrepared];
                            ((System.Windows.Forms.PictureBox)pictureBoxListPrepared[taskListIndexPrepared]).Visible = false;
                            ((System.Windows.Forms.PictureBox)pictureBoxListPrepared[taskListIndexPrepared - 1]).Image = currentTaskInmage;
                            Debug.WriteLine(" current index is :" + indexPrepared);
                        }
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

        private void doneButton_Click(object sender, EventArgs e)
        {
            // whendone button is visible we are exiting out of the application ....
            // Esx_ProtectionPanelHandler esx_ProtectionPanelHandler = (Esx_ProtectionPanelHandler)_panelHandlerList[_index];            
            //Close the current wizard window. Main form still exists
            
           
            this.Close();
        }

        private void pushAgentPanelBrowserButton_Click(object sender, EventArgs e)
        {

        }

        private void pushAgentDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            Debug.WriteLine("Generated dirty cell event ");
            if (pushAgentDataGridView.RowCount > 0)
            {
                //WE need this commit so that CellValueChanged event handler to do the actual handling
                // This commit is essential for CellValue changed event handler to work. 
                pushAgentDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

     
        //Event handler that gets called when any cell in the Target/Secondary server grid is called
        //
      
        // This method will be called once user selects addip button on P2v_primaryPanel.
        private void primaryServerAddButton_Click(object sender, EventArgs e)
        {
            try
            {

                primaryServerAddButton.Enabled = false;
                progressBar.Visible = true;
                if (appNameSelected == AppName.V2P)
                {
                    selectedMasterListbyUser = new HostList();
                }
                else
                {
                    selectedSourceListByUser = new HostList();
                }
                p2vLoadingBackgroundWorker.RunWorkerAsync();

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

        //same as above method this method will be called when user press enter on password field
        private void passWordText_KeyUp(object sender, KeyEventArgs e)
        {
            try
            {
                // In p2v first screen if user click enter in password text box we will take that as a add button cilck only..
                if (e.KeyCode == System.Windows.Forms.Keys.Enter)
                {
                    P2V_PrimaryServerPanelHandler p2v_PrimaryServerPanelHandler = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                    p2v_PrimaryServerPanelHandler.AddIp(this);
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


       

        private void secondaryServerPanelAddEsxButton_Click(object sender, EventArgs e)
        {
            try
            {
                // This is in the second screen to get the target esx info.
                bool result;
                Esx_SelectSecondaryPanelHandler esx_SelectSecondaryPanelHandler = (Esx_SelectSecondaryPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_SelectSecondaryPanelHandler.powerOn = false;
                result = esx_SelectSecondaryPanelHandler.GetEsxGuestVmInfo(this);
                if (result == true)
                {
                    Debug.WriteLine("AddEsx Event Handler: Got follosing vms from esx ");
                    //_initialMasterList.Print();
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
            // secondaryServerDataGridView.Enabled = true;
        }


    
        private void pushAgentsButton_Click(object sender, EventArgs e)
        {
            //this is for pushagent to push agents to selected machines means silent install to selected machines...
            try
            {
                pushGetDeatsilsButton.Enabled = false;
                previousButton.Enabled = false;
                pushAgentDataGridView.Enabled = false;
                progressBar.Visible = true;
                pushUninstallButton.Enabled = false;
                pushAgentsButton.Enabled = false;
                nextButton.Enabled = false;
                doneButton.Visible = false;
                cancelButton.Visible = true;
                cancelButton.Enabled = true;
                pushAgentBackgroundWorker.RunWorkerAsync();
               

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




        private void cancelButton_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void physicalMachineDiskSizeDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (physicalMachineDiskSizeDataGridView.RowCount > 0)
            {

                physicalMachineDiskSizeDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void physicalMachineDiskSizeDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {

        }







        private void addEsxButton_Click(object sender, EventArgs e)
        {
            try
            {
                // When we click get deatils button in the first screen it will come here call scripts.....
                ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_PrimaryServerPanelHandler.powerOn = false;
                esx_PrimaryServerPanelHandler.GetEsxDetails(this);
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

       

      
        private void physicalMachineDiskSizeDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            //the disks are unselectable 
            try
            {
                if (e.RowIndex >= 0)
                {         //physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[SELECTDISK_COLUMN].Value = true;
                    // physicalMachineDiskSizeDataGridView.RefreshEdit();


                    // this comment will be removed whenthe selection of disks is supported.

                    if ((bool)physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[P2V_PrimaryServerPanelHandler.PHYSICAL_MACHINE_DISK_SELECT_COLUMN].FormattedValue)
                    {
                        if (physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[P2V_PrimaryServerPanelHandler.PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Selected == true)
                        {
                            Host tempHost = new Host();
                            tempHost.displayname = currentDisplayedPhysicalHostIPselected;
                            int listIndex = 0;
                            selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex);

                            if (osTypeSelected == OStype.Linux)
                            {
                                tempHost = (Host)selectedSourceListByUser._hostList[listIndex];
                                Hashtable hash = (Hashtable)tempHost.disks.list[e.RowIndex];
                                bool diskExists = false;
                                foreach (Hashtable diskHash in tempHost.disks.list)
                                {
                                    if (hash["DiskScsiId"] != null && diskHash["DiskScsiId"] != null)
                                    {
                                        if (hash["DiskScsiId"].ToString().Length >1 && diskHash["DiskScsiId"].ToString().Length > 1)
                                        {
                                            if (hash["DiskScsiId"].ToString() == diskHash["DiskScsiId"].ToString())
                                            {
                                                if (hash["src_devicename"] != null && hash["src_devicename"].ToString().Contains("/dev/mapper"))
                                                {
                                                    diskExists = true;
                                                    DiskDetails disk = new DiskDetails();
                                                    disk.okButton.Text = "No";
                                                    disk.descriptionLabel.Text = "Selected disk is multipath disk." + Environment.NewLine
                                                                            + "Are you sure to continue by selecting this disk for protection?";
                                                    //MessageBox.Show("Selected disk is multipath disk. Original disk is already selected for protection", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                                    disk.descriptionLabel.Visible = true;
                                                    disk.detailsDataGridView.Visible = false;
                                                    disk.ShowDialog();
                                                    if (disk.selectedtoContinue == true)
                                                    {
                                                        int index = 0;
                                                        foreach (Hashtable selectedHash in tempHost.disks.list)
                                                        {
                                                            Trace.WriteLine(DateTime.Now + "\t Selected ids are : " + selectedHash["DiskScsiId"].ToString() + " and " + hash["DiskScsiId"].ToString());
                                                            if (selectedHash["DiskScsiId"].ToString().Length > 1 && hash["DiskScsiId"].ToString().Length > 1)
                                                            {
                                                                if (selectedHash["DiskScsiId"].ToString() == hash["DiskScsiId"].ToString())
                                                                {
                                                                    Trace.WriteLine(DateTime.Now + " \t indexes " + e.RowIndex.ToString() + "\t RHS index" + index.ToString());
                                                                    if (e.RowIndex != index)
                                                                    {
                                                                        physicalMachineDiskSizeDataGridView.Rows[index].Cells[2].Value = false;
                                                                        physicalMachineDiskSizeDataGridView.RefreshEdit();
                                                                        ((Host)selectedSourceListByUser._hostList[listIndex]).UnselectDisk(index);

                                                                    }
                                                                }
                                                            }
                                                            index++;
                                                        }
                                                        ((Host)selectedSourceListByUser._hostList[listIndex]).SelectDisk(e.RowIndex);
                                                        Host selectedHost = (Host)selectedSourceListByUser._hostList[listIndex];
                                                        int count = 0;
                                                        foreach (Hashtable hashes in selectedHost.disks.list)
                                                        {
                                                            if (hashes["Selected"].ToString().ToUpper() == "YES")
                                                            {
                                                                count++;
                                                            }

                                                        }
                                                        selectedDiskLabel.Text = "Total number of disks selected: " + count.ToString();
                                                        return;
                                                    }
                                                    else
                                                    {
                                                        physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[P2V_PrimaryServerPanelHandler.PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = false;
                                                        physicalMachineDiskSizeDataGridView.RefreshEdit();
                                                        Host selectedHost = (Host)selectedSourceListByUser._hostList[listIndex];
                                                        int count = 0;
                                                        foreach (Hashtable hashes in tempHost.disks.list)
                                                        {
                                                            if (hashes["Selected"].ToString().ToUpper() == "YES")
                                                            {
                                                                count++;
                                                            }

                                                        }
                                                        selectedDiskLabel.Text = "Total number of disks selected: " + count.ToString();
                                                        return;
                                                    }
                                                }
                                                else if (hash["src_devicename"] != null && !hash["src_devicename"].ToString().Contains("/dev/mapper"))
                                                {
                                                    if (hash["src_devicename"].ToString() != diskHash["src_devicename"].ToString())
                                                    {
                                                        if (diskHash["Selected"].ToString() == "Yes")
                                                        {
                                                            DiskDetails disk = new DiskDetails();
                                                            disk.okButton.Text = "No";
                                                            disk.yesButton.Visible = true;
                                                            disk.descriptionLabel.Text = "Selected disk SCSI id " + hash["DiskScsiId"].ToString() + Environment.NewLine
                                                                                          + " has multipath disk which is already selected for protection." + Environment.NewLine
                                                                                          + "Do you want to unselect multipath disk?";
                                                            disk.detailsDataGridView.Visible = false;
                                                            disk.ShowDialog();
                                                            if (disk.selectedtoContinue == false)
                                                            {
                                                                physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[P2V_PrimaryServerPanelHandler.PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = false;
                                                                physicalMachineDiskSizeDataGridView.RefreshEdit();
                                                            }
                                                            else
                                                            {
                                                                int i = 0;
                                                                foreach (Hashtable diskHashs in tempHost.disks.list)
                                                                {
                                                                    if (diskHashs["DiskScsiId"].ToString() == diskHash["DiskScsiId"].ToString())
                                                                    {
                                                                        if (i != e.RowIndex)
                                                                        {
                                                                            ((Host)selectedSourceListByUser._hostList[listIndex]).UnselectDisk(i);
                                                                            physicalMachineDiskSizeDataGridView.Rows[i].Cells[P2V_PrimaryServerPanelHandler.PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = false;
                                                                            physicalMachineDiskSizeDataGridView.RefreshEdit();
                                                                        }
                                                                    }
                                                                    i++;
                                                                }
                                                                ((Host)selectedSourceListByUser._hostList[listIndex]).SelectDisk(e.RowIndex);
                                                                Host selectedHost = (Host)selectedSourceListByUser._hostList[listIndex];
                                                                int count = 0;
                                                                foreach (Hashtable hashes in tempHost.disks.list)
                                                                {
                                                                    if (hashes["Selected"].ToString().ToUpper() == "YES")
                                                                    {
                                                                        count++;
                                                                    }

                                                                }
                                                                selectedDiskLabel.Text = "Total number of disks selected: " + count.ToString();
                                                            }
                                                            return;
                                                        }
                                                    }

                                                }

                                            }
                                        }
                                    }
                                }
                                if (diskExists == false)
                                {
                                    ((Host)selectedSourceListByUser._hostList[listIndex]).SelectDisk(e.RowIndex);
                                    Host selectedHost = (Host)selectedSourceListByUser._hostList[listIndex];
                                    int count = 0;
                                    foreach (Hashtable hashes in selectedHost.disks.list)
                                    {
                                        if (hashes["Selected"].ToString().ToUpper() == "YES")
                                        {
                                            count++;
                                        }

                                    }
                                    selectedDiskLabel.Text = "Total number of disks selected: " + count.ToString();
                                }

                            }
                            else if (osTypeSelected == OStype.Windows)
                            {
                                ((Host)selectedSourceListByUser._hostList[listIndex]).SelectDisk(e.RowIndex);
                                Host selectedHost = (Host)selectedSourceListByUser._hostList[listIndex];

                                Hashtable hash = (Hashtable)selectedHost.disks.list[e.RowIndex];

                                if (hash["disk_signature"] != null && hash["cluster_disk"] != null && hash["cluster_disk"].ToString() == "yes")
                                {
                                    foreach (Host h in selectedSourceListByUser._hostList)
                                    {
                                        foreach (Hashtable sourceHash in h.disks.list)
                                        {
                                            if (sourceHash["disk_signature"] != null && hash["disk_signature"].ToString() == sourceHash["disk_signature"].ToString())
                                            {
                                                sourceHash["Selected"] = "Yes";
                                            }
                                        }
                                    }
                                }
                                int count = 0;
                                foreach (Hashtable hashes in selectedHost.disks.list)
                                {
                                    if (hashes["Selected"].ToString().ToUpper() == "YES")
                                    {
                                        count++;
                                    }

                                }
                                selectedDiskLabel.Text = "Total number of disks selected: " + count.ToString();
                            }

                        }
                    }
                    else
                    {

                        Host tempHost = new Host();
                        tempHost.displayname = currentDisplayedPhysicalHostIPselected;
                        int listIndex = 0;
                        selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex);
                        ((Host)selectedSourceListByUser._hostList[listIndex]).UnselectDisk(e.RowIndex);
                        Host selectedHost = (Host)selectedSourceListByUser._hostList[listIndex];
						
						Hashtable hash = (Hashtable)selectedHost.disks.list[e.RowIndex];

                        if (hash["disk_signature"] != null && hash["cluster_disk"] != null && hash["cluster_disk"].ToString() == "yes")
                        {
                            foreach (Host h in selectedSourceListByUser._hostList)
                            {
                                foreach (Hashtable sourceHash in h.disks.list)
                                {
                                    if (sourceHash["disk_signature"] != null && hash["disk_signature"].ToString() == sourceHash["disk_signature"].ToString())
                                    {
                                        sourceHash["Selected"] = "No";
                                    }
                                }
                            }
                        }

                        



						int count = 0;
                        foreach (Hashtable hashes in selectedHost.disks.list)
                        {
                            if (hashes["Selected"].ToString().ToUpper() == "YES")
                            {
                                count++;
                            }

                        }
                        selectedDiskLabel.Text = "Total number of disks selected: " + count.ToString();
                    }

                    if (physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[3].Selected == true)
                    {
                        Host tempHost = new Host();
                        tempHost.displayname = currentDisplayedPhysicalHostIPselected;
                        int listIndex = 0;
                        selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex);
                        tempHost = (Host)selectedSourceListByUser._hostList[listIndex];
                        Hashtable hash = (Hashtable)tempHost.disks.list[e.RowIndex];
                        if (osTypeSelected == OStype.Windows)
                        {
                            if (hash["src_devicename"] != null && hash["DiskScsiId"] != null)
                            {
                                DiskDetails disk = new DiskDetails();
                                disk.okButton.Text = "Ok";
                                disk.yesButton.Visible = false;                                
                                string message = "Disk details"+ Environment.NewLine + Environment.NewLine +
                                                 "Name " + hash["src_devicename"].ToString() +"."+ Environment.NewLine
                                                 + "SCSI ID of disk " + hash["DiskScsiId"].ToString();
                                disk.descriptionLabel.Text = "Disk deatils of " + physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[0].Value + ":";
                                disk.descriptionLabel.Visible = true;
                                disk.detailsDataGridView.Rows.Clear();

                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[0].Cells[0].Value = "Name";
                                disk.detailsDataGridView.Rows[0].Cells[1].Value = hash["src_devicename"].ToString();
                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[1].Cells[0].Value = "SCSI ID";
                                if (hash["DiskScsiId"].ToString().Length == 0)
                                {
                                    disk.detailsDataGridView.Rows[1].Cells[1].Value = "Not Available";
                                }
                                else
                                {
                                    disk.detailsDataGridView.Rows[1].Cells[1].Value = hash["DiskScsiId"].ToString();
                                }

                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[2].Cells[0].Value = "Size in KB(s)";
                                disk.detailsDataGridView.Rows[2].Cells[1].Value = hash["Size"].ToString();


                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[3].Cells[0].Value = "Type";
                                if (hash["efi"] != null)
                                {
                                    disk.detailsDataGridView.Rows[3].Cells[1].Value = hash["disk_type_os"].ToString() + ", Layout ->" + hash["DiskLayout"].ToString() + ", EFI -> " + hash["efi"].ToString();
                                }
                                else
                                {
                                    disk.detailsDataGridView.Rows[3].Cells[1].Value = hash["disk_type"].ToString() + ", Layout -> " + hash["DiskLayout"].ToString();
                                }


                                if (hash["volumelist"] != null)
                                {
                                    disk.detailsDataGridView.Rows.Add(1);
                                    disk.detailsDataGridView.Rows[4].Cells[0].Value = "Volumes in disk";
                                    disk.detailsDataGridView.Rows[4].Cells[1].Value = hash["volumelist"].ToString();

                                    if (hash["disk_signature"] != null)
                                    {
                                        disk.detailsDataGridView.Rows.Add(1);
                                        disk.detailsDataGridView.Rows[5].Cells[0].Value = "Disk Signature";
                                        disk.detailsDataGridView.Rows[5].Cells[1].Value = hash["disk_signature"].ToString();
                                    }
                                }
                                else
                                {
                                    if (hash["disk_signature"] != null)
                                    {
                                        disk.detailsDataGridView.Rows.Add(1);
                                        disk.detailsDataGridView.Rows[4].Cells[0].Value = "Disk Signature";
                                        disk.detailsDataGridView.Rows[4].Cells[1].Value = hash["disk_signature"].ToString();
                                    }
                                }

                                

                                disk.ShowDialog();
                                
                                //MessageBox.Show(message, "Info", MessageBoxButtons.OK, MessageBoxIcon.Information);
                            }
                            else if (hash["src_devicename"] != null && hash["DiskScsiId"] == null)
                            {
                                DiskDetails disk = new DiskDetails();
                                disk.okButton.Text = "Ok";
                                disk.yesButton.Visible = false;
                                //string message = "Disk details" + Environment.NewLine + Environment.NewLine + "Name " + hash["src_devicename"].ToString();
                                //disk.descriptionLabel.Text = message;
                                disk.detailsDataGridView.Visible = true;
                                disk.descriptionLabel.Text = "Disk deatils of " + physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[0].Value + ":";
                                disk.descriptionLabel.Visible = true;
                                disk.detailsDataGridView.Rows.Clear();

                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[0].Cells[0].Value = "Name";
                                disk.detailsDataGridView.Rows[0].Cells[1].Value = hash["src_devicename"].ToString();

                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[1].Cells[0].Value = "Size in KB(s)";
                                disk.detailsDataGridView.Rows[1].Cells[1].Value = hash["Size"].ToString();
                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[2].Cells[0].Value = "Type";
                                if (hash["efi"] != null)
                                {
                                    disk.detailsDataGridView.Rows[2].Cells[1].Value = hash["disk_type"].ToString() + ", Layout -> " + hash["DiskLayout"].ToString() + ", EFI -> " + hash["efi"].ToString();
                                }
                                else
                                {
                                    disk.detailsDataGridView.Rows[2].Cells[1].Value = hash["disk_type"].ToString() + ", Layout -> " + hash["DiskLayout"].ToString();
                                }

                                if (hash["volumelist"] != null)
                                {
                                    disk.detailsDataGridView.Rows.Add(1);
                                    disk.detailsDataGridView.Rows[3].Cells[0].Value = "Volumes in disk";
                                    disk.detailsDataGridView.Rows[3].Cells[1].Value = hash["volumelist"].ToString();

                                    if (hash["disk_signature"] != null)
                                    {
                                        disk.detailsDataGridView.Rows.Add(1);
                                        disk.detailsDataGridView.Rows[4].Cells[0].Value = "Disk Signature";
                                        disk.detailsDataGridView.Rows[4].Cells[1].Value = hash["disk_signature"].ToString();
                                    }
                                }
                                else
                                {
                                    if (hash["disk_signature"] != null)
                                    {
                                        disk.detailsDataGridView.Rows.Add(1);
                                        disk.detailsDataGridView.Rows[3].Cells[0].Value = "Disk Signature";
                                        disk.detailsDataGridView.Rows[3].Cells[1].Value = hash["disk_signature"].ToString();
                                    }
                                }
                                disk.ShowDialog();
                                //MessageBox.Show(message, "Info", MessageBoxButtons.OK, MessageBoxIcon.Information);
                            }
                            else if (hash["src_devicename"] == null && hash["DiskScsiId"] != null)
                            {
                                DiskDetails disk = new DiskDetails();
                                disk.okButton.Text = "Ok";
                                disk.yesButton.Visible = false;
                                string message = "Disk details" + Environment.NewLine + Environment.NewLine +"SCSI ID of disk " + hash["DiskScsiId"].ToString();
                                //MessageBox.Show(message, "Info", MessageBoxButtons.OK, MessageBoxIcon.Information);
                                disk.detailsDataGridView.Visible = true;
                                disk.descriptionLabel.Text = "Disk deatils of " + physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[0].Value + ":";
                                disk.descriptionLabel.Visible = true;
                                disk.detailsDataGridView.Rows.Clear();

                               
                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[0].Cells[0].Value = "SCSI ID";
                                if (hash["DiskScsiId"].ToString().Length == 0)
                                {
                                    disk.detailsDataGridView.Rows[0].Cells[1].Value = "Not Available";
                                }
                                else
                                {
                                    disk.detailsDataGridView.Rows[0].Cells[1].Value = hash["DiskScsiId"].ToString();
                                }
                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[1].Cells[0].Value = "Size in KB(s)";
                                disk.detailsDataGridView.Rows[1].Cells[1].Value = hash["Size"].ToString();

                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[2].Cells[0].Value = "Type";
                                if (hash["efi"] != null)
                                {
                                    disk.detailsDataGridView.Rows[2].Cells[1].Value = hash["disk_type"].ToString() + ", Layout -> " + hash["DiskLayout"].ToString() + ", EFI ->" + hash["efi"].ToString();
                                }
                                else
                                {
                                    disk.detailsDataGridView.Rows[2].Cells[1].Value = hash["disk_type"].ToString() + ", Layout -> " + hash["DiskLayout"].ToString();
                                }

                                if (hash["volumelist"] != null)
                                {
                                    disk.detailsDataGridView.Rows.Add(1);
                                    disk.detailsDataGridView.Rows[3].Cells[0].Value = "Volumes in disk";
                                    disk.detailsDataGridView.Rows[3].Cells[1].Value = hash["volumelist"].ToString();

                                    if (hash["disk_signature"] != null)
                                    {
                                        disk.detailsDataGridView.Rows.Add(1);
                                        disk.detailsDataGridView.Rows[4].Cells[0].Value = "Disk Signature";
                                        disk.detailsDataGridView.Rows[4].Cells[1].Value = hash["disk_signature"].ToString();
                                    }
                                }
                                else
                                {
                                    if (hash["disk_signature"] != null)
                                    {
                                        disk.detailsDataGridView.Rows.Add(1);
                                        disk.detailsDataGridView.Rows[3].Cells[0].Value = "Disk Signature";
                                        disk.detailsDataGridView.Rows[3].Cells[1].Value = hash["disk_signature"].ToString();
                                    }
                                }
                                disk.ShowDialog();
                            }
                        }
                        else if(osTypeSelected == OStype.Linux)
                        {
                            if (hash["src_devicename"] != null && hash["DiskScsiId"] != null)
                            {
                                DiskDetails disk = new DiskDetails();
                                disk.okButton.Text = "Ok";
                                disk.yesButton.Visible = false;
                                disk.descriptionLabel.Text = "Disk deatils of " + physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[0].Value + ":";
                                disk.descriptionLabel.Visible = true;
                                disk.detailsDataGridView.Rows.Clear();

                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[0].Cells[0].Value = "Name";
                                disk.detailsDataGridView.Rows[0].Cells[1].Value = hash["src_devicename"].ToString();
                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[1].Cells[0].Value = "SCSI ID";
                                if (hash["DiskScsiId"].ToString().Length == 0)
                                {
                                    disk.detailsDataGridView.Rows[1].Cells[1].Value = "Not Available";
                                }
                                else
                                {
                                    disk.detailsDataGridView.Rows[1].Cells[1].Value = hash["DiskScsiId"].ToString();
                                }
                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[2].Cells[0].Value = "Size in KB(s)";
                                disk.detailsDataGridView.Rows[2].Cells[1].Value = hash["Size"].ToString();


                                if (hash["disk_signature"] != null)
                                {
                                    disk.detailsDataGridView.Rows.Add(1);
                                    disk.detailsDataGridView.Rows[3].Cells[0].Value = "Disk Signature";
                                    disk.detailsDataGridView.Rows[3].Cells[1].Value = hash["disk_signature"].ToString();
                                }

                                string devmapperDevises = null;
                                foreach (Hashtable tempHash in tempHost.disks.list)
                                {
                                    if (hash["src_devicename"] != null & tempHash["src_devicename"] != null)
                                    {
                                        if (hash["src_devicename"].ToString() != tempHash["src_devicename"].ToString())
                                        {
                                            if (hash["DiskScsiId"] != null && tempHash["DiskScsiId"] != null && hash["DiskScsiId"].ToString().Length >1 && tempHash["DiskScsiId"].ToString().Length >1)
                                            {
                                                if (hash["DiskScsiId"].ToString() == tempHash["DiskScsiId"].ToString())
                                                {
                                                    if (devmapperDevises == null)
                                                    {
                                                        devmapperDevises = tempHash["src_devicename"].ToString();
                                                    }
                                                    else
                                                    {
                                                        devmapperDevises = devmapperDevises + tempHash["src_devicename"].ToString();
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                if (devmapperDevises != null)
                                {

                                    disk.detailsDataGridView.Rows.Add(1);
                                    int rowcount = disk.detailsDataGridView.RowCount;
                                    disk.detailsDataGridView.Rows[rowcount-1].Cells[0].Value = "Multipath disks";
                                    disk.detailsDataGridView.Rows[rowcount - 1].Cells[1].Value = devmapperDevises; ;
                                }
                                disk.noteLabel.Visible = true;
                                disk.ShowDialog();
                            }
                            else if (hash["src_devicename"] != null && hash["DiskScsiId"] == null)
                            {
                                DiskDetails disk = new DiskDetails();
                                disk.okButton.Text = "Ok";
                                disk.yesButton.Visible = false;
                                disk.detailsDataGridView.Visible = true;
                                disk.descriptionLabel.Text = "Disk deatils of " + physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[0].Value + ":";
                                disk.descriptionLabel.Visible = true;
                                disk.detailsDataGridView.Rows.Clear();

                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[0].Cells[0].Value = "Name";
                                disk.detailsDataGridView.Rows[0].Cells[1].Value = hash["src_devicename"].ToString();
                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[1].Cells[0].Value = "Size in KB(s)";
                                disk.detailsDataGridView.Rows[1].Cells[1].Value = hash["Size"].ToString();


                                if (hash["disk_signature"] != null)
                                {
                                    disk.detailsDataGridView.Rows.Add(1);
                                    disk.detailsDataGridView.Rows[2].Cells[0].Value = "Disk Signature";
                                    disk.detailsDataGridView.Rows[2].Cells[1].Value = hash["disk_signature"].ToString();
                                }
                                disk.descriptionLabel.Visible = false;
                                disk.noteLabel.Visible = true;
                                disk.ShowDialog();
                            }
                            else if (hash["src_devicename"] == null && hash["DiskScsiId"] != null)
                            {
                                DiskDetails disk = new DiskDetails();
                                disk.okButton.Text = "Ok";
                                disk.yesButton.Visible = false;
                                disk.detailsDataGridView.Visible = true;
                                disk.descriptionLabel.Text = "Disk deatils of " + physicalMachineDiskSizeDataGridView.Rows[e.RowIndex].Cells[0].Value + ":";
                                disk.descriptionLabel.Visible = true;
                                disk.detailsDataGridView.Rows.Clear();


                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[0].Cells[0].Value = "SCSI ID";
                                if (hash["DiskScsiId"].ToString().Length == 0)
                                {
                                    disk.detailsDataGridView.Rows[0].Cells[1].Value = "Not Available";
                                }
                                else
                                {
                                    disk.detailsDataGridView.Rows[0].Cells[1].Value = hash["DiskScsiId"].ToString();
                                }
                                disk.detailsDataGridView.Rows.Add(1);
                                disk.detailsDataGridView.Rows[1].Cells[0].Value = "Size in KB(s)";
                                disk.detailsDataGridView.Rows[1].Cells[1].Value = hash["Size"].ToString();


                                if (hash["disk_signature"] != null)
                                {
                                    disk.detailsDataGridView.Rows.Add(1);
                                    disk.detailsDataGridView.Rows[2].Cells[0].Value = "Disk Signature";
                                    disk.detailsDataGridView.Rows[2].Cells[1].Value = hash["disk_signature"].ToString();
                                }
                                disk.noteLabel.Visible = true;
                                disk.ShowDialog();
                            }
                        }
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



        private void primaryServerPanelIpText_MouseHover(object sender, EventArgs e)
        {
            // helptextBox.Text = "Enter Physical Machine IP address";
        }

        private void primaryServerPanelIpText_MouseLeave(object sender, EventArgs e)
        {
            // helptextBox.Text = null;
        }

      
        private void SourceToMasterDataGridView_SelectionChanged(object sender, EventArgs e)
        {
        }

        private void retentionPathText_KeyUp(object sender, KeyEventArgs e)
        {
            try
            {
                Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_SourceToMasterPanelHandler.DataStoreChanges(this);
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

        private void retentionLogSizeText_KeyUp(object sender, KeyEventArgs e)
        {
            try
            {
                Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_SourceToMasterPanelHandler.DataStoreChanges(this);
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

        private void retentionText_KeyUp(object sender, KeyEventArgs e)
        {
            try
            {
                Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_SourceToMasterPanelHandler.DataStoreChanges(this);
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

        private void consistencyHourCombobox_SelectionChangeCommitted(object sender, EventArgs e)
        {
            try
            {
                Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_SourceToMasterPanelHandler.DataStoreChanges(this);
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

        private void consistencyMinuteCombobox_SelectionChangeCommitted(object sender, EventArgs e)
        {
            try
            {
                Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_SourceToMasterPanelHandler.DataStoreChanges(this);
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



        private void dataStoreDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {


        }

        private void consistencyDayComboBox_SelectionChangeCommitted(object sender, EventArgs e)
        {
            try
            {
                Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_SourceToMasterPanelHandler.DataStoreChanges(this);
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

        private void processServerIpText_KeyUp(object sender, KeyEventArgs e)
        {
            try
            {
                Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_SourceToMasterPanelHandler.DataStoreChanges(this);
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

        private void dataStoreDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (dataStoreDataGridView.RowCount > 0)
            {

                dataStoreDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void SourceToMasterDataGridView_CellClick(object sender, DataGridViewCellEventArgs e)
        { // if user selects the first column we are displaying the values selectedby the user or displaying as the empty...
            try
            {
                string ip;

                if (e.RowIndex >= 0)
                {
                    if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_PATH_COLUMN].Selected == true)
                    {

                        SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_PATH_COLUMN].DataGridView.DefaultCellStyle.Font = new Font(SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_PATH_COLUMN].DataGridView.DefaultCellStyle.Font, FontStyle.Regular);
                        SourceToMasterDataGridView.RefreshEdit();
                    }


                    // this is for the selection of the BMR Protection....
                    if ((appNameSelected == AppName.Bmr && osTypeSelected == OStype.Windows) || (appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v" && osTypeSelected == OStype.Windows))
                    {
                        //if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Value.ToString() == "Provide OS CD")
                        //{
                        //    foreach (Host h in _selectedSourceList._hostList)
                        //    {
                        //        //for the bmr protection we are comparing with the ip values....

                        //        ip = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();

                        //        if (h.ip == ip)
                        //        {

                        //            if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Selected == true)
                        //            {
                        //                ip = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].ToString();
                        //                Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)_panelHandlerList[_index];
                        //                if (esx_SourceToMasterPanelHandler.CopyDriversFromOsCD(ip, this, h.operatingSystem, e.RowIndex) == true)
                        //                {
                        //                    // SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Value = "Available";

                        //                }

                        //            }
                        //        }
                        //    }



                        //}
                    }


                    //if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Selected == true)
                    {



                        Host h = (Host)selectedSourceListByUser._hostList[e.RowIndex];

                        listIndexPrepared = e.RowIndex;

                        if (appNameSelected == AppName.Bmr || (appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                        {

                            h.ip = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                            dataStoreCaptureDisplayNamePrepared = h.ip;
                            // detailsOfDataStoreGridViewLabel.Text =  "Select datastore for " + h.ip;
                            if (h.targetDataStore != null)
                            {
                                // detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.ip + " is... ";
                            }
                        }


                        if (appNameSelected == AppName.Esx)
                        {
                            h.displayname = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                            dataStoreCaptureDisplayNamePrepared = h.displayname;
                            // detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.displayname;
                            if (h.targetDataStore != null)
                            {
                                //detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.displayname + " is... ";
                            }
                        }
                        if (appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "ESX")
                        {
                            h.displayname = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                            dataStoreCaptureDisplayNamePrepared = h.displayname;
                            // detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.displayname;
                            if (h.targetDataStore != null)
                            {
                                // detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.displayname + " is... ";
                            }
                        }
                        if (appNameSelected == AppName.Failback)
                        {
                            h.displayname = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                            dataStoreCaptureDisplayNamePrepared = h.displayname;
                            // detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.displayname;
                            if (h.targetDataStore != null)
                            {
                                dataStoreDataGridView.Enabled = false;
                            }
                            if (h.targetDataStore != null)
                            {
                                // detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.displayname + " is... ";
                            }

                        }
                        if (appNameSelected == AppName.Offlinesync)
                        {
                            if (ResumeForm.selectedActionForPlan == "Esx")
                            {
                                h.displayname = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                dataStoreCaptureDisplayNamePrepared = h.displayname;
                                //  detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.displayname;
                                if (h.targetDataStore != null)
                                {
                                    //detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.displayname + " is... ";
                                }
                            }
                            else
                            {
                                h.ip = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                dataStoreCaptureDisplayNamePrepared = h.ip;
                                //detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.ip;
                                if (h.targetDataStore != null)
                                {
                                    // detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.ip + " is... ";
                                }
                            }
                        }


                        // if datastore value is not null we are displaying the selected datastore to the user....

                        if (h.targetDataStore != null)
                        {
                            for (int i = 0; i < dataStoreDataGridView.RowCount; i++)
                            {
                                if (h.targetDataStore != dataStoreDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.DATASTORE_NAME_COLUMN].Value.ToString())
                                {


                                    Debug.WriteLine(DateTime.Now + " \t Data store matchs in the grid");
                                    //dataStoreDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.DATASOURCE_SELECTED_COLUMN].Value = false;
                                }
                                else
                                {
                                    //if datastore contains in the host is equal to the datagridview we are making the checkbox as true....
                                    SourceToMasterDataGridView.RefreshEdit();
                                    //dataStoreDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.DATASOURCE_SELECTED_COLUMN].Value = true;
                                    dataStoreDataGridView.RefreshEdit();
                                }
                            }


                        }

                        if (h.targetDataStore == null)
                        {
                            for (int i = 0; i < dataStoreDataGridView.RowCount; i++)
                            {
                                //if datastroe is null we are making entire datagridview checkboxes as false....

                                //  dataStoreDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.DATASOURCE_SELECTED_COLUMN].Value = false;
                            }
                        }
                        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.UNSELECT_MACHINE_COLUMN].Selected == true)
                        {
                            Esx_SourceToMasterPanelHandler sourceToMAsterTargetPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                            sourceToMAsterTargetPanelHandler.UnselctMachine(this, e.RowIndex);
                        }
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

        private void AllServersForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            try
            {
                // this will call when ever user clicks on the clickbutton or on the control box....

                if (closedCalled == 0)
                {
                    if (closeFormForcelyByUser == false)
                    {
                        if (doneButton.Visible == false)
                        {
                            switch (MessageBox.Show("Are you sure you want to exit?", "vContinuum", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                            {
                                case DialogResult.Yes:
                                    FindAndKillProcess();
                                    calledCancelByUser = true;
                                    Trace.WriteLine(DateTime.Now + " \t Closing the AllServersFrom");
                                    closedCalled = 1;
                                    this.Close();
                                    break;
                                case DialogResult.No:
                                    e.Cancel = true;
                                    break;

                            }
                        }
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



        private void retentionLogSizeText_KeyPress(object sender, KeyPressEventArgs e)
        {
            // for the retention size only number keys can work....
            try
            {
                e.Handled = !Char.IsNumber(e.KeyChar) && (e.KeyChar != '\b');
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

        private void retentionDaysText_KeyPress(object sender, KeyPressEventArgs e)
        {
            // for the retention size only number keys can work....
            try
            {
                e.Handled = !Char.IsNumber(e.KeyChar) && (e.KeyChar != '\b');
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







        private void esxDiskInfoDatagridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (esxDiskInfoDatagridView.RowCount > 0)
            {

                esxDiskInfoDatagridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }

        }

        private void esxDiskInfoDatagridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            try
            {

                // Making the datagridview checkboxes as un selectable...

                if (esxDiskInfoDatagridView.RowCount > 0)
                {
                    if (esxDiskInfoDatagridView.Rows[e.RowIndex].Cells[SelectDiskColumn].Selected == true)
                    {
                        //esxDiskInfoDatagridView.Rows[e.RowIndex].Cells[SELECTDISK_COLUMN].Value = true;

                        // esxDiskInfoDatagridView.RefreshEdit();
                        // this comment will be removed whenthe selection of disks is supported.

                        Debug.WriteLine("Printing the display name " + currentDisplayedPhysicalHostIPselected + " " + e.RowIndex);
                        ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                        esx_PrimaryServerPanelHandler.SelectDiskOrUnSelectDisk(this, e.RowIndex);

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

      
        private void addDiskDetailsDataGridView_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            //this is the datagridview which is used in the adddisk panel for incremental disk support....
            try
            {
                Host h = new Host();
                if (e.RowIndex >= 0)
                {
                    if (selectedSourceListByUser._hostList.Count > 0)
                    {
                        h.displayname = addDiskDetailsDataGridView.Rows[e.RowIndex].Cells[AddDiskPanelHandler.SourceDisplaynameColumn].Value.ToString();
                        currentDisplayedPhysicalHostIPselected = h.displayname;
                        AddDiskPanelHandler addDiskPanelHandler = (AddDiskPanelHandler)panelHandlerListPrepared[indexPrepared];

                        foreach (Host h1 in selectedSourceListByUser._hostList)
                        {
                            // comparing the display name in host and in the datagridview if equal we are displaying the disk details...

                            if (h1.displayname == h.displayname)
                            {

                                addDiskPanelHandler.AddingDiskDetails(this, h1);
                                break;
                            }
                            else
                            {
                                // if comparision fails we are making datagridview visble as false....


                                Trace.WriteLine(DateTime.Now + " \t came to know that display name is not matched  " + h1.displayname + h.displayname);
                                newDiskDetailsDataGridView.Visible = false;
                            }

                        }
                    }
                    else
                    {
                        //if hostlist is null we are making diskdetials of datagridview visible as false...
                        Trace.WriteLine(DateTime.Now + " \t came to know that primary list is empty");
                        newDiskDetailsDataGridView.Visible = false;
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

        private void newDiskDetailsDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (newDiskDetailsDataGridView.RowCount > 0)
            {

                newDiskDetailsDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }

        }

        private void newDiskDetailsDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            // this is a datagrid view events for to show user how many disks has been added and they are unselectable....
            try
            {
                if (newDiskDetailsDataGridView.RowCount > 0)
                {
                    if (newDiskDetailsDataGridView.Rows[e.RowIndex].Cells[AddDiskPanelHandler.AdditionDiskCheckBoxColumn].Selected == true)
                    {
                        if (newDiskDetailsDataGridView.Rows[e.RowIndex].Cells[AddDiskPanelHandler.ProtectedDiskColumn].Value.ToString() == "No")
                        {
                            // newDiskDetailsDataGridView.Rows[e.RowIndex].Cells[AddDiskPanelHandler.ADDTION_DISK_DISK_CHECKBOX_COLUMN].Value = true;
                            // newDiskDetailsDataGridView.RefreshEdit();
                            AddDiskPanelHandler esx_PrimaryServerPanelHandler = (AddDiskPanelHandler)panelHandlerListPrepared[indexPrepared];
                            esx_PrimaryServerPanelHandler.SelectDiskOrUnSelectDisk(this, e.RowIndex);
                        }
                        else
                        {
                            newDiskDetailsDataGridView.Rows[e.RowIndex].Cells[AddDiskPanelHandler.AdditionDiskCheckBoxColumn].Value = false;
                            newDiskDetailsDataGridView.RefreshEdit();
                        }

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


        private void addDiskDetailsDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (addDiskDetailsDataGridView.RowCount > 0)
            {

                addDiskDetailsDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void dataStoreDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            /* if (e.RowIndex >= 0)
             {
                 if (dataStoreDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.DATASOURCE_SELECTED_COLUMN].Selected == true)
                 {



                     for (int i = 0; i < dataStoreDataGridView.RowCount; i++)
                     {
                         if (i != e.RowIndex)
                         {

                             dataStoreDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.DATASOURCE_SELECTED_COLUMN].Value = false;
                             dataStoreDataGridView.RefreshEdit();

                         }

                     }

                     Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)_panelHandlerList[_index];
                     esx_SourceToMasterPanelHandler.DataSotreDataGridViewValues(this, e.RowIndex);

                 }
             }*/

        }

        private void MasterTargetBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx_SelectSecondaryPanelHandler esx_SelectSecondaryPanelHandler = (Esx_SelectSecondaryPanelHandler)panelHandlerListPrepared[indexPrepared];
                if (esx_SelectSecondaryPanelHandler.MTPreCheck(this) == true)
                {
                    
                   // esx_SelectSecondaryPanelHandler.BackGroundWorkerForMasterTargetPreReq(this);
                }
                else
                {
                   
                    
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

        private void MasterTargetBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;

        }

        private void MasterTargetBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                Esx_SelectSecondaryPanelHandler secondaryPanelHandler = (Esx_SelectSecondaryPanelHandler)panelHandlerListPrepared[indexPrepared];

                nextButton.Enabled = true;
                previousButton.Enabled = true;
                p2v_SelectPrimarySecondaryPanel.Enabled = true;
                // this.Enabled = true;
                this.BringToFront();
                progressBar.Visible = false;
                if (Esx_SelectSecondaryPanelHandler._useVmAsMT == false)
                {
                    for (int i = 0; i < selectMasterTargetTreeView.Nodes.Count; i++)
                    {
                        // allServersForm.selectMasterTargetTreeView.Nodes[i].Checked = false;
                        for (int j = 0; j < selectMasterTargetTreeView.Nodes[i].Nodes.Count; j++)
                        {
                            selectMasterTargetTreeView.Nodes[i].Nodes[j].Checked = false;
                        }
                    }
                }
               // secondaryPanelHandler.PostCredentialsCheck(this);
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

        private void esxDetailsBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler;
                if (appNameSelected != AppName.Pushagent)
                {
                    esx_PrimaryServerPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                }
                else
                {
                    esx_PrimaryServerPanelHandler = new ESX_PrimaryServerPanelHandler();
                    esx_PrimaryServerPanelHandler.powerOn = false;
                }
                if (esx_PrimaryServerPanelHandler.powerOn == true)
                {
                    esx_PrimaryServerPanelHandler.PowerOnVm(this);


                    //esx_PrimaryServerPanelHandler.GetDetailsBackGroundWorker(this);
                }
                else
                {

                    esx_PrimaryServerPanelHandler.GetDetailsBackGroundWorker(this);

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

        private void esxDetailsBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {

            progressBar.Value = e.ProgressPercentage;


        }

        private void esxDetailsBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler;
                if (appNameSelected != AppName.Pushagent)
                {
                    esx_PrimaryServerPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                }
                else
                {
                    esx_PrimaryServerPanelHandler = new ESX_PrimaryServerPanelHandler();
                }
                
                if (esx_PrimaryServerPanelHandler.powerOn == true)
                {
                    esx_PrimaryServerPanelHandler.ReloadAfterPowerOn(this);
                }
                else
                {
                    esx_PrimaryServerPanelHandler.ReloadTreeView(this);
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

        private void preReqsbutton_Click(object sender, EventArgs e)
        {
            try
            {
                progressBar.Visible = true;
                preReqsbutton.Enabled = false;
                reviewDataGridView.Enabled = false;
                planNameTableLayoutPanel.Visible = false;
                protectionTabControl.Visible = true;
                previousButton.Enabled = false;
                protectionTabControl.SelectTab(logsTabPage);
                preReqChecksBackgroundWorker.RunWorkerAsync();              
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

        private void limitResyncCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (limitResyncCheckBox.Checked == false)
                {
                    limitResyncTextBox.Text = "0";
                    limitResyncTextBox.ReadOnly = true;
                }
                else
                {
                    limitResyncTextBox.Text = "3";
                    limitResyncTextBox.ReadOnly = false;
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

        private void targetBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx_SelectSecondaryPanelHandler secondaryPanel = (Esx_SelectSecondaryPanelHandler)panelHandlerListPrepared[indexPrepared];

                if (secondaryPanel.powerOn == true)
                {
                    secondaryPanel.PowerOnVm(this);
                }
                else
                {
                    secondaryPanel.GetEsxInfo(this);
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

        private void targetBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;

        }

        private void targetBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                Esx_SelectSecondaryPanelHandler secondaryPanel = (Esx_SelectSecondaryPanelHandler)panelHandlerListPrepared[indexPrepared];

                if (secondaryPanel.powerOn == true)
                {
                    secondaryPanel.ReloadAfterPowerOn(this);
                    this.Enabled = true;
                    previousButton.Enabled = true;
                    p2v_SelectPrimarySecondaryPanel.Enabled = true;
                    secondaryServerPanelAddEsxButton.Enabled = true;
                    progressBar.Visible = false;
                    selectSecondaryRefreshLinkLabel.Visible = true;
                }
                else
                {
                    secondaryPanel.ReloadTargetTreeViewAfterGetDetails(this);
                }
                //progressBar.Visible = false;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                progressBar.Visible = false;
            }
        }

        private void checkCredentialsBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                PushAgentPanelHandler pushAgentPanelHandler = (PushAgentPanelHandler)panelHandlerListPrepared[indexPrepared];
                pushAgentPanelHandler.BackGroundWorkerForSourceCredentialCheck(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            // ESX_PrimaryServerPanelHandler primaryPanelHandler = (ESX_PrimaryServerPanelHandler)_panelHandlerList[_index];
            //primaryPanelHandler.BackGroundWorkerForSourceCredentialCheck(this);
        }

        private void checkCredentialsBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void checkCredentialsBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                PushAgentPanelHandler pushAgentPanelHandler = (PushAgentPanelHandler)panelHandlerListPrepared[indexPrepared];
                addSourcePanel.Enabled = true;
                this.Enabled = true;
                if (appNameSelected != AppName.Pushagent)
                {
                    nextButton.Enabled = true;
                }
                progressBar.Visible = false;
                pushAgentPanelHandler.PostCredentialsCheck(this);
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

        private void SourceToMasterDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (SourceToMasterDataGridView.RowCount > 0)
                    {
                        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Selected == true)
                        {
                            Esx_SourceToMasterPanelHandler sourceToMaster = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                            if (appNameSelected == AppName.Bmr)
                            {
                                if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Selected == true)
                                {
                                    dataStoreCaptureDisplayNamePrepared = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                    {
                                        listIndexPrepared = e.RowIndex;
                                        sourceToMaster.DataSotreDataGridViewValues(this, e.RowIndex);
                                    }
                                }
                            }
                            if (appNameSelected == AppName.Esx || appNameSelected == AppName.Failback || appNameSelected == AppName.Adddisks)
                            {
                                if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Selected == true)
                                {
                                    dataStoreCaptureDisplayNamePrepared = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                    //if(SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Value != null)
                                    {
                                        sourceToMaster.DataSotreDataGridViewValues(this, e.RowIndex);
                                    }
                                }
                            }
                            if (appNameSelected == AppName.Offlinesync)
                            {
                                if (ResumeForm.selectedActionForPlan == "Esx")
                                {
                                    dataStoreCaptureDisplayNamePrepared = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                }
                                else
                                {
                                    dataStoreCaptureDisplayNamePrepared = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                }
                                if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Value != null)
                                {
                                    for (int i = 0; i < SourceToMasterDataGridView.RowCount; i++)
                                    {
                                        SourceToMasterDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Value = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Value.ToString();
                                    }
                                    sourceToMaster.DataSotreDataGridViewValues(this, e.RowIndex);
                                }
                            }
                        }

                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SourceToMasterTargetDataGridView_CellValueChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void SourceToMasterDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (SourceToMasterDataGridView.RowCount > 0)
            {
                SourceToMasterDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void planNameText_KeyUp(object sender, KeyEventArgs e)
        {

        }

        private void SourceToMasterDataGridView_CellLeave(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (SourceToMasterDataGridView.RowCount > 0)
                {
                    if (e.RowIndex >= 0)
                    {
                        // MessageBox.Show("Reach here");
                        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_IN_DAYS_COLUMN].Selected == true)
                        {
                            Esx_SourceToMasterPanelHandler sourceToMaster = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                            //Debug.WriteLine("Reached to enter the same value for all rows");
                            sourceToMaster.RetentionInDays(this, e.RowIndex);
                        }
                        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.CONSISTENCY_TIME_COLUMN].Selected == true)
                        {
                            Esx_SourceToMasterPanelHandler sourceToMaster = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                            //Debug.WriteLine("Reached to enter the same value for all rows");
                            sourceToMaster.ConsistencyTime(this, e.RowIndex);
                        }
                        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_SIZE_COLUMN].Selected == true)
                        {
                            Esx_SourceToMasterPanelHandler sourceToMaster = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                            //Debug.WriteLine("Reached to enter the same value for all rows");
                            sourceToMaster.RetentionSize(this, e.RowIndex);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException nht = new FormatException("Inner exception", ex);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void SourceToMasterDataGridView_CellValidating(object sender, DataGridViewCellValidatingEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {

                    if (e.ColumnIndex == SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.RETENTION_SIZE_COLUMN].Index) //this is our numeric column
                    {
                        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_SIZE_COLUMN].Value != null)
                        {
                            if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_SIZE_COLUMN].Value.ToString() != "Enter value")
                            {
                                int i;
                                if (!int.TryParse(Convert.ToString(e.FormattedValue), out i))
                                {
                                    e.Cancel = true;
                                    MessageBox.Show("Retention size should be valid number.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                }
                            }
                        }
                    }
                    if (e.ColumnIndex == SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.CONSISTENCY_TIME_COLUMN].Index) //this is our numeric column
                    {
                        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.CONSISTENCY_TIME_COLUMN].Value != null)
                        {
                            if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.CONSISTENCY_TIME_COLUMN].Value.ToString() != "Enter value")
                            {
                                int i;
                                if (!int.TryParse(Convert.ToString(e.FormattedValue), out i))
                                {
                                    e.Cancel = true;
                                    SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.CONSISTENCY_TIME_COLUMN].Value = null;
                                    MessageBox.Show("Consistency time should be valid number.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                }
                            }
                        }
                    }
                    if (e.ColumnIndex == SourceToMasterDataGridView.Columns[Esx_SourceToMasterPanelHandler.RETENTION_IN_DAYS_COLUMN].Index) //this is our numeric column
                    {
                        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_IN_DAYS_COLUMN].Value != null)
                        {
                            if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_IN_DAYS_COLUMN].Value.ToString() != "Enter value")
                            {
                                int i;
                                if (!int.TryParse(Convert.ToString(e.FormattedValue), out i))
                                {
                                    e.Cancel = true;
                                    MessageBox.Show("Retention days should be valid number.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void limitResyncTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            try
            {
                e.Handled = !Char.IsNumber(e.KeyChar) && (e.KeyChar != '\b');
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void SourceToMasterDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SELECT_LUNS_COLUMN].Selected == true)
                    {
                        Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                        esx_SourceToMasterPanelHandler.SelectLunForRDMDisk(this, e.RowIndex);
                    }

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SourceToMasterDataGridView_CellContentClick " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void clearLogsLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            protectionText.Text = null;
        }

        private void SourceToMasterDataGridView_DataError(object sender, DataGridViewDataErrorEventArgs e)
        {
            try
            {
                // MessageBox.Show("Error happened " + e.Context.ToString());
                if (e.Context == DataGridViewDataErrorContexts.Commit)
                {
                    // MessageBox.Show("Commit error");
                }
                if (e.Context == DataGridViewDataErrorContexts.CurrentCellChange)
                {
                    // MessageBox.Show("Cell change");
                }
                if (e.Context == DataGridViewDataErrorContexts.Parsing)
                {
                    //MessageBox.Show("parsing error");
                }
                if (e.Context == DataGridViewDataErrorContexts.LeaveControl)
                {
                    // MessageBox.Show("leave control error");
                }
                if ((e.Exception) is ConstraintException)
                {
                    DataGridView view = (DataGridView)sender;
                    view.Rows[e.RowIndex].ErrorText = "an error";
                    view.Rows[e.RowIndex].Cells[e.ColumnIndex].ErrorText = "an error";
                    e.ThrowException = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SourceToMasterDataGridView_DataError " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void getDetailsButton_Click(object sender, EventArgs e)
        {
            try
            {
                AdditionOfDiskSelectMachinePanelHandler additionOfDiskSelectMachinePanelHandler = (AdditionOfDiskSelectMachinePanelHandler)panelHandlerListPrepared[indexPrepared];
                additionOfDiskSelectMachinePanelHandler.GetDetailsOfProtectedVms(this);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: getDetailsButton_Click " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void additionOfDiskBackGroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                AdditionOfDiskSelectMachinePanelHandler additionOfDiskSelectMachinePanelHandler = (AdditionOfDiskSelectMachinePanelHandler)panelHandlerListPrepared[indexPrepared];
                additionOfDiskSelectMachinePanelHandler.DownLoadEsxMasterXmlFile(this);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: additionOfDiskBackGroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void additionOfDiskBackGroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void additionOfDiskBackGroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                AdditionOfDiskSelectMachinePanelHandler additionOfDiskSelectMachinePanelHandler = (AdditionOfDiskSelectMachinePanelHandler)panelHandlerListPrepared[indexPrepared];
                additionOfDiskSelectMachinePanelHandler.LoadDataGridViewAfterDownLoadingEsx_MasterXml(this);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: additionOfDiskBackGroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void detailsOfAdditionOfDiskDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (detailsOfAdditionOfDiskDataGridView.RowCount > 0)
                    {
                        AdditionOfDiskSelectMachinePanelHandler additionOfDiskSelectMachinePanelHandler = (AdditionOfDiskSelectMachinePanelHandler)panelHandlerListPrepared[indexPrepared];
                        additionOfDiskSelectMachinePanelHandler.SelectedMachineForAdditionOfDisk(this, e.RowIndex);
                    }
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: detailsOfAdditionOfDiskDataGridView_CellContentClick " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void detailsOfAdditionOfDiskDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (detailsOfAdditionOfDiskDataGridView.RowCount > 0)
            {
                detailsOfAdditionOfDiskDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void slideSourceLabel_Click(object sender, EventArgs e)
        {
            /*  if (slideSourceLabel.Text == "<<")
              {
                  slideSourceLabel.Text = ">>";
                  slideOfAddSourcePanel.Location = new Point(495, 0);
                  slideOfAddSourcePanel.Size = new Size(176, 467);
                  slideOfAddSourcePanel.BackColor = Color.LightYellow;
              }
              else if (slideSourceLabel.Text == ">>")
              {
                  slideSourceLabel.Text = "<<";
                  slideOfAddSourcePanel.Location = new Point(652, 0);
                  slideOfAddSourcePanel.Size = new Size(19, 467);
                  slideOfAddSourcePanel.BackColor = Color.White;

              }*/

        }

        private void addSourceSlidePictureBox_Click(object sender, EventArgs e)
        {
            /*  if (_slideOpen == false)
              {
                  _slideOpen                           = true;
                  addSourceSlidePictureBox.Image       = _close;
              
                  addSourceHelpPanel.Location          = new Point(503, 0);
                  addSourceHelpPanel.Size              = new Size(176, 503);
                  addSourceHelpTextBox.BackColor       = Color.FromArgb(244, 244, 244);
                  addSourceHelpPanel.BackColor         = Color.FromArgb(244, 244, 244);
               
              }
              else if (_slideOpen == true)
              {
                  _slideOpen = false;
                  addSourceSlidePictureBox.Image       = _open;
                  addSourceHelpPanel.Location          = new Point(657, 0);
                  addSourceHelpPanel.Size              = new Size(19, 503);
                  addSourceHelpPanel.BackColor         = Color.Transparent;
              
              }
              */
        }



        private void pushAgentPanelBrowserButton_Click_1(object sender, EventArgs e)
        {
            try
            {
                //openFileDialog.ShowDialog();

                //if (openFileDialog.FileName.Length > 0)
                //{
                //    binaryPathTextBox.Text = openFileDialog.FileName;
                //}
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: pushAgentPanelBrowserButton_Click_1 " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

       

        private void SourceToMasterDataGridView_CellEnter(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                string ip;
                if (SourceToMasterDataGridView.RowCount > 0)
                {
                    if (e.RowIndex >= 0)
                    {
                        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_PATH_COLUMN].Selected == true)
                        {
                            SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_PATH_COLUMN].DataGridView.DefaultCellStyle.Font = new Font(SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.RETENTION_PATH_COLUMN].DataGridView.DefaultCellStyle.Font, FontStyle.Regular);
                            SourceToMasterDataGridView.RefreshEdit();
                        }
                        // this is for the selection of the BMR Protection....
                        //if (_appName == AppName.BMR || (_appName == AppName.OFFICELINESYNC && ResumeForm._selectedAction == "P2v"))
                        //{
                        //    if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Selected == true)
                        //    {
                        //        if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Value.ToString() == "Provide OS CD")
                        //        {
                        //            foreach (Host h in _selectedSourceList._hostList)
                        //            {
                        //                //for the bmr protection we are comparing with the ip values....

                        //                ip = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();

                        //                if (h.ip == ip)
                        //                {
                        //                    if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Selected == true)
                        //                    {
                        //                        ip = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].ToString();
                        //                        Esx_SourceToMasterPanelHandler esx_SourceToMasterPanelHandler = (Esx_SourceToMasterPanelHandler)_panelHandlerList[_index];
                        //                        if (esx_SourceToMasterPanelHandler.CopyDriversFromOsCD(ip, this, h.operatingSystem, e.RowIndex) == true)
                        //                        {
                        //                            // SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Value = "Available";
                        //                        }
                        //                    }
                        //                }
                        //            }
                        //        }
                        //    }
                        //}
                        //if (SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Selected == true)
                        {
                            try
                            {

                                Host h = (Host)selectedSourceListByUser._hostList[e.RowIndex];

                                listIndexPrepared = e.RowIndex;

                                if (appNameSelected == AppName.Bmr || (appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                                {
                                    h.ip = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                    dataStoreCaptureDisplayNamePrepared = h.ip;
                                    // detailsOfDataStoreGridViewLabel.Text =  "Select datastore for " + h.ip;
                                    if (h.targetDataStore != null)
                                    {
                                        // detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.ip + " is... ";
                                    }
                                }


                                if (appNameSelected == AppName.Esx)
                                {
                                    h.displayname = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                    dataStoreCaptureDisplayNamePrepared = h.displayname;
                                    // detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.displayname;
                                    if (h.targetDataStore != null)
                                    {
                                        //detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.displayname + " is... ";
                                    }
                                }
                                if (appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "ESX")
                                {
                                    h.displayname = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                    dataStoreCaptureDisplayNamePrepared = h.displayname;
                                    // detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.displayname;
                                    if (h.targetDataStore != null)
                                    {
                                        // detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.displayname + " is... ";
                                    }
                                }
                                if (appNameSelected == AppName.Failback)
                                {
                                    h.displayname = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                    dataStoreCaptureDisplayNamePrepared = h.displayname;
                                    // detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.displayname;
                                    if (h.targetDataStore != null)
                                    {
                                        dataStoreDataGridView.Enabled = false;
                                    }
                                    if (h.targetDataStore != null)
                                    {
                                        // detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.displayname + " is... ";
                                    }
                                }
                                if (appNameSelected == AppName.Offlinesync)
                                {
                                    if (ResumeForm.selectedActionForPlan == "Esx")
                                    {
                                        h.displayname = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                        dataStoreCaptureDisplayNamePrepared = h.displayname;
                                        //  detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.displayname;
                                        if (h.targetDataStore != null)
                                        {
                                            //detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.displayname + " is... ";
                                        }
                                    }
                                    else
                                    {
                                        h.ip = SourceToMasterDataGridView.Rows[e.RowIndex].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                                        dataStoreCaptureDisplayNamePrepared = h.ip;
                                        //detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + h.ip;
                                        if (h.targetDataStore != null)
                                        {
                                            // detailsOfDataStoreGridViewLabel.Text = "Selected datastore for " + h.ip + " is... ";
                                        }
                                    }
                                }
                                // if datastore value is not null we are displaying the selected datastore to the user....
                                if (h.targetDataStore != null)
                                {
                                    for (int i = 0; i < dataStoreDataGridView.RowCount; i++)
                                    {
                                        if (h.targetDataStore != dataStoreDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.DATASTORE_NAME_COLUMN].Value.ToString())
                                        {
                                            Debug.WriteLine(DateTime.Now + " \t Data store matchs in the grid");
                                            //dataStoreDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.DATASOURCE_SELECTED_COLUMN].Value = false;
                                        }
                                        else
                                        {
                                            //if datastore contains in the host is equal to the datagridview we are making the checkbox as true....
                                            SourceToMasterDataGridView.RefreshEdit();
                                            //dataStoreDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.DATASOURCE_SELECTED_COLUMN].Value = true;
                                            dataStoreDataGridView.RefreshEdit();
                                        }
                                    }
                                }

                                if (h.targetDataStore == null)
                                {
                                    for (int i = 0; i < dataStoreDataGridView.RowCount; i++)
                                    {
                                        //if datastroe is null we are making entire datagridview checkboxes as false....

                                        //  dataStoreDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.DATASOURCE_SELECTED_COLUMN].Value = false;
                                    }
                                }
                            }
                            catch (Exception ex)
                            {
                                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                                Trace.WriteLine("ERROR*******************************************");
                                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SourceToMasterDataGridView_CellEnter We can ignore this case " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                                Trace.WriteLine("Exception  =  " + ex.Message);
                                Trace.WriteLine("ERROR___________________________________________");
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:We can ignore this case SourceToMasterDataGridView_CellEnter  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void helpPictureBox_Click(object sender, EventArgs e)
        {
            try
            {
                if (slideOpenHelp == false)
                {
                    helpPanel.BringToFront();
                    helpPanel.Visible = true;
                    slideOpenHelp = true;
                }
                else
                {
                    slideOpenHelp = false;
                    helpPanel.Visible = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: helpPictureBox_Click " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void closePictureBox_Click(object sender, EventArgs e)
        {
            try
            {
                helpPanel.Visible = false;
                slideOpenHelp = false;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: closePictureBox_Click " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void esxProtectionLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                if (File.Exists(installationPath + "\\Manual.chm"))
                {
                    if (appNameSelected == AppName.Esx)
                    {
                        // Process.Start("http://" + WinTools.GetcxIPWithPortNumber() + "/help/Content/ESX Solution/Protect ESX.htm");
                        Help.ShowHelp(null, installationPath + "\\Manual.chm", HelpNavigator.TopicId, ProtectionEsx);
                    }
                    else if (appNameSelected == AppName.Failback)
                    {
                        //Process.Start("http://" + WinTools.GetcxIPWithPortNumber() + "/help/Content/ESX Solution/Failback Protection.htm");
                        Help.ShowHelp(null, installationPath + "\\Manual.chm", HelpNavigator.TopicId, FailbackHelp);
                    }
                    else if (appNameSelected == AppName.Offlinesync)
                    {
                        // Process.Start("http://" + WinTools.GetcxIPWithPortNumber() + "/help/Content/ESX Solution/Offline Sync.htm");
                        Help.ShowHelp(null, installationPath + "\\Manual.chm", HelpNavigator.TopicId, OfflinesyncHelp);
                    }
                    else if (appNameSelected == AppName.Adddisks)
                    {
                        // Process.Start("http://" + WinTools.GetcxIPWithPortNumber() + "/help/Content/ESX Solution/Protect Incremental Disk.htm");
                        Help.ShowHelp(null, installationPath + "\\Manual.chm", HelpNavigator.TopicId, AddDiskHelp);
                    }
                    else if (appNameSelected == AppName.Recover)
                    {
                        Help.ShowHelp(null, installationPath + "\\Manual.chm", HelpNavigator.TopicId, "2");
                    }
                    else if (appNameSelected == AppName.Pushagent)
                    {
                        Help.ShowHelp(null, installationPath + "\\Manual.chm", HelpNavigator.TopicId, PushagentHelp);
                    }
                    else if (appNameSelected == AppName.Bmr)
                    {
                        Help.ShowHelp(null, installationPath + "\\Manual.chm", HelpNavigator.TopicId, ProtectionP2vHelp);
                    }
                    else if (appNameSelected == AppName.Drdrill)
                    {
                        Help.ShowHelp(null, installationPath + "\\Manual.chm", HelpNavigator.TopicId, DrdrillHelp);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: esxProtectionLinkLabel_LinkClicked " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void helpPanel_MouseLeave(object sender, EventArgs e)
        {
            //helpPanel.Visible = false;
        }

        private void addSourceRefreshLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                ESX_PrimaryServerPanelHandler esx_PrimaryServerPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_PrimaryServerPanelHandler.RefreshLinkLabelClickEvent(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: addSourceRefreshLinkLabel_LinkClicked " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void selectSecondaryRefreshLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                Esx_SelectSecondaryPanelHandler esx_SelectSecondaryPanelHandler = (Esx_SelectSecondaryPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_SelectSecondaryPanelHandler.SelectSecondaryRefreshLinkLabelClickEvent(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: selectSecondaryRefreshLinkLabel_LinkClicked  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void closePictureBox_Click_1(object sender, EventArgs e)
        {
            try
            {
                helpPanel.Visible = false;
                slideOpenHelp = false;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: closePictureBox_Click_1 " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void monitiorLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            //MonitorScreenForm monitorScreenForm = new MonitorScreenForm();
            // monitorScreenForm.ShowDialog();
            try
            {
                Process.Start("http://" + WinTools.GetcxIPWithPortNumber() + "/ui");
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: monitiorLinkLabel_LinkClicked " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void postJobAutomationBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx_ProtectionPanelHandler esx_ProtectionPanelHandler = (Esx_ProtectionPanelHandler)panelHandlerListPrepared[indexPrepared];
                if (esx_ProtectionPanelHandler.PostJobAutomation(this) == false)
                {
                    Esx_ProtectionPanelHandler.POST_JOBAUTOMATION = false;
                }
                else
                {
                    Esx_ProtectionPanelHandler.POST_JOBAUTOMATION = true;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: monitiorLinkLabel_LinkClicked " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void postJobAutomationBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void postJobAutomationBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                Esx_ProtectionPanelHandler esx_ProtectionPanelHandler = (Esx_ProtectionPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_ProtectionPanelHandler.PostJobAutomationResultAction(this);
                monitiorLinkLabel.Visible = true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: postJobAutomationBackgroundWorker_RunWorkerCompleted  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void scriptBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx_ProtectionPanelHandler esx_ProtectionPanelHandler = (Esx_ProtectionPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_ProtectionPanelHandler.PreScriptRun(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: scriptBackgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void scriptBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void scriptBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                Esx_ProtectionPanelHandler esx_ProtectionPanelHandler = (Esx_ProtectionPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_ProtectionPanelHandler.PostScriptRunAfetrJobAutomation(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: scriptBackgroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void pushAgentDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (pushAgentDataGridView.Rows[e.RowIndex].Cells[PushAgentPanelHandler.PUSHAGENT_COLUMN].Selected == true)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Selected pushagent column ");
                        PushAgentPanelHandler panerlHandler = (PushAgentPanelHandler)panelHandlerListPrepared[indexPrepared];
                        panerlHandler.PushOptionsChanged(this, e.RowIndex);
                    }
                    else if (pushAgentDataGridView.Rows[e.RowIndex].Cells[PushAgentPanelHandler.ADVANCED_COLUMN].Selected == true)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Selected Advanced option for push "); 
                        PushAgentPanelHandler panerlHandler = (PushAgentPanelHandler)panelHandlerListPrepared[indexPrepared];
                        panerlHandler.SelectedHostForAdvancedOptions(this, e.RowIndex);
                    }

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: pushAgentDataGridView_CellContentClick " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }


        /*
         * get parent process for a given PID
         */
        private int GetParentProcess(int Id)
        {
            int parentPid = 0;
            using (ManagementObject mo = new ManagementObject("win32_process.handle='" + Id.ToString() + "'"))
            {
                mo.Get();
                parentPid = Convert.ToInt32(mo["ParentProcessId"]);
            }
            return parentPid;
        }
        internal bool FindAndKillProcess()
        {
            bool didIkillAnybody = false;
            //finding current process id...
            System.Diagnostics.Process CurrentPro = new System.Diagnostics.Process();

            CurrentPro = System.Diagnostics.Process.GetCurrentProcess();
            Process[] procs = Process.GetProcesses();

            foreach (Process p in procs)
            {
                if (GetParentProcess(p.Id) == CurrentPro.Id)
                {
                    SuppressMsgBoxes = true;
                    p.Kill();
                    didIkillAnybody = true;

                }
            }



            if (didIkillAnybody)
            {
                Trace.WriteLine(DateTime.Now + "\t\t Killed all running child processes...");
                //CurrentPro.Kill();

            }
            else
            {

                Trace.WriteLine(DateTime.Now + "\t\t No child processes to kill..");
            }
            return true;


        }

        private void copyDriversBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx_SourceToMasterPanelHandler sourceToMasterTargetPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];

                sourceToMasterTargetPanelHandler.CopyDriversUsingBackGroundWorker(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: copyDriversBackgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void copyDriversBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void copyDriversBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                Esx_SourceToMasterPanelHandler sourceToMasterTargetPanelHandler = (Esx_SourceToMasterPanelHandler)panelHandlerListPrepared[indexPrepared];
                progressBar.Visible = false;
                nextButton.Enabled = true;
                sourceToMasterTargetPanelHandler.FindDriverFileReturnCode(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: copyDriversBackgroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

      
        private void selectAllForPushCheckBox_CheckedChanged(object sender, EventArgs e)
        {

        }

     

        private void pushAgentCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            PushAgentPanelHandler push = (PushAgentPanelHandler)panelHandlerListPrepared[indexPrepared];

            push.SelectAllVmsForPush(this);
        }

        private void natConfigurationCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            ESX_PrimaryServerPanelHandler primaryPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
            //primaryPanelHandler.NATChanges(this);
        }

        

        private void reviewDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {

        }

        private void reviewDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (reviewDataGridView.RowCount > 0)
            {
                reviewDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void pushGetDeatsilsButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (selectionByUser == Selection.Esxpush)
                {
                    generalLogBoxClearLinkLabel.Visible = false;
                    pushAgentCheckBox.Checked = false;
                    pushUninstallButton.Visible = false;
                    generalLogTextBox.Visible = false;
                    pushAgentDataGridView.Rows.Clear();
                    pushAgentDataGridView.Visible = false;
                    pushAgentCheckBox.Visible = false;
                    pushAgentsButton.Visible = false;
                    ESX_PrimaryServerPanelHandler esx = new ESX_PrimaryServerPanelHandler();
                    pushGetDeatsilsButton.Enabled = false;
                    esx.GetEsxDetails(this);
                }
                else
                {
                    P2V_PrimaryServerPanelHandler p2vPush = new P2V_PrimaryServerPanelHandler();
                    p2vPush.AddIp(this);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: pushGetDeatsilsButton_Click " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        

        private void pushAgentBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                PushAgentPanelHandler p2v_PushAgentPanelHandler = (PushAgentPanelHandler)panelHandlerListPrepared[indexPrepared];
                p2v_PushAgentPanelHandler.PushAgentsHandler(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: pushAgentBackgroundWorker_DoWork" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void pushAgentBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void pushAgentBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                pushAgentDataGridView.Enabled = true;
                progressBar.Visible = false;
                pushAgentsButton.Enabled = true;
                pushUninstallButton.Enabled = true;
                if (appNameSelected != AppName.Pushagent)
                {
                    previousButton.Enabled = true;
                    nextButton.Enabled = true;
                    pushAgentsButton.Enabled = true;
                    pushUninstallButton.Enabled = true;

                }
                else
                {
                    pushGetDeatsilsButton.Enabled = true;
                    //doneButton.Visible = true;
                    previousButton.Visible = false;
                    nextButton.Visible = false;                    
                    cancelButton.Visible = false;
                    doneButton.Visible = true;
                    doneButton.Enabled = true;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: pushAgentBackgroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void unInstallBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                PushAgentPanelHandler pushUninstall = (PushAgentPanelHandler)panelHandlerListPrepared[indexPrepared];
                pushUninstall.UnInstallAgents(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: unInstallBackgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void unInstallBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void unInstallBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                pushAgentDataGridView.Enabled = true;
                progressBar.Visible = false;
                pushAgentsButton.Enabled = true;
                pushUninstallButton.Enabled = true;
                if (appNameSelected != AppName.Pushagent)
                {
                    previousButton.Enabled = true;
                    nextButton.Enabled = true;
                    pushAgentsButton.Enabled = true;
                    pushUninstallButton.Enabled = true;
                }
                else
                {
                    pushGetDeatsilsButton.Enabled = true;
                    //doneButton.Visible = true;
                    previousButton.Visible = false;
                    nextButton.Visible = false;
                    cancelButton.Visible = true;

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: unInstallBackgroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void pushUninstallButton_Click(object sender, EventArgs e)
        {
            try
            {
                nextButton.Enabled = false;
                pushGetDeatsilsButton.Enabled = false;
                previousButton.Enabled = false;
                pushAgentDataGridView.Enabled = false;
                pushUninstallButton.Enabled = false;
                pushAgentsButton.Enabled = false;
                cancelButton.Visible = true;
                cancelButton.Enabled = true;
                progressBar.Visible = true;
                unInstallBackgroundWorker.RunWorkerAsync();
                
                doneButton.Visible = false;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: pushUninstallButton_Click" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void generalLogBoxClearLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            generalLogTextBox.Text = null;
        }


        private void networkDataGridView_CellClick(object sender, DataGridViewCellEventArgs e)
        {

        }
        private void memoryValuesComboBox_SelectionChangeCommitted(object sender, EventArgs e)
        {
            try
            {
                if (appNameSelected == AppName.Recover|| appNameSelected == AppName.Drdrill)
                {
                    RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                    config.MemorySelectedinGBOrMB(this);
                }
                else
                {
                    ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                    config.MemorySelectedinGBOrMB(this);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: memoryValuesComboBox_SelectionChangeCommitted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }


        private void createNewPlanCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            if (createNewPlanCheckBox.Checked == true)
            {
                Esx_ProtectionPanelHandler._usingExistingPlanName = false;
                existingPlanNameComboBox.Enabled = false;
                planNameText.Enabled = true;
            }
            else
            {
                Esx_ProtectionPanelHandler._usingExistingPlanName = true;
                planNameText.Enabled = false;
                existingPlanNameComboBox.Enabled = true;
            }
        }

        private void existingPlanNameComboBox_SelectionChangeCommitted(object sender, EventArgs e)
        {
            if (existingPlanNameComboBox.Text != null)
            {
                nextButton.Enabled = true;
            }
        }

        private void planNameText_KeyUp_1(object sender, KeyEventArgs e)
        {
            nextButton.Enabled = true;
        }

        private void recoveryGetDetailsButton_Click(object sender, EventArgs e)
        {
            try
            {
                Esx_RecoverPanelHandler recoveryPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoveryPanelHandler.GetDetailsButtonClickEventForRecovery(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryGetDetailsButton_Click " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void credentialCheckBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx_RecoverPanelHandler recoveryPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoveryPanelHandler.RecoveryReadinessChecks(this);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: credentialCheckBackgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
        }

        private void credentialCheckBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void credentialCheckBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                Esx_RecoverPanelHandler recoveryPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoveryPanelHandler.PostReadinessCheck(this);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: credentialCheckBackgroundWorker_RunWorkerCompleted" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
        }

        private void recoveryPreReqsButton_Click(object sender, EventArgs e)
        {
            try
            {
                Esx_RecoverPanelHandler recoverPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoveryText.Visible = false;
                clearLogLinkLabel.Visible = false;
                nextButton.Enabled = false;
                recoverPanelHandler.PrePareHostListForRecovery(this);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryPreReqsButton_Click " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }

        }

        private void recoveryDetailsBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx_RecoverPanelHandler recoverPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoverPanelHandler.DoWorkOfRecoveryDetailsBackGroundWorker(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryDetailsBackgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void recoveryDetailsBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void recoveryDetailsBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                Esx_RecoverPanelHandler recoverPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoverPanelHandler.RunWorkerCompletedOfRecoveryDetailsBackGroundWorker(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryDetailsBackgroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void recoverDataGridView_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                Esx_RecoverPanelHandler recoverPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoverPanelHandler.RevoverDataCellClickEvent(this, e.RowIndex);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoverDataGridView_CellClick " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void recoverDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex >= 0)
            {
                Esx_RecoverPanelHandler recoverPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoverPanelHandler.RecoverDataGridViewCellValueChangesEvent(this, e.RowIndex);
            }
        }

        private void recoverDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (recoverDataGridView.RowCount > 0)
            {
                recoverDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void specificTimeDateTimePicker_Leave(object sender, EventArgs e)
        {
            specificTimeDateTimePicker.Format = DateTimePickerFormat.Custom;
            DateTime dt = DateTime.Now;


            System.Globalization.CultureInfo culture = System.Globalization.CultureInfo.CurrentCulture;
            System.Globalization.DateTimeFormatInfo info = culture.DateTimeFormat;

            string datePattern = info.ShortDatePattern;

            string timePattern = info.ShortTimePattern;

            specificTimeDateTimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
            specificTimeDateTimePicker.CustomFormat = datePattern + " " + timePattern;

            //  dateText.Text = dateTimePicker.Text;

            try
            {
                dt = specificTimeDateTimePicker.Value;



                // dt = DateTime.Parse(dateTimePicker.Text);
                DateTime gmt = DateTime.Parse(dt.ToString());
                //DateTime oneSecondExtraTime;


               // oneSecondExtraTime=gmt.AddSeconds(1);
                //dateTimePicker.Value = DateTime.Parse(gmt.ToString("yyyy/mm/dd HH:mm:ss"));
                //recoveryTagText.Text = dt.ToUniversalTime().ToString();
                universalTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                //  MessageBox.Show(gmt.ToString("yyyy/MM/dd HH:mm:ss"));

                Esx_RecoverPanelHandler recoverPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoverPanelHandler.RecoveryChanges(this);
            }
            catch (Exception t)
            {
                Trace.WriteLine(DateTime.Now + "\t RecoveryForm.cs: dataTimePicker_Closeup Select a valid time");
                MessageBox.Show("Select a valid time ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                Trace.WriteLine(DateTime.Now + "\t " + t.Message);

            }
        }

        private void specificTimeDateTimePicker_CloseUp(object sender, EventArgs e)
        {
            specificTimeDateTimePicker.Format = DateTimePickerFormat.Custom;
            DateTime dt = DateTime.Now;



            System.Globalization.CultureInfo culture = System.Globalization.CultureInfo.CurrentCulture;
            System.Globalization.DateTimeFormatInfo info = culture.DateTimeFormat;

            string datePattern = info.ShortDatePattern;

            string timePattern = info.ShortTimePattern;

            specificTimeDateTimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
            specificTimeDateTimePicker.CustomFormat = datePattern + " " + timePattern;

            //  dateText.Text = dateTimePicker.Text;

            try
            {
                dt = specificTimeDateTimePicker.Value;



                // dt = DateTime.Parse(dateTimePicker.Text);
                DateTime gmt = DateTime.Parse(dt.ToString());
                //DateTime oneSecondExtraTime;


                // oneSecondExtraTime=gmt.AddSeconds(1);
                //dateTimePicker.Value = DateTime.Parse(gmt.ToString("yyyy/mm/dd HH:mm:ss"));
                //recoveryTagText.Text = dt.ToUniversalTime().ToString();
                universalTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                //  MessageBox.Show(gmt.ToString("yyyy/MM/dd HH:mm:ss"));


                Esx_RecoverPanelHandler recoverPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoverPanelHandler.RecoveryChanges(this);

            }
            catch (Exception t)
            {
                Trace.WriteLine(DateTime.Now + "\t RecoveryForm.cs: dataTimePicker_Closeup Select a valid time");
                MessageBox.Show("Select a valid time ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                Trace.WriteLine(DateTime.Now + "\t " + t.Message);
            }
        }

        private void dateTimePicker_CloseUp(object sender, EventArgs e)
        {
            dateTimePicker.Format = DateTimePickerFormat.Custom;
            DateTime dt = DateTime.Now;


           // dateTimePicker.CustomFormat = "yyyy/MM/dd HH:mm:ss";
            System.Globalization.CultureInfo culture = System.Globalization.CultureInfo.CurrentCulture;
            System.Globalization.DateTimeFormatInfo info = culture.DateTimeFormat;

            string datePattern = info.ShortDatePattern;

            string timePattern = info.ShortTimePattern;

            dateTimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
            dateTimePicker.CustomFormat = datePattern + " " + timePattern;


            //  dateText.Text = dateTimePicker.Text;

            try
            {
                dt = dateTimePicker.Value;



                // dt = DateTime.Parse(dateTimePicker.Text);
                DateTime gmt = DateTime.Parse(dt.ToString());
                //DateTime oneSecondExtraTime;


                // oneSecondExtraTime=gmt.AddSeconds(1);
                //dateTimePicker.Value = DateTime.Parse(gmt.ToString("yyyy/mm/dd HH:mm:ss"));
                //recoveryTagText.Text = dt.ToUniversalTime().ToString();
                universalTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                //  MessageBox.Show(gmt.ToString("yyyy/MM/dd HH:mm:ss"));

                Esx_RecoverPanelHandler recoverPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoverPanelHandler.RecoveryChanges(this);

            }
            catch (Exception t)
            {
                Trace.WriteLine(DateTime.Now + "\t RecoveryForm.cs: dataTimePicker_Closeup Select a valid time");
                MessageBox.Show("Select a valid time ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                Trace.WriteLine(DateTime.Now + "\t " + t.Message);
            }

        }

        private void dateTimePicker_Leave(object sender, EventArgs e)
        {
            dateTimePicker.Format = DateTimePickerFormat.Custom;
            DateTime datetime = DateTime.Now;


            System.Globalization.CultureInfo culture = System.Globalization.CultureInfo.CurrentCulture;
            System.Globalization.DateTimeFormatInfo info = culture.DateTimeFormat;

            string datePattern = info.ShortDatePattern;

            string timePattern = info.ShortTimePattern;

            dateTimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
            dateTimePicker.CustomFormat = datePattern + " " + timePattern;

            //  dateText.Text = dateTimePicker.Text;

            try
            {
                datetime = dateTimePicker.Value;



                // dt = DateTime.Parse(dateTimePicker.Text);
                DateTime gmt = DateTime.Parse(datetime.ToString());
                //DateTime oneSecondExtraTime;


                // oneSecondExtraTime=gmt.AddSeconds(1);
                //dateTimePicker.Value = DateTime.Parse(gmt.ToString("yyyy/mm/dd HH:mm:ss"));
                //recoveryTagText.Text = dt.ToUniversalTime().ToString();
                universalTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                //  MessageBox.Show(gmt.ToString("yyyy/MM/dd HH:mm:ss"));
                Esx_RecoverPanelHandler recoverPanelHandler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoverPanelHandler.RecoveryChanges(this);
            }
            catch (Exception t)
            {
                Trace.WriteLine(DateTime.Now + "\t RecoveryForm.cs: dataTimePicker_Closeup Select a valid time");
                MessageBox.Show("Select a valid time ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                Trace.WriteLine(DateTime.Now + "\t " + t.Message);
            }
        }

        private void selectAllCheckBoxForRecovery_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (selectAllCheckBoxForRecovery.Checked == true)
                {
                    string mtName = null;
                    for (int i = 0; i < recoverDataGridView.RowCount; i++)
                    {
                        if ((bool)recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].FormattedValue)
                        {
                            mtName = recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.MASTERTARGET_DISPLAYNAME].Value.ToString();
                            break;
                        }
                    }
                    for (int i = 0; i < recoverDataGridView.RowCount; i++)
                    {
                        if (mtName == recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.MASTERTARGET_DISPLAYNAME].Value.ToString())
                        {
							// This will occer when user selected select all check box on datagridview table
							// Earlier we use to select all the machines from the datagridview table
							//When user selects one option and readinesschecks fails for that machine 
							//Again user used select all option to select machien we will falce one isssue 1819562
							// To avoid that issue we need to select machine which are not selected only.
							// So added this check
                            if (!(bool)recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].FormattedValue)
                            {
                                recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].Value = true;
                                recoverDataGridView.RefreshEdit();
                                Host h = new Host();
                                h.hostname = recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value.ToString();
                                int index = 0;
                                if (sourceListByUser.DoesHostExist(h, ref index) == true)
                                {
                                    h = (Host)sourceListByUser._hostList[index];
                                    if (tagBasedRadioButton.Checked == true)
                                    {
                                        h.tag = "LATEST";
                                        h.recoveryOperation = 1;
                                        h.tagType = "FS";
                                    }
                                    else if (timeBasedRadioButton.Checked == true)
                                    {
                                        h.tag = "LATESTTIME";
                                        h.recoveryOperation = 2;
                                        h.tagType = "FS";
                                    }
                                    else if (userDefinedTimeRadioButton.Checked == true)
                                    {
                                        DateTime dt = DateTime.Now;
                                        DateTime gmt = DateTime.Parse(dt.ToString());
                                        h.recoveryOperation = 3;
                                        h.tagType = "FS";
                                        universalTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                                        h.userDefinedTime = universalTime;
                                        Host h1 = new Host();
                                        h1 = h;
                                        WinTools.SetHostTimeinGMT(h1);
                                        h1 = WinTools.GetHost;
                                        h.userDefinedTime = h1.userDefinedTime;
                                    }
                                    else if (specificTimeRadioButton.Checked == true)
                                    {
                                        DateTime dt = DateTime.Now;
                                        DateTime gmt = DateTime.Parse(dt.ToString());
                                        h.recoveryOperation = 4;
                                        h.tagType = "TIME";
                                        universalTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                                        h.userDefinedTime = universalTime;
                                        Host h1 = new Host();
                                        h1 = h;
                                        WinTools.SetHostTimeinGMT(h1);
                                        h1 = WinTools.GetHost;
                                        h.userDefinedTime = h1.userDefinedTime;
                                    }
                                }

                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + " Host does not found in the list");
                            }

                        }
                    }
                }
                else
                {
                    if (recoverDataGridView.RowCount > 0)
                    {
                        for (int i = 0; i < recoverDataGridView.RowCount; i++)
                        {
                            recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].Value = false;
                            recoverDataGridView.RefreshEdit();
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: selectAllCheckBoxForRecovery_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void recoveryBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                ReviewPanelHandler reviewPanelHander = (ReviewPanelHandler)panelHandlerListPrepared[indexPrepared];
                reviewPanelHander.RecoverMachines(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryBackgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void recoveryBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;

        }

        private void recoveryBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                ReviewPanelHandler reviewPanelHander = (ReviewPanelHandler)panelHandlerListPrepared[indexPrepared];
                reviewPanelHander.PostActionAfterRecoveryCompleted(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryBackgroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void userDefinedTimeRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (userDefinedTimeRadioButton.Checked == true)
                {
                    specificTimeRadioButton.Location = new Point(10, 116);
                    specificTagRadioButton.Location = new Point(10, 135);
                    specificTimeDateTimePicker.Visible = false;
                    DateTime dt = DateTime.Now;
                    DateTime gmt = DateTime.Parse(dt.ToString());
                    Host h = new Host();
                    h.hostname = recoverDataGridView.Rows[listIndexPrepared].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value.ToString();
                    int index = 0;
                    if (sourceListByUser.DoesHostExist(h, ref index) == true)
                    {
                        h = (Host)sourceListByUser._hostList[index];
                        if (h.userDefinedTime != null)
                        {
                            h.recoveryOperation = 3;
                            DateTime convertedDate = DateTime.Parse(h.userDefinedTime);
                            dateTimePicker.Text = h.selectedTimeByUser;
                          
                            h.tagType = "FS";
                        }
                        else
                        {
                            h.recoveryOperation = 3;
                            h.tagType = "FS";
                            //dateTimePicker.Value = DateTime.Parse(gmt.ToString("yyyy/mm/dd HH:mm:ss"));
                            //recoveryTagText.Text = dt.ToUniversalTime().ToString();
                            universalTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                          
                            //recoveryTagText.ReadOnly = false;
                            h.selectedTimeByUser = universalTime;
                            h.userDefinedTime = universalTime;
                            Host h1 = new Host();
                            h1 = h;
                            try
                            {
                                WinTools.SetHostTimeinGMT( h1);
                                h1 = WinTools.GetHost;
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to set the time in gmt " + ex.Message);
                            }
                            h.userDefinedTime = h1.userDefinedTime;

                            //RecoveryChanges();

                        }
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " Host does not found in the list");
                    }

                    dateTimePicker.Visible = true;
                }
                else
                {
                    specificTimeRadioButton.Location = new Point(10, 89);
                    if (specificTagRadioButton.Checked == false)
                    {
                        specificTagRadioButton.Location = new Point(10, 112);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: UserDefined time " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void specificTimeRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (specificTimeRadioButton.Checked == true)
                {
                    specificTimeDateTimePicker.Location = new Point(60, 116);
                    specificTagRadioButton.Location = new Point(10, 139);
                    dateTimePicker.Visible = false;
                    DateTime dt = DateTime.Now;
                    DateTime gmt = DateTime.Parse(dt.ToString());
                    Host h = new Host();
                    h.hostname = recoverDataGridView.Rows[listIndexPrepared].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value.ToString();
                    int index = 0;
                    if (sourceListByUser.DoesHostExist(h, ref index) == true)
                    {
                        h = (Host)sourceListByUser._hostList[index];
                        if (h.userDefinedTime != null)
                        {
                            //DateTime convertedDate = DateTime.Parse(h.userDefinedTime);
                            h.recoveryOperation = 4;
                            // DateTime dt1 = TimeZone.CurrentTimeZone.ToLocalTime(convertedDate);
                            DateTime convertedDate = DateTime.Parse(h.userDefinedTime);

                            // dt = TimeZone.CurrentTimeZone.ToLocalTime(convertedDate);
                            // h.userDefinedTime = dt.ToString();
                            //dateTimePicker.Text = h.selectedTimeByUser;
                            specificTimeDateTimePicker.Text = h.selectedTimeByUser;
                            h.tagType = "TIME";
                            h.tag = h.userDefinedTime;
                        }
                        else
                        {

                            
                            h.recoveryOperation = 4;
                            h.tagType = "TIME";
                            //dateTimePicker.Value = DateTime.Parse(gmt.ToString("yyyy/mm/dd HH:mm:ss"));
                            //recoveryTagText.Text = dt.ToUniversalTime().ToString();
                            universalTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                            //recoveryTagText.ReadOnly = false;
                            h.userDefinedTime = universalTime;
                            h.selectedTimeByUser = universalTime;
                            Host h1 = new Host();
                            h1 = h;
                            h.userDefinedTime = universalTime;
                            try
                            {
                                WinTools.SetHostTimeinGMT( h1);
                                h1 = WinTools.GetHost;
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to set the time in gmt " + ex.Message );
                            }
                            h.userDefinedTime = h1.userDefinedTime;
                            h.tag = h1.userDefinedTime;
                            //RecoveryChanges();
                        }
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " Host does not found in the list");
                    }
                    specificTimeDateTimePicker.Visible = true;
                }
                else
                {
                    specificTimeDateTimePicker.Visible = false;
                    specificTagRadioButton.Location = new Point(10, 112);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: specificTimeRadioButton_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void tagBasedRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                //tagComboBox.Enabled = true;
                //recoveryTagText.Text = "LATEST";
                //recoveryTagText.ReadOnly = false;
                if (tagBasedRadioButton.Checked == true)
                {
                    dateTimePicker.Visible = false;
                    specificTimeDateTimePicker.Visible = false;
                    specificTimeRadioButton.Location = new Point(10, 89);
                    specificTagRadioButton.Location = new Point(10, 112);
                    // RecoveryChanges();
                    Host h = new Host();
                    h.hostname = recoverDataGridView.Rows[listIndexPrepared].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value.ToString();
                    int index = 0;
                    if (sourceListByUser.DoesHostExist(h, ref index) == true)
                    {
                        h = (Host)sourceListByUser._hostList[index];
                        h.recoveryOperation = 1;
                        h.tag = "LATEST";
                        h.tagType = "FS";
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " Host does not found in the list");
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: TagBasedRadioButton " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void timeBasedRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                specificTimeDateTimePicker.Visible = false;
                dateTimePicker.Visible = false;
                Host h = new Host();
                h.hostname = recoverDataGridView.Rows[listIndexPrepared].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value.ToString();
                int index = 0;
                specificTimeRadioButton.Location = new Point(10, 89);
                specificTagRadioButton.Location = new Point(10, 112);
                if (sourceListByUser.DoesHostExist(h, ref index) == true)
                {
                    h = (Host)sourceListByUser._hostList[index];
                    h.recoveryOperation = 2;
                    h.tag = "LATESTTIME";
                    h.tagType = "FS";
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + " Host does not found in the list " + h.hostname);
                }
                // tagComboBox.Enabled = false;
                // recoveryTagText.Text = "LATESTTIME";
                //recoveryTagText.ReadOnly = true;
                //RecoveryChanges();
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: TimeBasedRadioButtion " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void recoveryForMasterXmlBackGroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx_RecoverPanelHandler recoveryPanelHanldler = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recoveryPanelHanldler.RunMasterScriptToGetMaSterXMl(this);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in recoveryForMasterXmlBackGroundWorker_DoWork " + ex.Message);
            }

        }

        private void recoveryForMasterXmlBackGroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;

        }

        private void recoveryForMasterXmlBackGroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {                 
            

            try
            {
                progressBar.Visible = false;
                recoverPanel.Enabled = true;
                progressLabel.Visible = false;
                Esx_RecoverPanelHandler.SKIP_DATA = null;
                recoveryWarningLinkLabel.Visible = false;
                if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log"))
                {
                    StreamReader sr = new StreamReader(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log");
                    string skipData = sr.ReadToEnd();
                    if (skipData.Length != 0)
                    {
                        Esx_RecoverPanelHandler.SKIP_DATA = skipData;
                        recoveryWarningLinkLabel.Visible = true;
                        errorProvider.SetError(recoveryWarningLinkLabel, "Skipped data");
                        //MessageBox.Show("Some of the data has been skipped form the vCenter/vSphere server." + Environment.NewLine +skipData, "Skipped Data for Server", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    }
                    sr.Close();

                }
            }
           
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryForMasterXmlBackGroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }

        }

        private void recoverylinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                Process.Start("http://" + WinTools.GetcxIPWithPortNumber() + "/ui");
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoverylinkLabel_LinkClicked " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void reviewDataGridView_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (reviewDataGridView.Rows[e.RowIndex].Cells[Esx_ProtectionPanelHandler.SOURCE_SELECT_UNSELECT_COLUMN].Selected == true)
                    {
                        Esx_ProtectionPanelHandler esx_ProtectionPanelHandler = (Esx_ProtectionPanelHandler)panelHandlerListPrepared[indexPrepared];
                        esx_ProtectionPanelHandler.UnselectMachine(this, e.RowIndex);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: reviewDataGridView_CellClick " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void clearLogLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                recoveryText.Clear();
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in clearLogLinkLabel_LinkClicked " + ex.Message);
            }
        }

        private void versionLabel_MouseEnter(object sender, EventArgs e)
        {
            try
            {
                System.Windows.Forms.ToolTip toolTip = new System.Windows.Forms.ToolTip();
                toolTip.AutoPopDelay = 5000;
                toolTip.InitialDelay = 1000;
                toolTip.ReshowDelay = 500;
                toolTip.ShowAlways = true;
                toolTip.GetLifetimeService();
                toolTip.IsBalloon = false;
                toolTip.UseAnimation = true;
                toolTip.SetToolTip(versionLabel, HelpForcx.BuildDate);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: versionLabel_MouseEnter " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }

        private void cpuComboBox_SelectionChangeCommitted(object sender, EventArgs e)
        {
            try
            {
                if (appNameSelected == AppName.Recover || appNameSelected == AppName.Drdrill)
                {
                    RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                    config.SelectCpuCount(this);
                }
                else
                {
                    ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                    config.SelectCpuCount(this);
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in cpuComboBox_SelectionChangeCommitted " + ex.Message);
            }
        }

        private void sourceConfigurationTreeView_AfterSelect_1(object sender, TreeViewEventArgs e)
        {
            try
            {
                if (e.Node.IsSelected == true)
                {
                    //Trace.WriteLine(DateTime.Now + "\t Camwe here when select the tree view");
                    if (appNameSelected == AppName.Recover || appNameSelected == AppName.Drdrill)
                    {
                        RecoverConfigurationPanel configurationPanel = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                        configurationPanel.SelectedVM(this, e.Node.Text);
                    }
                    else
                    {
                        try
                        {
                            Trace.WriteLine(DateTime.Now + " \t printing the values of the displayname " + e.Node.Text);
                            ConfigurationPanelHandler configurationPanel = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                            configurationPanel.SelectedVM(this, e.Node.Text);
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Came here when unselect option in last screen " + ex.Message);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: versionLabel_MouseEnter " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }

        private void networkDataGridView_CellClick_1(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.CHANGE_VALUE_COLUMN].Selected == true)
                    {
                        string nicName = networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.NIC_NAME_COLUMN].Value.ToString();
                        if (appNameSelected == AppName.Recover || appNameSelected == AppName.Drdrill)
                        {
                            RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                            //configurationScreen.networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.KEPP_OLD_VALUES_COLUMN].Value = false;
                            networkDataGridView.RefreshEdit();
                            config.NetworkSetting(this, nicName,e.RowIndex);
                        }
                        else
                        {
                            ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                            // configurationScreen.networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.KEPP_OLD_VALUES_COLUMN].Value = false;
                            networkDataGridView.RefreshEdit();
                            config.NetworkSetting(this, nicName,e.RowIndex);
                        }
                    }

                    
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: networkDataGridView_CellClick_1 " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void memoryValuesComboBox_SelectionChangeCommitted_1(object sender, EventArgs e)
        {
            try
            {
               
                if (appNameSelected == AppName.Recover || appNameSelected == AppName.Drdrill)
                {
                    RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                    config.MemorySelectedinGBOrMB(this);
                }
                else
                {
                    ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                    config.MemorySelectedinGBOrMB(this);
                }
               
            }
            catch (Exception ex)
            {
               
                Trace.WriteLine(DateTime.Now + "\t Caught exception in memoryValuesComboBox_SelectionChangeCommitted_1 " + ex.Message);
            }

        }

        private void memoryNumericUpDown_ValueChanged(object sender, EventArgs e)
        {
            /*try
            {
                if (_userSelectdMBGBDropList == false)
                {
                    if (_appName == AppName.RECOVER || _appName == AppName.DRDRILL)
                    {

                        RecoverConfigurationPanel config = (RecoverConfigurationPanel)_panelHandlerList[_index];
                        config.SelectedMemory(this);
                    }
                    else
                    {
                        ConfigurationPanelHandler config = (ConfigurationPanelHandler)_panelHandlerList[_index];
                        config.SelectedMemory(this);
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in memoryNumericUpDown_ValueChanged " + ex.Message);
            }*/
        }

        private void hardWareSettingsCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (appNameSelected == AppName.Recover || appNameSelected == AppName.Drdrill)
                {
                    RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                    config.SelectAllCheckBoxOfHardWareSetting(this);
                }
                else
                {
                    ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                    config.SelectAllCheckBoxOfHardWareSetting(this);
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in hardWareSettingsCheckBox_CheckedChanged " + ex.Message);
            }

        }

        private void applyRecoverySettingsCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (applyRecoverySettingsCheckBox.Checked == true)
                {
                    Host h1 = new Host();
                    foreach (Host h in sourceListByUser._hostList)
                    {
                        h1 = h;
                        if (tagBasedRadioButton.Checked == true)
                        {
                            h.tag = "LATEST";
                            h.recoveryOperation = 1;
                            h.tagType = "FS";
                        }
                        else if (timeBasedRadioButton.Checked == true)
                        {
                            h.tag = "LATESTTIME";
                            h.recoveryOperation = 2;
                            h.tagType = "FS";

                        }
                        else if (userDefinedTimeRadioButton.Checked == true)
                        {
                            h.userDefinedTime = universalTime;
                            h.recoveryOperation = 3;
                            h.selectedTimeByUser = universalTime;
                            WinTools.SetHostTimeinGMT( h1);
                            h1 = WinTools.GetHost;
                            h.userDefinedTime = h1.userDefinedTime;
                            Trace.WriteLine(DateTime.Now + " \t printing universal time " + universalTime);
                            h.tagType = "FS";

                        }
                        else if (specificTimeRadioButton.Checked == true)
                        {

                            h.userDefinedTime = universalTime;
                            h.selectedTimeByUser = universalTime;
                            h.recoveryOperation = 4;
                            WinTools.SetHostTimeinGMT( h1);
                            h1 = WinTools.GetHost;
                            h.userDefinedTime = h1.userDefinedTime;
                            h.tagType = "TIME";
                            h.tag = h1.userDefinedTime;
                        }
                        else if (specificTagRadioButton.Checked == true)
                        {
                            h.recoveryOperation = 5;
                            h.tagType = "FS";
                            if (specificTagTextBox.Text.Length != 0)
                            {
                                h.tag = specificTagTextBox.Text.TrimEnd();
                            }
                            else
                            {
                                MessageBox.Show("Please specify tag to which you want to rollback replication pairs.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                applyRecoverySettingsCheckBox.Checked = false;
                                return;
                            }
                        }

                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: applyRecoverySettingsCheckBox_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void historyLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            /*if (File.Exists(Directory.GetCurrentDirectory() + " \\patch.log"))
            {
                //System.Diagnostics.Process.Start("patch.log");
            }*/
        }

        private void patchLabel_MouseEnter(object sender, EventArgs e)
        {
            try
            {
                if (File.Exists(installationPath + " \\patch.log"))
                {
                    System.Windows.Forms.ToolTip toolTip = new System.Windows.Forms.ToolTip();
                    toolTip.AutoPopDelay = 5000;
                    toolTip.InitialDelay = 1000;
                    toolTip.ReshowDelay = 500;
                    toolTip.ShowAlways = true;
                    toolTip.GetLifetimeService();
                    toolTip.IsBalloon = true;

                    toolTip.UseAnimation = true;
                    StreamReader sr = new StreamReader(installationPath + " \\patch.log");

                    string s = sr.ReadToEnd().ToString();
                    toolTip.SetToolTip(patchLabel, s);
                    sr.Close();

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: patchLabel_MouseEnter " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }
        private void addSourceTreeView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + "\t Printing the Node vale in select event " + e.Node.Text);
                if (e.Node.IsSelected == true)
                {
                    Debug.WriteLine(DateTime.Now + "\t Printing the node text " + e.Node.Text);
                    string vSphereHost = null;
                    ESX_PrimaryServerPanelHandler primaryPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                    try
                    {
                        if (e.Node.Nodes.Count == 0)
                        {
                            if (e.Node.Parent.Text.ToString().Contains("MSCS("))
                            {

                            }
                            else
                            {
                                vSphereHost = e.Node.Parent.Text;
                            }
                            
                        }
                        Debug.WriteLine(DateTime.Now + "\t Printing the node text " + e.Node.Text);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text);
                        Trace.WriteLine(DateTime.Now + "\t " + ex.Message);
                    }

                    primaryPanelHandler.CheckHostContainsinWhichList(this, e.Node.Text, vSphereHost);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: versionLabel_MouseEnter " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }

        private void addSourceTreeView_AfterCheck(object sender, TreeViewEventArgs e)
        {
            try
            {
                Cursor = Cursors.WaitCursor;
                int index = 0;

                int nodeIndex = e.Node.Index;

                Trace.WriteLine(DateTime.Now + "\t Printing the count " + selectedSourceListByUser._hostList.Count.ToString());
                if (selectedSourceListByUser._hostList.Count == 0)
                {
                    Trace.WriteLine(DateTime.Now + "\t Came here because the selected list is zero");
                    if (e.Node.Checked == true)
                    {
                        ESX_PrimaryServerPanelHandler esx = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                        if (e.Node.Nodes.Count != 0)
                        {
                            if (!e.Node.Text.Contains("MSCS("))
                            {

                                foreach (TreeNode node in e.Node.Nodes)
                                {
                                    Trace.WriteLine(DateTime.Now + " \t printing the node name " + node.Text);
                                    Host h = new Host();
                                    h.displayname = node.Text;
                                    h.vSpherehost = node.Parent.Text;
                                    if (initialSourceListFromXml.DoesHostExist(h, ref index) == true)
                                    {
                                        h = (Host)initialSourceListFromXml._hostList[index];


                                        if (h.poweredStatus == "ON")
                                        {
                                            if (h.hostname != null && h.hostname != "NOT SET" && h.ip != "NOT SET" && h.ip != null && (h.vmwareTools == "OK" || h.vmwareTools.ToUpper() == "OUTOFDATE") && h.poweredStatus == "ON" && h.nicList.list.Count != 0 && h.datacenter != null && h.operatingSystem != null)
                                            {

                                                if (esx.CheckIdeDisk(this, h) == false)
                                                {

                                                }
                                                else
                                                {
                                                    Host tempHost1 = new Host();
                                                    tempHost1.hostname = h.hostname;
                                                    if (esx.CheckinVMinCXWithSilet(this, ref tempHost1) == true)
                                                    {
                                                        foreach (Hashtable hash in h.nicList.list)
                                                        {
                                                            foreach (Hashtable tempHash in tempHost1.nicList.list)
                                                            {
                                                                if (hash["nic_mac"] != null && tempHash["nic_mac"] != null)
                                                                {
                                                                    if (hash["nic_mac"].ToString().ToUpper() == tempHash["nic_mac"].ToString().ToUpper())
                                                                    {
                                                                        tempHash["network_name"] = hash["network_name"];
                                                                        tempHash["adapter_type"] = hash["adapter_type"];
                                                                    }
                                                                }
                                                            }
                                                            //   _selectedHost.nicList.list = tempHost1.nicList.list;
                                                        }
                                                        // h.nicList.list = tempHost1.nicList.list;
                                                        h.nicList.list = tempHost1.nicList.list;
                                                        h.resourcePool_src = h.resourcePool;
                                                        h.resourcepoolgrpname_src = h.resourcepoolgrpname;
                                                        selectedSourceListByUser.AddOrReplaceHost(h);
                                                        nextButton.Enabled = true;
                                                        totalCountLabel.Text = "Total number of vms selected for protection is " + selectedSourceListByUser._hostList.Count.ToString();
                                                        totalCountLabel.Visible = true;
                                                        if (node.Checked == false)
                                                        {
                                                            node.Checked = true;
                                                        }
                                                        //   _selectedHost.nicList.list = tempHost1.nicList.list;
                                                    }
                                                    else
                                                    {
                                                        //node.Checked = false;
                                                        Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx");

                                                    }

                                                    Trace.WriteLine("\t Came to here to add this vm " + h.displayname + " to the selected list ");


                                                }
                                                //EsxDiskDetails(allServersForm, _selectedHost);                                    
                                            }
                                        }
                                    }
                                }
                            }
                            else
                            {
                                foreach (TreeNode node in e.Node.Nodes)
                                {
                                    Trace.WriteLine(DateTime.Now + " \t printing the node name " + node.Text);
                                    Host h = new Host();
                                    h.displayname = node.Text;
                                    //h.vSpherehost = node.Parent.Text;
                                    if (initialSourceListFromXml.DoesHostExist(h, ref index) == true)
                                    {
                                        h = (Host)initialSourceListFromXml._hostList[index];

                                        
                                        if (h.poweredStatus == "ON")
                                        {
                                            if (h.hostname != null && h.hostname != "NOT SET" && h.ip != "NOT SET" && h.ip != null && (h.vmwareTools == "OK" || h.vmwareTools.ToUpper() == "OUTOFDATE") && h.poweredStatus == "ON" && h.nicList.list.Count != 0 && h.datacenter != null && h.operatingSystem != null)
                                            {

                                                if (esx.CheckIdeDisk(this, h) == false)
                                                {

                                                }
                                                else
                                                {
                                                    Host tempHost1 = new Host();
                                                    tempHost1.hostname = h.hostname;
                                                    if (esx.CheckinVMinCXWithSilet(this, ref tempHost1) == true)
                                                    {
                                                        foreach (Hashtable hash in h.nicList.list)
                                                        {
                                                            foreach (Hashtable tempHash in tempHost1.nicList.list)
                                                            {
                                                                if (hash["nic_mac"] != null && tempHash["nic_mac"] != null)
                                                                {
                                                                    if (hash["nic_mac"].ToString().ToUpper() == tempHash["nic_mac"].ToString().ToUpper())
                                                                    {
                                                                        tempHash["network_name"] = hash["network_name"];
                                                                        tempHash["adapter_type"] = hash["adapter_type"];
                                                                    }
                                                                }
                                                            }
                                                            //   _selectedHost.nicList.list = tempHost1.nicList.list;
                                                        }
                                                        // h.nicList.list = tempHost1.nicList.list;
                                                        h.nicList.list = tempHost1.nicList.list;
                                                        h.resourcePool_src = h.resourcePool;
                                                        h.resourcepoolgrpname_src = h.resourcepoolgrpname;
                                                        selectedSourceListByUser.AddOrReplaceHost(h);
                                                        nextButton.Enabled = true;
                                                        totalCountLabel.Text = "Total number of vms selected for protection is " + selectedSourceListByUser._hostList.Count.ToString();
                                                        totalCountLabel.Visible = true;
                                                        if (node.Checked == false)
                                                        {
                                                            node.Checked = true;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        //node.Checked = false;
                                                        Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx");

                                                    }

                                                    Trace.WriteLine("\t Came to here to add this vm " + h.displayname + " to the selected list ");


                                                }
                                                //EsxDiskDetails(allServersForm, _selectedHost);                                    
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            try
                            {
                                if (e.Node.Parent.Text.ToString().Contains("MSCS("))
                                {
                                    Host h = new Host();

                                    h.displayname = e.Node.Text;
                                    string vSphereHost = null;

                                    if (esx.GetSelectedSourceInfo(this, h.displayname, e.Node.Index, vSphereHost, e.Node) == false)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t User seleted VM " + h.displayname + "\t But with some reason it is not able to select");
                                        e.Node.Checked = false;
                                        h.plan = null;
                                        selectedSourceListByUser.RemoveHost(h);
                                        totalCountLabel.Text = "Total number of vms selected for protection is " + selectedSourceListByUser._hostList.Count.ToString();
                                    }
                                    if (e.Node.Parent.Checked == false)
                                    {
                                        e.Node.Parent.Checked = true;
                                    }
                                }
                                else
                                {
                                    Host h = new Host();
                                    h.vSpherehost = e.Node.Parent.Text.ToString();
                                    h.displayname = e.Node.Text;


                                    if (esx.GetSelectedSourceInfo(this, h.displayname, e.Node.Index, e.Node.Parent.Text.ToString(), e.Node) == false)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t User seleted VM " + h.displayname + "\t But with some reason it is not able to select");
                                        e.Node.Checked = false;
                                        h.plan = null;
                                        selectedSourceListByUser.RemoveHost(h);
                                        totalCountLabel.Text = "Total number of vms selected for protection is " + selectedSourceListByUser._hostList.Count.ToString();
                                    }
                                }
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t there are no vms  to read");
                                Trace.WriteLine(DateTime.Now + "\t " + ex.Message);
                                // MessageBox.Show("There are no vms to resume", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                //return ;
                            }
                            addSourceTreeView.SelectedNode = e.Node;
                        }
                    }

                }
                else
                {
                    Host h = new Host();
                    if (e.Node.Checked == false)
                    {
                        if (e.Node.Nodes.Count != 0)
                        {
                            if (e.Node.Text.ToString().Contains("MSCS("))
                            {


                                foreach (TreeNode node in e.Node.Nodes)
                                {

                                    h.displayname = node.Text;
                                    node.Checked = false;
                                    selectedSourceListByUser.RemoveHost(h);
                                    totalCountLabel.Text = "Total number of vms selected for protection is " + selectedSourceListByUser._hostList.Count.ToString();
                                    if (selectedSourceListByUser._hostList.Count == 0)
                                    {
                                        try
                                        {

                                            e.Node.Parent.Checked = false;

                                        }
                                        catch (Exception ex)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text);
                                            Trace.WriteLine(DateTime.Now + "\t " + ex.Message);
                                        }
                                    }
                                }

                            }
                            else
                            {
                                foreach (TreeNode node in e.Node.Nodes)
                                {
                                    h.vSpherehost = node.Parent.Text;
                                    h.displayname = node.Text;
                                    node.Checked = false;
                                    selectedSourceListByUser.RemoveHost(h);
                                    totalCountLabel.Text = "Total number of vms selected for protection is " + selectedSourceListByUser._hostList.Count.ToString();
                                    if (selectedSourceListByUser._hostList.Count == 0)
                                    {
                                        try
                                        {

                                            e.Node.Parent.Checked = false;

                                        }
                                        catch (Exception ex)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text);
                                            Trace.WriteLine(DateTime.Now + "\t " + ex.Message);
                                        }
                                    }
                                }
                            }
                        }
                        if (e.Node.Nodes.Count == 0)
                        {
                            h.displayname = e.Node.Text;
                            try
                            {
                                if (e.Node.Parent.Text.ToString().Contains("MSCS("))
                                {

                                }
                                else
                                {
                                    h.vSpherehost = e.Node.Parent.Text;
                                }
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text);
                                Trace.WriteLine(DateTime.Now + "\t " + ex.Message);
                            }
                            selectedSourceListByUser.RemoveHost(h);
                            totalCountLabel.Text = "Total number of vms selected for protection is " + selectedSourceListByUser._hostList.Count.ToString();
                            if (selectedSourceListByUser._hostList.Count == 0)
                            {
                                try
                                {

                                    e.Node.Parent.Checked = false;

                                }
                                catch (Exception ex)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text);
                                    Trace.WriteLine(DateTime.Now + "\t " + ex.Message);
                                }
                            }
                        }

                    }
                    if (e.Node.Checked == true)
                    {
                        ESX_PrimaryServerPanelHandler esx = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                        if (e.Node.Nodes.Count != 0)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Printing the esx ips  in next node " + h.esxIp + "  " + e.Node.Text);

                            foreach (TreeNode node in e.Node.Nodes)
                            {
                                h = new Host();
                                h.displayname = node.Text;
                                if (node.Parent.Text.ToString().Contains("MSCS("))
                                {

                                }
                                else
                                {
                                    h.vSpherehost = node.Parent.Text;
                                }
                                if (initialSourceListFromXml.DoesHostExist(h, ref index) == true)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Count of selected list " + selectedSourceListByUser._hostList.Count.ToString());
                                    Trace.WriteLine(DateTime.Now + "\t Printing node name " + node.Text);
                                    foreach (Host sourceHost in selectedSourceListByUser._hostList)
                                    {
                                        if (h.vSpherehost != null)
                                        {
                                            if (h.displayname == sourceHost.displayname && h.vSpherehost != sourceHost.vSpherehost)
                                            {
                                                MessageBox.Show("Already same display name is selected for this protection. Multiple machines with same displayname is not supported", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                                e.Node.Checked = false;
                                                Cursor = Cursors.Default;
                                                return;
                                            }
                                        }
                                    }

                                    h = (Host)initialSourceListFromXml._hostList[index];
                                    if (h.hostname != null && h.hostname != "NOT SET" && h.ip != "NOT SET" && h.ip != null && h.vmwareTools == "OK" && h.poweredStatus == "ON" && h.nicList.list.Count != 0 && h.datacenter != null && h.operatingSystem != null)
                                    {
                                        if (esx.CheckIdeDisk(this, h) == false)
                                        {
                                            node.Checked = false;
                                        }
                                        else
                                        {
                                            if (h.poweredStatus == "ON")
                                            {
                                                Host tempHost1 = new Host();
                                                tempHost1.hostname = h.hostname;
                                                if (esx.CheckinVMinCXWithSilet(this, ref tempHost1) == true)
                                                {
                                                    foreach (Hashtable hash in h.nicList.list)
                                                    {
                                                        foreach (Hashtable tempHash in tempHost1.nicList.list)
                                                        {
                                                            if (hash["nic_mac"] != null && tempHash["nic_mac"] != null)
                                                            {
                                                                if (hash["nic_mac"].ToString().ToUpper() == tempHash["nic_mac"].ToString().ToUpper())
                                                                {
                                                                    tempHash["network_name"] = hash["network_name"];
                                                                    tempHash["adapter_type"] = hash["adapter_type"];
                                                                }
                                                            }
                                                        }
                                                        //   _selectedHost.nicList.list = tempHost1.nicList.list;
                                                    }
                                                    //h.nicList.list = tempHost1.nicList.list;
                                                    h.nicList.list = tempHost1.nicList.list;
                                                    h.resourcePool_src = h.resourcePool;
                                                    h.resourcepoolgrpname_src = h.resourcepoolgrpname;
                                                    selectedSourceListByUser.AddOrReplaceHost(h);
                                                    nextButton.Enabled = true;
                                                    totalCountLabel.Text = "Total number of vms selected for protection is " + selectedSourceListByUser._hostList.Count.ToString();
                                                    totalCountLabel.Visible = true;
                                                    if (node.Checked == false)
                                                    {
                                                        node.Checked = true;
                                                    }
                                                }
                                                else
                                                {
                                                    //node.Checked = false;
                                                    Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx");

                                                }


                                                // _selectedSourceList.AddOrReplaceHost(h);
                                            }
                                        }
                                        totalCountLabel.Text = "Total number of vms selected for protection is " + selectedSourceListByUser._hostList.Count.ToString();
                                        totalCountLabel.Visible = true;
                                        //EsxDiskDetails(allServersForm, _selectedHost);
                                        nextButton.Enabled = true;
                                        addSourceTreeView.SelectedNode = node;

                                    }
                                }
                            }
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Came here because user has selected only single machine");

                            h.displayname = e.Node.Text;
                            try
                            {
                                if (e.Node.Parent.Text.ToString().Contains("MSCS("))
                                {

                                }
                                else
                                {
                                    h.vSpherehost = e.Node.Parent.Text;
                                }
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(ex.Message);
                                Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text);
                            }
                            if (esx.GetSelectedSourceInfo(this, h.displayname, e.Node.Index, h.vSpherehost, e.Node) == false)
                            {
                                e.Node.Checked = false;
                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t Count of selected list " + selectedSourceListByUser._hostList.Count.ToString());
                                try
                                {
                                    if (e.Node.Parent.Text.ToString().Contains("MSCS("))
                                    {
                                        if (e.Node.Parent.Checked == false)
                                        {
                                            e.Node.Parent.Checked = true;
                                        }
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Failed to select parent node " + ex.Message);
                                }
                                foreach (Host sourceHost in selectedSourceListByUser._hostList)
                                {
                                    if (h.vSpherehost != null)
                                    {
                                        if (h.displayname == sourceHost.displayname && h.vSpherehost != sourceHost.vSpherehost)
                                        {
                                            MessageBox.Show("Already same display name is selected for this protection. Multiple machines with same displayname is not supported", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            e.Node.Checked = false;
                                            return;
                                        }
                                    }
                                }
                                if (initialSourceListFromXml.DoesHostExist(h, ref index) == true)
                                {

                                    h = (Host)initialSourceListFromXml._hostList[index];
                                    h.resourcePool_src = h.resourcePool;
                                    h.resourcepoolgrpname_src = h.resourcepoolgrpname;
                                    selectedSourceListByUser.AddOrReplaceHost(h);
                                    addSourceTreeView.SelectedNode = e.Node;
                                }
                            }

                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException nht = new FormatException("Inner exception", ex);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadHostNode " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
            Cursor = Cursors.Default;
        }

        private void selectMasterTargetTreeView_AfterCheck(object sender, TreeViewEventArgs e)
        {
            try
            {
                if (e.Node.Checked == true)
                {
                    Host h = new Host();
                    string displayName;
                    string vSphereHostName = null;
                    Trace.WriteLine(DateTime.Now + "\t Came here to check whether the seleced node is parent or child " + e.Node.Text);
                    if (e.Node.Nodes.Count == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t User selected the child node as mt");

                        if (e.Node.Checked == true)
                        {
                            if (appNameSelected == AppName.Esx || appNameSelected == AppName.Bmr || appNameSelected == AppName.Offlinesync)
                            {
                                foreach (Host sourceHost in selectedSourceListByUser._hostList)
                                {
                                    h.targetDataStore = null;

                                }
                            }

                            displayName = e.Node.Text;
                            try
                            {
                                vSphereHostName = e.Node.Parent.Text;
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text);
                                Trace.WriteLine(DateTime.Now + "\t " + ex.Message);
                            }
                            selectedMasterListbyUser = new HostList();

                            for (int i = 0; i < selectMasterTargetTreeView.Nodes.Count; i++)
                            {
                                if (e.Node.Text != selectMasterTargetTreeView.Nodes[i].Text)
                                {
                                    //selectMasterTargetTreeView.Nodes[i].Checked = false;
                                    for (int j = 0; j < selectMasterTargetTreeView.Nodes[i].Nodes.Count; j++)
                                    {
                                        if (e.Node.Text != selectMasterTargetTreeView.Nodes[i].Nodes[j].Text)
                                        {
                                            selectMasterTargetTreeView.Nodes[i].Nodes[j].Checked = false;
                                        }
                                    }
                                }
                            }
                            if (e.Node.Nodes.Count == 0)
                            {
                                foreach (TreeNode node in e.Node.Nodes)
                                {

                                    if (node.Text != e.Node.Text)
                                    {
                                        node.Checked = false;
                                    }
                                    if (node.Nodes.Count != 0)
                                    {
                                        foreach (TreeNode nodes in node.Nodes)
                                        {
                                            if (node.Text != e.Node.Text)
                                            {
                                                nodes.Checked = false;
                                            }
                                        }
                                    }
                                }
                                Esx_SelectSecondaryPanelHandler selectSecondaryPanelHandler = (Esx_SelectSecondaryPanelHandler)panelHandlerListPrepared[indexPrepared];
                                if (selectSecondaryPanelHandler.MasterVmSelected(this, displayName,vSphereHostName) == false)
                                {
                                    e.Node.Checked = false;
                                }
                            }
                        }
                        else
                        {
                            selectedMasterListbyUser = new HostList();
                        }
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t User selected the parent node ");
                        e.Node.Checked = false;
                    }
                }
                else
                {
                    selectedMasterListbyUser = new HostList();
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadHostNode " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                
            }

        }

        internal void selectMasterTargetTreeView_DrawNode(object sender, DrawTreeNodeEventArgs e)
        {
            //int type = 1;
            //if (e.Node.Parent == null)
            //{
            //    Color backColor, foreColor;
            //    if ((e.State & TreeNodeStates.Selected) == TreeNodeStates.Selected)
            //    {
            //        backColor = SystemColors.Highlight;
            //        foreColor = SystemColors.HighlightText;
            //    }
            //    else if ((e.State & TreeNodeStates.Hot) == TreeNodeStates.Hot)
            //    {
            //        backColor = SystemColors.HotTrack;
            //        foreColor = SystemColors.HighlightText;
            //    }
            //    else
            //    {
            //        backColor = e.Node.BackColor;
            //        foreColor = e.Node.ForeColor;
            //    }
            //    using (SolidBrush brush = new SolidBrush(backColor))
            //    {
            //        e.Graphics.FillRectangle(brush, e.Node.Bounds);
            //    }
            //    TextRenderer.DrawText(e.Graphics, e.Node.Text, this.selectMasterTargetTreeView.Font, e.Node.Bounds, foreColor, backColor);

            //    if ((e.State & TreeNodeStates.Focused) == TreeNodeStates.Focused)
            //    {
            //        ControlPaint.DrawFocusRectangle(e.Graphics, e.Node.Bounds, foreColor, backColor);
            //    }
            //    //e.DrawDefault = false; 
            //    /*Esx_SelectSecondaryPanelHandler esx = (Esx_SelectSecondaryPanelHandler)_panelHandlerList[_index];
            //    esx.HideCheckBox(selectMasterTargetTreeView, e.Node);*/
            //}
            //else
            //{
            //    e.DrawDefault = true;
            //}
        }

        private void viewAllIPsLabel_MouseEnter(object sender, EventArgs e)
        {
            // Displaying all the ips as the tooltip for both click event and the mouseenter event.
            //try
            //{
            //    ESX_PrimaryServerPanelHandler esx_PrimaryPanelHandler = (ESX_PrimaryServerPanelHandler)_panelHandlerList[_index];
            //    esx_PrimaryPanelHandler.DisplayAllIPs(this);
            //}
            //catch (Exception ex)
            //{
            //    Trace.WriteLine(DateTime.Now + "\t Caught exception in viewAllIPsLabel_MouseEnter " + ex.Message);
            //}
            


        }

        private void viewAllIPsLabel_Click(object sender, EventArgs e)
        {// Displaying all the ips as the tooltip for both click event and the mouseenter event.
            try
            {
                ESX_PrimaryServerPanelHandler esx_PrimaryPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_PrimaryPanelHandler.DisplayAllIPs(this);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in viewAllIPsLabel_Click " + ex.Message);
            }
        }

        private void guestOSValueLabel_MouseEnter(object sender, EventArgs e)
        {
            try
            {

                //Displaying entire text of operationg system of the selected machine as tooltip.
                ESX_PrimaryServerPanelHandler esx_PrimaryPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_PrimaryPanelHandler.DisplayEntireTextofOs(this);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in guestOSValueLabel_MouseEnter " + ex.Message);
            }


        }

        
        

        private void keepSameAsSouceRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (keepSameAsSouceRadioButton.Checked == true)
                {
                    useForAllDisplayNameCheckBox.Visible = true;
                    enterTargetDisplayNameTextBox.Visible = false;
                    appliedPrefixTextBox.Visible = false;
                    suffixTextBox.Visible = false;
                    if (appNameSelected != AppName.Drdrill)
                    {
                        ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                        config.UseSameDisplayNameOnTarget(this);
                    }
                    else if (appNameSelected == AppName.Drdrill)
                    {
                        RecoverConfigurationPanel recovery = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                        recovery.UseSameDisplayNameOnTarget(this);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: keepSameAsSouceRadioButton_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void enterTargetDisplaynameRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (enterTargetDisplaynameRadioButton.Checked == true)
                {
                    useForAllDisplayNameCheckBox.Visible = false;
                    useForAllDisplayNameCheckBox.Checked = false;
                    enterTargetDisplayNameTextBox.Visible = true;
                    appliedPrefixTextBox.Visible = false;
                    suffixTextBox.Visible = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: enterTargetDisplaynameRadioButton_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void applyPrefixRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (applyPrefixRadioButton.Checked == true)
                {
                    appliedPrefixTextBox.Visible = true;
                    useForAllDisplayNameCheckBox.Visible = true;
                    suffixTextBox.Visible = false;
                    enterTargetDisplayNameTextBox.Visible = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: applyPrefixRadioButton_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void applySuffixRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (applySuffixRadioButton.Checked == true)
                {
                    useForAllDisplayNameCheckBox.Visible = true;
                    suffixTextBox.Visible = true;
                    appliedPrefixTextBox.Visible = false;
                    enterTargetDisplayNameTextBox.Visible = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: applySuffixRadioButton_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void enterTargetDisplayNameTextBox_KeyUp(object sender, KeyEventArgs e)
        {
            try
            {
                if (enterTargetDisplayNameTextBox.Text.Trim().Length != 0)
                {
                    if (appNameSelected == AppName.Drdrill)
                    {
                        RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                        config.UseDisplayNameForSingleVM(this);
                    }
                    else
                    {
                        ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                        config.UseDisplayNameForSingleVM(this);
                    }
                }
                else
                {
                    if (e.KeyCode != Keys.CapsLock)
                    {
                        enterTargetDisplayNameTextBox.Text = null;
                        MessageBox.Show("Please enter some text in the text box", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in enterTargetDisplayNameTextBox_KeyUp " + ex.Message);
            }
        }

        private void appliedPrefixTextBox_KeyUp(object sender, KeyEventArgs e)
        {
            try
            {
                if (appliedPrefixTextBox.Text.Trim().Length != 0)
                {
                    if (appNameSelected == AppName.Drdrill)
                    {
                        RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                        config.ApplyPreFixToSelectedVM(this);
                    }
                    else
                    {

                        ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                        config.ApplyPreFixToSelectedVM(this);

                    }
                }
                else
                {
                    if (e.KeyCode != Keys.CapsLock)
                    {
                        appliedPrefixTextBox.Text = null;
                        MessageBox.Show("Please enter some text in the text box", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in appliedPrefixTextBox_KeyUp " + ex.Message);
            }

        }

        private void suffixTextBox_KeyUp(object sender, KeyEventArgs e)
        {
            try
            {
                if (suffixTextBox.Text.Trim().Length != 0)
                {
                    if (appNameSelected == AppName.Drdrill)
                    {
                        RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                        config.ApplySuffixToSelectedVM(this);
                    }
                    else
                    {
                        ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                        config.ApplySuffixToSelectedVM(this);
                    }
                }
                else
                {
                    if (e.KeyCode != Keys.CapsLock)
                    {
                        suffixTextBox.Text = null;
                        MessageBox.Show("Please enter some text in the text box", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in suffixTextBox_KeyUp " + ex.Message);
            }
        }

        private void useForAllDisplayNameCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (useForAllDisplayNameCheckBox.Checked == true)
                {
                    if (keepSameAsSouceRadioButton.Checked == true)
                    {
                        if (appNameSelected == AppName.Drdrill)
                        {
                            foreach (Host h in recoveryChecksPassedList._hostList)
                            {
                                foreach (Host h1 in vmsSelectedRecovery._hostList)
                                {
                                    if (h1.displayname == h.displayname)
                                    {
                                        h.new_displayname = h1.new_displayname;
                                    }
                                }
                            }
                        }
                        else
                        {
                            foreach (Host h in selectedSourceListByUser._hostList)
                            {
                                h.new_displayname = h.displayname;
                            }
                        }
                    }
                    else if (applyPrefixRadioButton.Checked == true)
                    {
                        if (appliedPrefixTextBox.Text.Trim().Length == 0)
                        {
                            MessageBox.Show("First Fill the respected text box and then check use for all check box", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                            useForAllDisplayNameCheckBox.Checked = false;
                        }
                        if (appNameSelected == AppName.Drdrill)
                        {
                            foreach (Host h in recoveryChecksPassedList._hostList)
                            {
                                foreach (Host h1 in vmsSelectedRecovery._hostList)
                                {
                                    if (h1.displayname == h.displayname)
                                    {
                                        h.new_displayname = appliedPrefixTextBox.Text + "_" + h1.new_displayname;
                                        Trace.WriteLine(DateTime.Now + "\t Prinitng the new and old displaynames " + h.new_displayname + "\t" + h.displayname);
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            foreach (Host h in selectedSourceListByUser._hostList)
                            {
                                h.new_displayname = appliedPrefixTextBox.Text + "_" + h.displayname;
                            }
                        }
                    }
                    else if (applySuffixRadioButton.Checked == true)
                    {
                        if (suffixTextBox.Text.Trim().Length == 0)
                        {
                            MessageBox.Show("First Fill the respected text box and then check use for all check box", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                            useForAllDisplayNameCheckBox.Checked = false;
                        }
                        if (appNameSelected == AppName.Drdrill)
                        {
                            foreach (Host h in recoveryChecksPassedList._hostList)
                            {
                                foreach (Host h1 in vmsSelectedRecovery._hostList)
                                {
                                    if (h1.displayname == h.displayname)
                                    {
                                        h.new_displayname = h1.new_displayname + "_" + suffixTextBox.Text;
                                        Trace.WriteLine(DateTime.Now + "\t Prinitng the new and old displaynames " + h.new_displayname + "\t" + h.displayname);
                                    }
                                }
                            }
                        }
                        else
                        {
                            foreach (Host h in selectedSourceListByUser._hostList)
                            {
                                h.new_displayname = h.displayname + "_" + suffixTextBox.Text;
                            }
                        }
                    }

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: useForAllDisplayNameCheckBox_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void drDrillDatastroeDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            try
            {
                if (drDrillDatastroeDataGridView.RowCount > 0)
                {
                    drDrillDatastroeDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: drDrillDatastroeDataGridView_CurrentCellDirtyStateChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void drDrillDatastroeDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    Host h = new Host();
                    if (drDrillDatastroeDataGridView.Rows[e.RowIndex].Cells[DrDrillPanelHandler.DATASTORE_COLUMN].Selected == true)
                    {
                        h.displayname = drDrillDatastroeDataGridView.Rows[e.RowIndex].Cells[DrDrillPanelHandler.SOURCE_DISPLAYNAME_COLUMN].Value.ToString();
                        h.hostname = drDrillDatastroeDataGridView.Rows[e.RowIndex].Cells[DrDrillPanelHandler.HOST_NAME_COLUMN].Value.ToString();
                        if (drDrillDatastroeDataGridView.Rows[e.RowIndex].Cells[DrDrillPanelHandler.TARGET_DISPLAY_NAME_COLUMN].Value != null)
                        {
                            h.new_displayname = drDrillDatastroeDataGridView.Rows[e.RowIndex].Cells[DrDrillPanelHandler.TARGET_DISPLAY_NAME_COLUMN].Value.ToString();
                        }
                        DrDrillPanelHandler drDrillPanelHandler = (DrDrillPanelHandler)panelHandlerListPrepared[indexPrepared];
                        int index = 0;
                        if (recoveryChecksPassedList.DoesHostExist(h, ref index) == true)
                        {
                            h = (Host)recoveryChecksPassedList._hostList[index];
                            drDrillPanelHandler.EsxDataStoreSelectedDataStore(this, h, e.RowIndex);

                            drDrillPanelHandler.SelectDataStoreForFirstTime(this, rowIndex);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to find the machine in recoverylist");
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: drDrillDatastroeDataGridView_CellValueChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void reviewDataGridView_DataError(object sender, DataGridViewDataErrorEventArgs e)
        {
            try
            {
                // MessageBox.Show("Error happened " + e.Context.ToString());
                if (e.Context == DataGridViewDataErrorContexts.Commit)
                {
                    // MessageBox.Show("Commit error");
                }
                if (e.Context == DataGridViewDataErrorContexts.CurrentCellChange)
                {
                    // MessageBox.Show("Cell change");
                }
                if (e.Context == DataGridViewDataErrorContexts.Parsing)
                {
                    //MessageBox.Show("parsing error");
                }
                if (e.Context == DataGridViewDataErrorContexts.LeaveControl)
                {
                    // MessageBox.Show("leave control error");
                }
                if ((e.Exception) is ConstraintException)
                {
                    DataGridView view = (DataGridView)sender;
                    view.Rows[e.RowIndex].ErrorText = "an error";
                    view.Rows[e.RowIndex].Cells[e.ColumnIndex].ErrorText = "an error";
                    e.ThrowException = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: reviewDataGridView_DataError " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        

       

        private void advancedSettingsLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
               
                    if (appNameSelected == AppName.Drdrill)
                    {
                        RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                        config.ShowAdvancedSettingForm(this);
                    }
                    else
                    {
                        ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                        config.ShowAdvancedSettingForm(this);
                    }
                   
                
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: advancedSettingsLinkLabel " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void memoryNumericUpDown_Leave(object sender, EventArgs e)
        {
            try
            {
                
                    if (appNameSelected == AppName.Recover || appNameSelected == AppName.Drdrill)
                    {

                        RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                        config.SelectedMemory(this);
                    }
                    else
                    {
                        ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                        config.SelectedMemory(this);
                    }
                
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in memoryNumericUpDown_ValueChanged " + ex.Message);
            }

        }

        private void warnigLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                errorProvider.Clear();
                ESX_PrimaryServerPanelHandler esx = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx.ShowSkipData(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: warnigLinkLabel_LinkClicked " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void secondaryPanelLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                errorProvider.Clear();
                Esx_SelectSecondaryPanelHandler esx = (Esx_SelectSecondaryPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx.ShowSkipData(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: secondaryPanelLinkLabel_LinkClicked " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void recoveryWarningLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                errorProvider.Clear();
                Esx_RecoverPanelHandler recovery = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
                recovery.ShowSkipData(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryWarningLinkLabel_LinkClicked " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void p2vOSComboBox_SelectedValueChanged(object sender, EventArgs e)
        {
            try
            {
                if (p2vOSComboBox.SelectedItem.ToString() == "Linux")
                {
                    osTypeSelected = OStype.Linux;
                    
                }
                else if (p2vOSComboBox.SelectedItem.ToString() == "Windows")
                {

                    osTypeSelected = OStype.Windows;
                }
                else if(p2vOSComboBox.SelectedItem.ToString() == "Solaris")
                {
                    osTypeSelected = OStype.Solaris;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: p2vOSComboBox_SelectedValueChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void recoveryReviewDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (recoveryReviewDataGridView.RowCount > 0)
            {
                recoveryReviewDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void recoveryReviewDataGridView_DataError(object sender, DataGridViewDataErrorEventArgs e)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + "\t User has changed the recovery order ");
                Trace.WriteLine(DateTime.Now + " \tError happened " + e.Context.ToString());

                if (e.Context == DataGridViewDataErrorContexts.Commit)
                {
                    Trace.WriteLine(DateTime.Now + " \tCommit error");
                }
                if (e.Context == DataGridViewDataErrorContexts.CurrentCellChange)
                {
                    Trace.WriteLine(DateTime.Now + " \tCell change");
                }
                if (e.Context == DataGridViewDataErrorContexts.Parsing)
                {
                    Trace.WriteLine(DateTime.Now + " \tparsing error");
                }
                if (e.Context == DataGridViewDataErrorContexts.LeaveControl)
                {
                    Trace.WriteLine(DateTime.Now + " \tleave control error");
                }

                if ((e.Exception) is ConstraintException)
                {
                    DataGridView view = (DataGridView)sender;
                    view.Rows[e.RowIndex].ErrorText = "an error";
                    view.Rows[e.RowIndex].Cells[e.ColumnIndex].ErrorText = "an error";

                    e.ThrowException = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryReviewDataGridView_DataError " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void recoveryReviewDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex >= 0)
            {
                if (recoveryReviewDataGridView.Rows[e.RowIndex].Cells[ReviewPanelHandler.RECOVERY_ORDER_COLUMN].Selected == true)
                {
                    recoveryReviewDataGridView.Rows[e.RowIndex].Cells[ReviewPanelHandler.RECOVERY_ORDER_COLUMN].Value = recoveryReviewDataGridView.Rows[e.RowIndex].Cells[ReviewPanelHandler.RECOVERY_ORDER_COLUMN].Value.ToString();
                }
            }
        }

        private void recoveryReviewDataGridView_SelectionChanged(object sender, EventArgs e)
        {

        }

        private void cxBasedPhysicalMachineRadioButton_CheckedChanged(object sender, EventArgs e)
        {
                       
            
        }

        private void wmiBasedPhysicalRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            
        }


        private bool CheckDuplicateEntryForCleanUP(ref Host h)
        {
            Trace.WriteLine(DateTime.Now + "\t Verifying duplicate entries of host " + h.hostname);
            WinPreReqs win = new WinPreReqs("", "", "", "");
            string cxip = WinTools.GetcxIPWithPortNumber();
            int returncode = win.CheckAnyDuplicateEntries(h.hostname, h.ip, cxip);
            if (returncode == 1)
            {
                Trace.WriteLine(DateTime.Now + "\t Found multiple entries in CX for the host " + h.hostname);
                HostList duplicateList = new HostList();
                duplicateList = WinPreReqs.duplicateList;
                Trace.WriteLine(DateTime.Now + "\t Count to duplicate list for the host " + h.hostname + duplicateList._hostList.Count.ToString());
                if (duplicateList._hostList.Count != 0)
                {
                    dummyHost = h;
                    duplicateListOfMachines = duplicateList;
                    duplicateEntires = true;
                    return false;
                }      
            }
            return true;
        }

        //Added this method to call this in background worker do work so that screen may not stuck while selecting machines
        private bool SelectMachine()
        {
            try
            {
                if (nodeSelected.Nodes.Count == 0)
                {


                    Host h = new Host();
                    h.hostname = nodeSelected.Text;
                    int index = 0;
                    P2V_PrimaryServerPanelHandler p2v = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                    if (p2v._initialList.DoesHostExist(h, ref index) == true)
                    {
                        h = (Host)p2v._initialList._hostList[index];
                        if (h.cluster == "yes")
                        {
                            if (node.Parent.Checked == false)
                            {
                                SetLeftTickerForTreeView(nodeSelected.Parent, true);
                                //e.Node.Parent.Checked = true;
                            }
                        }
                    }

                    if (CheckDuplicateEntryForCleanUP(ref h) == false)
                    {
                       // SetLeftTickerForTreeView(nodeSelected, false);
                        //e.Node.Checked = false;
                        return false;
                    }


                    if (p2v.GetResponceFromCXAPI(this, h.hostname) == false)
                    {
                        SetLeftTickerForTreeView(nodeSelected, false);
                        Trace.WriteLine(DateTime.Now + "\t Failed to get responce form the cx server");
                        //MessageBox.Show("Failed to fetch machine information from CX server", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        selectedMachineSuccessfully = false;

                    }
                    else
                    {
                        selectedMachineSuccessfully = true;
                        //SetLeftTickerForTreeView(nodeSelected, false);
                        currentDisplayedPhysicalHostIPselected = nodeSelected.Text;
                        //p2VPrimaryTreeView.SelectedNode = nodeSelected;
                    }
                }
                else
                {
                    TreeNode treenode = nodeSelected;
                    Host h = new Host();
                    foreach (TreeNode nodes in nodeSelected.Nodes)
                    {
                        //nodes.Checked = true;
                        h = new Host();
                        h.hostname = nodes.Text;

                        P2V_PrimaryServerPanelHandler p2v = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                        if (CheckDuplicateEntryForCleanUP(ref h) == false)
                        {
                            //SetLeftTickerForTreeView(nodes, false);
                            return false;
                        }

                        if (p2v.GetResponceFromCXAPI(this, h.hostname) == false)
                        {

                            Trace.WriteLine(DateTime.Now + "\t Failed to get responce form the cx server");
                            //MessageBox.Show("Failed to fetch machine information from CX server", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            selectedMachineSuccessfully = false;

                        }
                        else
                        {

                            currentDisplayedPhysicalHostIPselected = nodeSelected.Text;
                            selectedMachineSuccessfully = true;
                            SetLeftTickerForTreeView(nodes, true);
                            //nodes.Checked = true;
                        }
                    }
                }

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryReviewDataGridView_DataError " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            return true;
        }

        private void p2VPrimaryTreeView_AfterCheck(object sender, TreeViewEventArgs e)
        {



            

            if (appNameSelected == AppName.V2P)
            {
               
                try
                {
                    for (int j = 0; j < p2VPrimaryTreeView.Nodes[0].Nodes.Count; j++)
                    {
                        if (e.Node.Text != p2VPrimaryTreeView.Nodes[0].Nodes[j].Text)
                        {
                            if (p2VPrimaryTreeView.Nodes[0].Nodes[j].Checked == true)
                            {
                                p2VPrimaryTreeView.Nodes[0].Nodes[j].Checked = false;
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to unslect the previous selections of mt at the time of v2p");
                    Trace.WriteLine(DateTime.Now + "\t Error Message for unselection " + ex.Message);
                }
                selectedMasterListbyUser = new HostList();

                
            }

           

                if (e.Node.Checked == true)
                {

                    nodeSelected = e.Node;
                    treeNodeIndex = e.Node.Index;
                    //node = e.Node;
                    p2VTreeViewWhenselected = p2VPrimaryTreeView;
                    progressBar.Visible = true;
                    p2VPrimaryTreeView.Enabled = false;
                    p2vSelectBackgroundWorker.RunWorkerAsync();
                    return;
                }
                else
                {
                    currentDisplayedPhysicalHostIPselected = e.Node.Text;
                    p2VPrimaryTreeView.SelectedNode = e.Node;
                }

            //            Trace.WriteLine(DateTime.Now + "\t User has selected all cluster nodes for protection " + e.Node.Text);
            //        }
            //    }
            //    else
            //    {
            //        if (e.Node.Nodes.Count != 0)
            //        {
            //            foreach (TreeNode node in e.Node.Nodes)
            //            {
            //                node.Checked = false;
            //            }
            //        }
            //    }

            //}
            if (e.Node.Checked == false)
            {

                if (e.Node.Nodes.Count != 0)
                {
                    foreach (TreeNode node in e.Node.Nodes)
                    {
                        node.Checked = false;
                    }
                }
                P2V_PrimaryServerPanelHandler primaryPanelHandler = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                primaryPanelHandler.UnslectMachine(this, e.Node.Text);
            }
            //if (e.Node.Checked == true && wmiBasedPhysicalRadioButton.Checked == true)
            //{
            //    P2V_PrimaryServerPanelHandler primaryPanelHandler = (P2V_PrimaryServerPanelHandler)_panelHandlerList[_index];
            //    if (primaryPanelHandler.AddexistingMachinetoWmiBasedProtection(this, e.Node.Text) == false)
            //    {
            //        Trace.WriteLine(DateTime.Now + "\t Failed to add this machine to existing protection " + e.Node.Text);
            //    }
            //    else
            //    {
            //        _currentDisplayedPhysicalHostIp = e.Node.Text;
            //    }

            //}




        }

        

        private void p2VPrimaryTreeView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if (e.Node.Nodes.Count == 0)
            {
                Host h = new Host();
                h.hostname = e.Node.Text;
                int index = 0;

                if (selectedSourceListByUser.DoesHostExist(h, ref index) == true)
                {
                    currentDisplayedPhysicalHostIPselected = h.hostname;
                    h = (Host)selectedSourceListByUser._hostList[index];
                    p2vIPValueLabel.Text = h.ip;
                    p2vOSValueLabel.Text = h.operatingSystem;
                    p2vRamValueLabel.Text = h.memSize.ToString() + "(MB)";
                    p2vCpuValueLabel.Text = h.cpuCount.ToString();
                    if (h.machineType != null)
                    {
                        machineTypeValueLabel.Text = h.machineType;
                    }
                    else
                    {
                        machineTypeValueLabel.Text = "PhysicalMachine";
                    }
                    if (h.cluster == "yes")
                    {
                        p2vClusterValueLabel.Text = "Yes";
                    }
                    else
                    {
                        p2vClusterValueLabel.Text = "No";
                    }

                    p2vHostnameValueLabel.Text = h.hostname;
                    p2vPanel.Visible = true;
                    P2V_PrimaryServerPanelHandler p2v = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                    p2v.DisplayDriveDetails(this, h);
                    
                }
                else
                {
                    p2vPanel.Visible = false;
                    physicalMachineDiskSizeDataGridView.Visible = false;
                    hostDetailsLabel.Visible = false;
                    selectedDiskLabel.Visible = false;
                    totalNumberOfDisksLabel.Visible = false;
                    Trace.WriteLine(DateTime.Now + "\t Selected machine does not existing in the selectedsourcelist " + h.hostname);
                }
            }
            else
            {
                p2vPanel.Visible = false;
                physicalMachineDiskSizeDataGridView.Visible = false;
                hostDetailsLabel.Visible = false;
            }
        }

        private void p2vOSValueLabel_MouseEnter(object sender, EventArgs e)
        {
            try
            {
                P2V_PrimaryServerPanelHandler p2v_Handler = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                p2v_Handler.DisplayOSDetails(this);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to display tool tip for the os " + ex.Message);
            }
        }

        private void p2vLoadingBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            P2V_PrimaryServerPanelHandler p2v = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
            p2v.GetDetailsUsingBackGroundWorker(this);
        }

        private void p2vLoadingBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void p2vLoadingBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                P2V_PrimaryServerPanelHandler p2v = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                p2v.LoadTreeViewAfterGettingDetails(this);
                progressBar.Visible = false;
                primaryServerAddButton.Enabled = true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: p2vLoadingBackgroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            
        }

        private void viewAllIPsLabel_MouseLeave(object sender, EventArgs e)
        {
            try
            {
                ESX_PrimaryServerPanelHandler esx_PrimaryPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_PrimaryPanelHandler.UnDisplayAllIPs(this);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in viewAllIPsLabel_MouseEnter " + ex.Message);
            }
        }

        private void guestOSValueLabel_MouseLeave(object sender, EventArgs e)
        {
            try
            {

                //Displaying entire text of operationg system of the selected machine as tooltip.
                ESX_PrimaryServerPanelHandler esx_PrimaryPanelHandler = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                esx_PrimaryPanelHandler.UnDisplayEntireTextofOs(this);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception in guestOSValueLabel_MouseEnter " + ex.Message);
            }

        }

        private void p2vOSValueLabel_MouseLeave(object sender, EventArgs e)
        {
            try
            {
                P2V_PrimaryServerPanelHandler p2v_Handler = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                p2v_Handler.UnDisplayOSDetails(this);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to display tool tip for the os " + ex.Message);
            }

        }

        private void preReqChecksBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx_ProtectionPanelHandler eph = (Esx_ProtectionPanelHandler)panelHandlerListPrepared[indexPrepared];
                eph.PreReqCheck(this);      
                
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed at target readiness check " + ex.Message);
            }
           
        }

        private void preReqChecksBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void preReqChecksBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                Esx_ProtectionPanelHandler eph = (Esx_ProtectionPanelHandler)panelHandlerListPrepared[indexPrepared];
                if (eph.PostReadinessCheck(this) == true)
                {
                    if (appNameSelected != AppName.Adddisks)
                    {
                        planNameTableLayoutPanel.Visible = true;
                        createNewPlanCheckBox.Checked = true;
                       
                        if (Esx_ProtectionPanelHandler._hasPlanName == false)
                        {
                            createNewPlanCheckBox.Enabled = false;
                            createNewPlanCheckBox.Visible = false;
                        }
                        protectionText.AppendText("Run readiness check completed." + Environment.NewLine + "Please enter plan name and press protect button" + Environment.NewLine);
                        if (planNameText.Text.Length == 0)
                        {

                            errorProvider.SetError(planNameText, "Enter plan name");
                        }
                        else
                        {
                            nextButton.Enabled = true;
                        }
                        planNameLabel.Visible = true;
                        planNameText.Visible = true;
                    }
                    else
                    {
                        mandatoryLabel.Visible = false;
                        mandatorydoubleFieldLabel.Visible = false;
                        nextButton.Enabled = true;
                    }
                }
                else
                {
                    nextButton.Enabled = false;
                }
                reviewDataGridView.Enabled = true;

                preReqsbutton.Enabled = true;
                progressBar.Visible = false;
                previousButton.Enabled = true;
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed at postreadiness check " + ex.Message);
            }

        }

        private void networkDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (networkDataGridView.RowCount > 0)
            {
                networkDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void networkDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.SELECT_UNSELECT_NIC_COLUMN].Selected == true)
                    {
                        string nicName = networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.NIC_NAME_COLUMN].Value.ToString();
                        if (appNameSelected == AppName.Recover || appNameSelected == AppName.Drdrill)
                        {
                            RecoverConfigurationPanel config = (RecoverConfigurationPanel)panelHandlerListPrepared[indexPrepared];
                            //configurationScreen.networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.KEPP_OLD_VALUES_COLUMN].Value = false;
                            networkDataGridView.RefreshEdit();
                            config.SelectedNic(this, nicName, e.RowIndex);
                        }
                        else
                        {
                            ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
                            networkDataGridView.RefreshEdit();
                            config.SelectedNic(this, nicName, e.RowIndex);
                        }
                    }

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: networkDataGridView_CellValueChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void fxJobRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            ReviewPanelHandler._fileMadeandCopiedSuccessfully = false;
            recoveryPlanNameLabel.Visible = true;
            recoveryPlanNameTextBox.Visible = true;
            recoveryPlanNameTextBox.Enabled = true;
            planNameSelected = null;


        }

        private void wmiBasedRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (wmiBasedRadioButton.Checked == true)
                {
                    ReviewPanelHandler._fileMadeandCopiedSuccessfully = false;
                    recoveryPlanNameLabel.Visible = false;
                    recoveryPlanNameTextBox.Visible = false;
                    planNameSelected = null;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: wmiBasedRadioButton_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void recoveryLaterRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (recoveryLaterRadioButton.Checked == true)
                {



                    ReviewPanelHandler._fileMadeandCopiedSuccessfully = false;

                    recoveryThroughGroupBox.Visible = false;
                    fxJobRadioButton.Checked = true;
                    planNameSelected = null;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryLaterRadioButton_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void recoveryNowRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                planNameSelected = null;
                ReviewPanelHandler._fileMadeandCopiedSuccessfully = false;
                if (recoveryNowRadioButton.Checked == true)
                {

                    if (masterHostAdded.os == OStype.Windows)
                    {
                        recoveryThroughGroupBox.Visible = true;
                    }
                    else
                    {
                        recoveryThroughGroupBox.Visible = false;
                    }
                    fxJobRadioButton.Checked = true;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: recoveryNowRadioButton_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void selectSourceVMBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Host h = new Host();
                ESX_PrimaryServerPanelHandler esx_SelectVMs = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                if (esx_SelectVMs.CheckinVMinCX(this, ref h) == false)
                {

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: selectSourceVMBackgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void selectSourceVMBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                ESX_PrimaryServerPanelHandler esx_SelectVMs = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                if (esx_SelectVMs.CheckReturnCodeOfBackGroundWorker(this) == true)
                {

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: selectSourceVMBackgroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void selectSourceVMBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            
        }

        private void mapDiskDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            try
            {
                if (mapDiskDataGridView.RowCount > 0)
                {

                    mapDiskDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: mapDiskDataGridView_CurrentCellDirtyStateChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void mapDiskDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (mapDiskDataGridView.RowCount > 0)
                    {
                        if (mapDiskDataGridView.Rows[e.RowIndex].Cells[2].Selected == true && mapDiskDataGridView.Rows[e.RowIndex].Cells[2].Value != null)
                        {
                            MapDiskPanelHandler map = (MapDiskPanelHandler)panelHandlerListPrepared[indexPrepared];

                            map.SelectedDisk(this, e.RowIndex);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: mapDiskDataGridView_CellValueChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void v2pReadinessCheckButton_Click(object sender, EventArgs e)
        {
            try
            {
               
                MapDiskPanelHandler map = (MapDiskPanelHandler)panelHandlerListPrepared[indexPrepared];
                v2pCheckReportDataGridView.Rows.Clear();
                if (map.PreReadinesschecks(this) == true)
                {
                    map.ReadinessChecks(this);
                    map._rowIndex = 0;
                    foreach (Host h in selectedSourceListByUser._hostList)
                    {
                        map.ReloadDataGridViewAfterRunReadinessCheck(this, h);
                    }
                    foreach (Host h in selectedMasterListbyUser._hostList)
                    {
                        map.ReloadDataGridViewAfterRunReadinessCheck(this, h);
                    }
                    v2pLogTabControl.SelectTab(v2pGridTabPage);
                    v2pCheckReportDataGridView.Visible = true;
                    if (map.CheckReadniessChecks(this) == false)
                    {
                        nextButton.Enabled = false;
                    }
                    else
                    {
                        //errorProvider.SetError(v2pPlanNameTextBox, "Enter plan name");
                        nextButton.Enabled = true;
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: v2pReadinessCheckButton_Click " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void retentionInSizeTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            try
            {
                if (Char.IsDigit(e.KeyChar) || e.KeyChar == '\b')
                {
                    e.Handled = false;
                }
                else
                {
                    e.Handled = true;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: retentionInSizeTextBox_KeyPress " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void retentionInDaysTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            try
            {
                if (Char.IsDigit(e.KeyChar) || e.KeyChar == '\b')
                {
                    e.Handled = false;
                }
                else
                {
                    e.Handled = true;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: retentionInDaysTextBox_KeyPress " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void consistencyTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            try
            {
                if (Char.IsDigit(e.KeyChar) || e.KeyChar == '\b')
                {
                    e.Handled = false;
                }
                else
                {
                    e.Handled = true;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: consistencyTextBox_KeyPress " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void drDrillDatastroeDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (drDrillDatastroeDataGridView.Rows[e.RowIndex].Cells[DrDrillPanelHandler.SELECT_LUNS_COLUMN].Selected == true)
                    {
                        DrDrillPanelHandler drDrillPanelHandler = (DrDrillPanelHandler)panelHandlerListPrepared[indexPrepared];
                        drDrillPanelHandler.SelectRDMLun(this, e.RowIndex);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: drDrillDatastroeDataGridView_CellContentClick " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void drDrillReadinessCheckBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                ReviewPanelHandler review = (ReviewPanelHandler)panelHandlerListPrepared[indexPrepared];
                review.ReadinessChecks(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: drDrillReadinessCheckBackgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void drDrillReadinessCheckBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {

        }

        private void drDrillReadinessCheckBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                ReviewPanelHandler review = (ReviewPanelHandler)panelHandlerListPrepared[indexPrepared];
                review.PostReadinessChecks(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: drDrillReadinessCheckBackgroundWorker_RunWorkerCompleted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void drDrillReadinessCheckButton_Click(object sender, EventArgs e)
        {
            try
            {
                ReviewPanelHandler review = (ReviewPanelHandler)panelHandlerListPrepared[indexPrepared];
                review.CheckRecoveryOrderandPrePareXMLfile(this);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: drDrillReadinessCheckButton_Click " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void arrayBasedDrDrillRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (arrayBasedDrDrillRadioButton.Checked == true)
            {
              string message = "You can take snapshot of datastore LUNs on which protected target VMs are exist." + Environment.NewLine 

                                + "Make sure that datastore(s) from newly created virtual LUNs are attached to same ESX server and datastore(s)"  

                                + "discovered at ESX level." + Environment.NewLine 


                                + "Click Yes to proceed." + Environment.NewLine 

                                + "Click No to choose other option for DR Drill";
              switch (MessageBox.Show(message,"Note", MessageBoxButtons.YesNo, MessageBoxIcon.Information))
              {
                  case DialogResult.No:
                      hostBasedRadioButton.Checked = true;
                      return;
              }
            }
        }

        private void parallelVolumeTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            try
            {
                if (Char.IsDigit(e.KeyChar) || e.KeyChar == '\b')
                {
                    e.Handled = false;
                }
                else
                {
                    e.Handled = true;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: parallelVolumeTextBox_KeyPress " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void v2pLogClearLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {

                v2pLogTextBox.Clear();
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to clear the log at the time v2p recovery " + ex.Message);
            }
        }

        private void powerOnALLVMSCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (powerOnALLVMSCheckBox.Checked == true)
                {
                    for (int i = 0; i < recoveryReviewDataGridView.RowCount; i++)
                    {
                        recoveryReviewDataGridView.Rows[i].Cells[ReviewPanelHandler.POWER_ON_VM].Value = true;
                        recoveryReviewDataGridView.RefreshEdit();
                    }
                }
                else
                {
                    for (int i = 0; i < recoveryReviewDataGridView.RowCount; i++)
                    {
                        recoveryReviewDataGridView.Rows[i].Cells[ReviewPanelHandler.POWER_ON_VM].Value = false;
                        recoveryReviewDataGridView.RefreshEdit();
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: powerOnALLVMSCheckBox_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                
            }
        }

        private void validateBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            Esx_RecoverPanelHandler recovery = (Esx_RecoverPanelHandler)panelHandlerListPrepared[indexPrepared];
            recovery.ValidateCredentials(this);
        }

        private void validateBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            progressBar.Visible = false;
            recoverPanel.Enabled = true;
            progressLabel.Visible = false;
        }

        private void validateBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {

                                
        }

        private void esxDiskInfoDatagridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex >= 0)
            {
                if (esxDiskInfoDatagridView.Rows[e.RowIndex].Cells[3].Selected == true)
                {
                    ESX_PrimaryServerPanelHandler esx_Primary = (ESX_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                    esx_Primary.DislpayDiskDetails(this, e.RowIndex);

                }
            }
        }

        private void newDiskDetailsDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex >= 0)
            {
                if (newDiskDetailsDataGridView.Rows[e.RowIndex].Cells[4].Selected == true)
                {
                    AddDiskPanelHandler add = (AddDiskPanelHandler)panelHandlerListPrepared[indexPrepared];
                    add.DisplayDiskDetails(this, e.RowIndex);
                }
            }
        }

        private void UpdateNatPsIPlinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            //Passing selecteduserlist as arugment to NAT config screen there it may use to chekc for selected process servers list.
            NatConFigurationForm natConfigurationForm = new NatConFigurationForm(selectedSourceListByUser);           
            natConfigurationForm.ShowDialog();
        }

        private void srcToPSCommunicationCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
            config.SrcToPSCommunication(this);


        }

        private void psToTgtCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
            config.PsToTargetCommunication(this);
        }

        private void useForAllNATCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            ConfigurationPanelHandler config = (ConfigurationPanelHandler)panelHandlerListPrepared[indexPrepared];
            config.NatSettingsUseForAll(this);
        }

       
        
       


        internal bool VerfiyEsxCreds(string IP, string type)
        {
            Cxapicalls cxAPi = new Cxapicalls();
            cxAPi.GetHostCredentials(IP);
            if (Cxapicalls.GetUsername == null && Cxapicalls.GetPassword == null)
            {
                SourceEsxDetailsPopUpForm sourceEsx = new SourceEsxDetailsPopUpForm();
                if (type != "source")
                {
                    sourceEsx.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                }
                else
                {
                    sourceEsx.ipLabel.Text = "Source vCenter/vSphere IP/Name *";
                }
                sourceEsx.sourceEsxIpText.ReadOnly = false;
                sourceEsx.sourceEsxIpText.Text = esxIPSelected;
                sourceEsx.ShowDialog();
                esxIPSelected = sourceEsx.sourceEsxIpText.Text;
                tgtEsxUserName = sourceEsx.userNameText.Text;
                tgtEsxPassword = sourceEsx.passWordText.Text;
                masterHostAdded.esxIp = sourceEsx.sourceEsxIpText.Text;
                masterHostAdded.esxUserName = sourceEsx.userNameText.Text;
                masterHostAdded.esxPassword = sourceEsx.passWordText.Text;

                if (sourceEsx.canceled == false)
                {
                    if (cxAPi. UpdaterCredentialsToCX(IP, tgtEsxUserName, tgtEsxPassword) == false)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update mete data to cx");
                        return false;
                    }
                }

            }
            return true;
        }





        internal static bool SetFolderPermissions(string path)
        {
            try
            {
                DirectoryInfo dInfo = new DirectoryInfo(path);

                // Get a DirectorySecurity object that represents the  
                // current security settings.
                DirectorySecurity dSecurity = dInfo.GetAccessControl();
                dSecurity.SetAccessRuleProtection(true, false);
                dSecurity.AddAccessRule(new FileSystemAccessRule("BUILTIN\\Administrators", FileSystemRights.FullControl, AccessControlType.Allow));
                dSecurity.AddAccessRule(new FileSystemAccessRule("NT AUTHORITY\\SYSTEM", FileSystemRights.FullControl, AccessControlType.Allow));

                // Set the new access settings.
                dInfo.SetAccessControl(dSecurity);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to set permissions " + ex.Message);
                return false;
            }

            return true;
        }

        private void specificTagRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (specificTagRadioButton.Checked == true)
                {
                    specificTagTextBox.Visible = true;
                    specificTagTextBox.Location = new Point(100, 112);
                    fileSystemTagLabel.Location = new Point(100, 135);
                    //Added new label to inform user to provide only filesystem tag 
                    fileSystemTagLabel.Visible = true;
                    specificTagRadioButton.Location = new Point(10, 112);
                    applyRecoverySettingsCheckBox.Location = new Point(12, 164);
                    dateTimePicker.Visible = false;
                    specificTimeDateTimePicker.Visible = false;
                    // RecoveryChanges();
                    Host h = new Host();
                    h.hostname = recoverDataGridView.Rows[listIndexPrepared].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value.ToString();
                    int index = 0;
                    if (sourceListByUser.DoesHostExist(h, ref index) == true)
                    {
                        h = (Host)sourceListByUser._hostList[index];
                        h.recoveryOperation = 5;
                        if (h.tag != null && !h.tag.Contains("LATEST"))
                        {
                            specificTagTextBox.Text = h.tag;
                            //specificTagTextBox.Font = new Font("Microsoft Sans Serif", 8);
                        }
                        else
                        {
                            h.tag = null;
                            specificTagTextBox.Text = null;
                            // specificTagTextBox.Font = new Font("Monotype Corsiva", 8);
                        }
                        h.tagType = "FS";
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " Host does not found in the list");
                    }
                }
                else
                {
                    specificTagTextBox.Visible = false;
                    // this note label shouild display only when user selected specific tag option 
                    fileSystemTagLabel.Visible = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: applyRecoverySettingsCheckBox_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void specificTagTextBox_KeyUp(object sender, KeyEventArgs e)
        {
            if (specificTagTextBox.Text.Length != 0 && specificTagRadioButton.Checked == true)
            {
                Host h = new Host();
                h.hostname = recoverDataGridView.Rows[listIndexPrepared].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value.ToString();
                int index = 0;
                if (sourceListByUser.DoesHostExist(h, ref index) == true)
                {
                    h = (Host)sourceListByUser._hostList[index];
                    h.recoveryOperation = 5;
                    h.tag = specificTagTextBox.Text.TrimEnd();
                    h.tagType = "FS";
                    nextButton.Enabled = false;
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + " Host does not found in the list");
                }
            }
        }



        private void SetLeftTickerForTreeView(TreeNode node, bool checkedNode)
        {
            //This will be used while backgroundworker is running we can update the text box by invoking the control....   
            //if(checkedNode == false)
            //{
            //    p2VTreeViewWhenselected.Nodes[treeNodeIndex].Checked = false;
            //    //node.Checked = false;
            //}
            //else
            //{
            //    p2VTreeViewWhenselected.Nodes[treeNodeIndex].Checked = true;
            //}
        }

        private void p2vSelectBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            duplicateEntires = false;
            selectedMachineSuccessfully = false;
            SelectMachine();
        }

        private void p2vSelectBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            
            if (duplicateEntires == true)
            {
                if (duplicateListOfMachines._hostList.Count != 0)
                {
                    RemoveDuplicateEntries remove = new RemoveDuplicateEntries(duplicateListOfMachines);
                    remove.ShowDialog();
                    if (remove.canceledForm == true)
                    {
                        return;
                    }
                }
                HostList hostList = new HostList();
                if (Cxapicalls.GetAllHostFromCX(hostList, cxIPwithPortNumber, osTypeSelected) == 0)
                {
                    hostList = Cxapicalls.GetHostlist;
                    foreach (Host tempHost in hostList._hostList)
                    {
                        if (dummyHost.hostname == tempHost.hostname)
                        {
                            dummyHost.inmage_hostid = tempHost.inmage_hostid;
                            dummyHost.ip = tempHost.ip;
                            p2vSelectBackgroundWorker.RunWorkerAsync();
                            return;
                        }
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to fetch all machines info from CX ");
                }

            }
            p2VPrimaryTreeView.Enabled = true;
            progressBar.Visible = false;
            if (selectedMachineSuccessfully == false)
            {
                p2vPanel.Visible = false;
                physicalMachineDiskSizeDataGridView.Visible = false;
                hostDetailsLabel.Visible = false;
                p2VPrimaryTreeView.SelectedNode = nodeSelected;
                nodeSelected.Checked = false;
            }
            else
            {
                P2V_PrimaryServerPanelHandler p2v = (P2V_PrimaryServerPanelHandler)panelHandlerListPrepared[indexPrepared];
                p2v.DisplayAllDetails(this);
                p2VPrimaryTreeView.SelectedNode = nodeSelected;

            }

        }
       
            
    }
}
