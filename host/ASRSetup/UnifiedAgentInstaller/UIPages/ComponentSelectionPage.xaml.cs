using ASRResources;
using ASRSetupFramework;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace UnifiedAgentInstaller
{
    /// <summary>
    /// Interaction logic for ComponentSelectionPage.xaml
    /// </summary>
    public partial class ComponentSelectionPage : BasePageForWpfControls
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="ComponentSelectionPage"/> class.
        /// </summary>
        public ComponentSelectionPage()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ComponentSelectionPage"/> class.
        /// </summary>
        /// <param name="page">Setup framework base page.</param>
        public ComponentSelectionPage(ASRSetupFramework.Page page)
            : base(page, StringResources.installation_option, 1)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Code executed when enter page is executed.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();
            this.Page.Host.SetNextButtonState(true, false);
            AnalyzeCurrentState();
        }

        /// <summary>
        /// Analyzes current system state.
        /// </summary>
        private void AnalyzeCurrentState()
        {
            Thread getOpTh = new Thread(new ThreadStart(new Action(() =>
            {
                this.stcWaitPanel.Dispatcher.Invoke(
                    new Action(() =>
                    {
                        this.stcWaitPanel.Visibility = System.Windows.Visibility.Visible;
                        this.gridMain.Visibility = System.Windows.Visibility.Hidden;
                    }),
                    System.Windows.Threading.DispatcherPriority.Render);

                SetupAction setupAction = PropertyBagDictionary.Instance.GetProperty<SetupAction>(
                    PropertyBagConstants.SetupAction);

                if (setupAction == SetupAction.InvalidOperation)
                {
                    MessageBox.Show(
                       StringResources.UpgradeUnSupportedHigherVersionText,
                       StringResources.SetupMessageBoxTitleUA,
                       MessageBoxButton.OK,
                       MessageBoxImage.Error);
                    this.Page.Host.Close();
                }
                else if (setupAction ==  SetupAction.UnSupportedUpgrade)
                {
                    MessageBox.Show(
                       StringResources.UpgradeUnSupportedText,
                       StringResources.SetupMessageBoxTitleUA,
                       MessageBoxButton.OK,
                       MessageBoxImage.Error);
                    this.Page.Host.Close();
                }

                InstallActionProcessor.SetPlatform(setupAction);

                if (setupAction == SetupAction.Upgrade || 
                    setupAction == SetupAction.CheckFilterDriver || 
                    setupAction == SetupAction.ExecutePostInstallSteps)
                {
                    this.gridMain.Dispatcher.Invoke(
                        new Action(() =>
                        {
                            PropertyBagDictionary.Instance.SafeAdd(
                                PropertyBagConstants.InstallationLocation,
                                SetupHelper.GetAgentInstalledLocation());
                            this.Page.Host.SetNextButtonState(true, true, StringResources.Upgrade);
                        }));

                    AgentInstallRole role = SetupHelper.GetAgentInstalledRole();

                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.InstallRole,
                        role);
                }

                this.stcWaitPanel.Dispatcher.Invoke(
                    new Action(() =>
                    {
                        this.stcWaitPanel.Visibility = System.Windows.Visibility.Collapsed;
                        this.gridMain.Visibility = System.Windows.Visibility.Visible;
                        this.Page.Host.SetNextButtonState(true, true);
                        this.Page.Host.SetCancelButtonState(true, true, StringResources.CancelButtonText);
                    }),
                    System.Windows.Threading.DispatcherPriority.Render);

                // If action is upgrade then we already have all the info needed. Just proceed to next step.
                if (setupAction == SetupAction.Upgrade || 
                    setupAction == SetupAction.CheckFilterDriver ||
                    setupAction == SetupAction.ExecutePostInstallSteps)
                {
                    this.stcWaitPanel.Dispatcher.Invoke(
                        new Action(() =>
                        {
                            this.Page.Host.HideSidebarStage(this.Page.SetupStage);
                            PageNavigation.Instance.MoveToNextPage();
                        }));

                    return;
                }

                // For Azure VM and V2A RCM we only can install MS. So don't ask the user auto-select and proceed.
                if (PropertyBagDictionary.Instance.GetProperty<Platform>(
                    PropertyBagConstants.Platform) == Platform.Azure || 
                    PropertyBagDictionary.Instance.GetProperty<ConfigurationServerType>(
                        PropertyBagConstants.CSType) == ConfigurationServerType.CSPrime)
                {
                    PropertyBagDictionary.Instance.SafeAdd(
                        PropertyBagConstants.InstallRole,
                        AgentInstallRole.MS);
                    this.stcWaitPanel.Dispatcher.Invoke(
                        new Action(() =>
                            {
                                this.Page.Host.HideSidebarStage(this.Page.SetupStage);
                                PageNavigation.Instance.MoveToNextPage();
                            }));

                    return;
                }
            })));

            getOpTh.Start();
        }

        /// <summary>
        /// Executed when next button is clicked.
        /// </summary>
        /// <returns>True.</returns>
        public override bool OnNext()
        {
            if (this.MSRadioButton.IsChecked.Value)
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.InstallRole,
                    AgentInstallRole.MS);
            }
            else
            {
                PropertyBagDictionary.Instance.SafeAdd(
                    PropertyBagConstants.InstallRole,
                    AgentInstallRole.MT);
            }

            return base.OnNext();
        }
    }
}
