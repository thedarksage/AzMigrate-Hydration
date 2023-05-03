using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using ASRSetupFramework;
using ASRResources;

namespace UnifiedSetup
{
    /// <summary>
    /// Interaction logic for Summary.xaml
    /// </summary>
    public partial class Summary : BasePageForWpfControls
    {
        private string installType;

        public Summary(ASRSetupFramework.Page page)
            : base(page, StringResources.summary,3)
        {
            this.InitializeComponent();
            
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="RegistrationPage"/> class.
        /// </summary>

        public Summary()
        {
            InitializeComponent();
        }

        public override void EnterPage()
        {
            base.EnterPage();
            string install_click = PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.summarypageinstallclick);
            if (install_click == "Yes")
            {
                this.Page.Host.SetNextButtonState(true, false, StringResources.Install);
            }
            else
            {
                this.Page.Host.SetNextButtonState(true, true, StringResources.Install);
            }

            if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.CSServerMode)
            {
                installType = StringResources.CSPSinstallType;
            }
            else if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
            {
                installType = StringResources.PSinstallType;
            }

            this.SummaryTreeViewItem.Header = StringResources.summary;
            this.InstallTypeTreeViewItem.Header = StringResources.SummaryPageInstallTypeHeader;
            this.InstallTypeTreeViewChildItem.Header = installType;
            this.EnvironmentTreeViewItem.Header = StringResources.SummaryPageEnvironmentHeader;
            this.EnvironmentTreeViewChildItem.Header = SetupParameters.Instance().EnvType;
            this.InstallationDriveTreeViewItem.Header = StringResources.SummaryPageInstallationDriveHeader;
            this.InstallationDriveTreeViewChildItem.Header = SetupParameters.Instance().CXInstallDir;
            this.MainTreeView.BorderThickness = new Thickness(0); 
        }

        public override bool OnNext()
        {
            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.summarypageinstallclick, "Yes");
            return this.ValidatePage(PageNavigation.Navigation.Next);
        }

        public override void ExitPage()
        {
            base.ExitPage();
        }
    }
}
