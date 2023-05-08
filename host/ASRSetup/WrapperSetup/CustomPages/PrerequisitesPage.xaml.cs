//------------------------------------------------------------------------
//  <copyright file="PrerequisitesPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Validates all the Pre-requisites required for installation to proceed.
//  </summary>
//
//  History:     08-Oct-2015   bhayachi     Created
//-------------------------------------------------------------------------

namespace UnifiedSetup
{
    using System;
    using System.Collections.Generic;
    using System.Net;
    using System.Security;
    using System.Text.RegularExpressions;
    using System.Threading;
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Threading;
    using System.Windows.Media;
    using System.Diagnostics;
    using System.IO;
    using ASRResources;
    using ASRSetupFramework;
    using IntegrityCheck = Microsoft.DisasterRecovery.IntegrityCheck;
    using System.Text;


    /// <summary>
    /// Interaction logic for PrerequisitesPage.xaml
    /// </summary>
    public partial class PrerequisitesPage : BasePageForWpfControls
    {
        public string sysDrive = Path.GetPathRoot(Environment.SystemDirectory);

        public PrerequisitesPage(ASRSetupFramework.Page page)
            : base(page, StringResources.prerequisites_check, 2)
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="PrerequisitesPage"/> class.
        /// </summary>
        public PrerequisitesPage()
        {
            this.InitializeComponent();
        }

        #region Fields
        /// <summary>
        /// Prerequisite failed status.
        /// </summary>
        int failedStatus = 0;

        /// <summary>
        /// Prerequisite suceeded status.
        /// </summary>
        int successStatus = 0;

        /// <summary>
        /// Prerequisite skipped status.
        /// </summary>
        int warningStatus = 0;

        /// <summary>
        /// Prerequisite skipped status.
        /// </summary>
        int skipStatus = 0;

        #endregion

        #region Delegates
        /// <summary>
        /// Delegate to Validate all Pre-requisites.
        /// </summary>
        private delegate void PrerequisiteValidationDelegate();

        /// <summary>
        /// Delegate to update UI after validation finishes.
        /// </summary>
        /// <param name="validationResult">Validation result</param>
        private delegate void UpdatePrereqUIDelegate(int result, string tooltipMessage);

        /// <summary>
        /// Delegate to update buttons when all Pre-requisites succeed.
        /// </summary>
        private delegate void UpdateButtonStateDelegate();
        # endregion

        /// <summary>
        /// Enters the page.
        /// </summary>
        public override void EnterPage()
        {
            base.EnterPage();

            // Delete prerequisite status on enter to the page.
            ValidationHelper.DeleteFile(
                SetupHelper.SetLogFilePath(UnifiedSetupConstants.PrerequisiteStatusFileName));
            UpdateReRunUI();

            Trc.Log(LogLevel.Always, "Invoking TriggerValidationInNewThread.");
            this.Dispatcher.BeginInvoke(DispatcherPriority.Background, new PrerequisiteValidationDelegate(this.TriggerValidationInNewThread));
        }

        /// <summary>
        /// Override default OnNext logic.
        /// </summary>
        /// <returns>True if next page can be shown, false otherwise.</returns>
        public override bool OnNext()
        {
            return this.ValidatePage(PageNavigation.Navigation.Next);
        }

        /// <summary>
        /// Exits the page.
        /// </summary>
        public override void ExitPage()
        {
            base.ExitPage();
        }

        /// <summary>
        /// Trigger installation in a new non-UI thread.
        /// </summary>
        private void TriggerValidationInNewThread()
        {
            ThreadPool.QueueUserWorkItem(new WaitCallback(this.PrerequisiteValidation));
        }

        /// <summary>
        /// Invoke threads depending on Pre-requisite status.
        /// </summary>
        private void PrerequisiteValidation(object o)
        {
            List<EvaluateOperations.Prerequisites> fabricPrereqs = EvaluateOperations.GetPrereqs();

            bool isPrereqFailed = false;
            string tooltipMessage;

            foreach (EvaluateOperations.Prerequisites preReq in fabricPrereqs)
            {
                // 1 sec thread sleep has been added for a better UI experience.
                Thread.Sleep(1000);
                tooltipMessage = string.Empty;
                int result = (int)IntegrityCheck.OperationResult.Success;

                EvaluateOperations.UiPrereqMappings[preReq].ForEach(validation =>
                    {
                        IntegrityCheck.Response validationResponse = EvaluateOperations.EvaluatePrereq(validation);
                        IntegrityCheck.IntegrityCheckWrapper.RecordOperation(
                            IntegrityCheck.ExecutionScenario.PreInstallation,
                            EvaluateOperations.ValidationMappings[validation],
                            validationResponse.Result,
                            validationResponse.Message,
                            validationResponse.Causes,
                            validationResponse.Recommendation,
                            validationResponse.Exception
                            );

                        this.WriteToPrereqStatusFile(EvaluateOperations.OperationFriendlyNames[EvaluateOperations.ValidationMappings[validation]], validationResponse);
                        result = result | (int)validationResponse.Result;

                        if (validationResponse.Result == IntegrityCheck.OperationResult.Failed)
                        {
                            isPrereqFailed = true;
                        }

                        if (!string.IsNullOrEmpty(validationResponse.Message))
                        {
                            tooltipMessage += string.Format(
                                "{0} {1} {2} {3} {4} {5}",
                                validationResponse.Message,
                                Environment.NewLine,
                                validationResponse.Causes,
                                Environment.NewLine,
                                validationResponse.Recommendation,
                                Environment.NewLine);
                        }

                    });

                switch (preReq)
                {
                    case EvaluateOperations.Prerequisites.IsFreeSpaceAvailable:
                        this.Dispatcher.BeginInvoke(
                            DispatcherPriority.Normal,
                            new UpdatePrereqUIDelegate(this.UpdateUIForIsFreeSpaceAvailable),
                            result,
                            tooltipMessage);
                        break;
                    case EvaluateOperations.Prerequisites.IsSystemRestartPending:
                        this.Dispatcher.BeginInvoke(
                            DispatcherPriority.Normal,
                            new UpdatePrereqUIDelegate(this.UpdateUIForIsRestartPending),
                            result,
                            tooltipMessage);
                        break;
                    case EvaluateOperations.Prerequisites.IsTimeInSync:
                        this.Dispatcher.BeginInvoke(
                            DispatcherPriority.Normal,
                            new UpdatePrereqUIDelegate(this.UpdateUIForIsTimesync),
                            result,
                            tooltipMessage);
                        break;
                    case EvaluateOperations.Prerequisites.IsWinGreaterThanOrEqualTo2012R2:
                        this.Dispatcher.BeginInvoke(
                            DispatcherPriority.Normal,
                            new UpdatePrereqUIDelegate(this.UpdateUIForIsWIN2K12R2),
                            result,
                            tooltipMessage);
                        break;
                    case EvaluateOperations.Prerequisites.CheckMachineConfiguration:
                        this.Dispatcher.BeginInvoke(
                            DispatcherPriority.Normal,
                            new UpdatePrereqUIDelegate(this.UpdateUIForCheckMachineConfiguration),
                            result,
                            tooltipMessage);
                        break;
                    case EvaluateOperations.Prerequisites.IsIISNotConfigured:
                        this.Dispatcher.BeginInvoke(
                            DispatcherPriority.Normal,
                            new UpdatePrereqUIDelegate(this.UpdateUIForIsIISNotConfigured),
                            result,
                            tooltipMessage);
                            break;
                    case EvaluateOperations.Prerequisites.GroupPoliciesAbsent:
                        this.Dispatcher.BeginInvoke(
                            DispatcherPriority.Normal,
                            new UpdatePrereqUIDelegate(this.UpdateUIForGroupPoliciesAbsent),
                            result,
                            tooltipMessage);
                            break;
                    case EvaluateOperations.Prerequisites.CheckDhcpStatus:
                            this.Dispatcher.BeginInvoke(
                                DispatcherPriority.Normal,
                                new UpdatePrereqUIDelegate(this.UpdateUIForCheckDhcpStatus),
                                result,
                                tooltipMessage);
                            break;
                    case EvaluateOperations.Prerequisites.IsRightPerlVersionInstalled:
                            this.Dispatcher.BeginInvoke(
                                DispatcherPriority.Normal,
                                new UpdatePrereqUIDelegate(this.UpdateUIForIsRightPerlVersionInstalled),
                                result,
                                tooltipMessage);
                            break;
                }
            }

            this.Dispatcher.BeginInvoke(
                DispatcherPriority.Normal,
                new UpdateButtonStateDelegate(this.UpdateButtonStateandUI));

            if (isPrereqFailed)
            {
                Trc.Log(
                    LogLevel.Always,
                    "Invoking UpdateButtonStateandUI and UpdateNextButtonStateOnFailure methods when any one of the pre-requisites fail.");
                this.Dispatcher.BeginInvoke(
                    DispatcherPriority.Normal,
                    new UpdateButtonStateDelegate(this.UpdateNextButtonStateOnFailure));
            }
            else
            {
                // Clean up DRA related registries.
                InstallActionProcessess.CallPurgeDRARegistryMethod();

                Trc.Log(
                    LogLevel.Always,
                    "Invoking UpdateButtonStateandUI and UpdateNextButtonStateOnSuccess methods when all the pre-requisites succeed.");
                this.Dispatcher.BeginInvoke(
                    DispatcherPriority.Normal,
                    new UpdateButtonStateDelegate(this.UpdateNextButtonStateOnSuccess));

                // Enable global navigation
                this.Page.Host.EnableGlobalSideBarNavigation();
            }
        }

        /// <summary>
        /// Update UI status based on the validation result
        /// </summary>
        /// <param name="validationResponse">Validation Response</param>
        private void UpdateUIForIsRestartPending(int result, string tooltipMessage)
        {
            this.PrereqRestartPendingStatus.Visibility = System.Windows.Visibility.Collapsed;

            if (result == 0)
            {
                successStatus += 1;

                this.PrereqRestartSuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqRestartPassedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqRestartPassedStatus.ToolTip = tooltipMessage;
            }
            else
            {
                failedStatus += 1;

                this.PrereqRestartErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqRestartFailedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqRestartFailedStatus.ToolTip = tooltipMessage;
            }

            this.PrereqCurrentOperationTextBlock.Text = string.Format(
                StringResources.PrereqCurrentOperationText,
                successStatus,
                failedStatus,
                warningStatus,
                skipStatus);
        }

        /// <summary>
        /// Update UI status based on the validation result
        /// </summary>
        /// <param name="validationResponse">Validation Response</param>
        private void UpdateUIForCheckDhcpStatus(int result, string tooltipMessage)
        {
            this.PrereqNetworkInterfacePendingStatus.Visibility = System.Windows.Visibility.Collapsed;

            if (result == 0)
            {
                successStatus += 1;

                this.PrereqNetworkInterfaceSuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqNetworkInterfacePassedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqNetworkInterfacePassedStatus.ToolTip = tooltipMessage;
            }
            else
            {
                warningStatus += 1;

                this.PrereqNetworkInterfaceWarningImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqNetworkInterfaceWarningStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqNetworkInterfaceWarningStatus.ToolTip = tooltipMessage;
            }

            this.PrereqCurrentOperationTextBlock.Text = string.Format(
                StringResources.PrereqCurrentOperationText,
                successStatus,
                failedStatus,
                warningStatus,
                skipStatus);
        }

        /// <summary>
        /// Update UI status based on the validation result
        /// </summary>
        /// <param name="validationResponse">Validation Response</param>
        private void UpdateUIForIsRightPerlVersionInstalled(int result, string tooltipMessage)
        {
            this.PrereqPerlCheckPendingStatus.Visibility = System.Windows.Visibility.Collapsed;

            if (result == 0)
            {
                successStatus += 1;

                this.PrereqPerlCheckSuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqPerlCheckPassedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqPerlCheckPassedStatus.ToolTip = tooltipMessage;
            }
            else
            {
                failedStatus += 1;

                this.PrereqPerlCheckErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqPerlCheckFailedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqPerlCheckFailedStatus.ToolTip = tooltipMessage;
            }

            this.PrereqCurrentOperationTextBlock.Text = string.Format(
                StringResources.PrereqCurrentOperationText,
                successStatus,
                failedStatus,
                warningStatus,
                skipStatus);
        }

        /// <summary>
        /// Update UI status based on the validation result
        /// </summary>
        /// <param name="validationResponse">Validation Response</param>
        private void UpdateUIForCheckMachineConfiguration(int result, string tooltipMessage)
        {
            this.PrereqMachineConfigPendingStatus.Visibility = System.Windows.Visibility.Collapsed;

            if (result == 0)
            {
                successStatus += 1;

                this.PrereqMachineConfigSuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqMachineConfigPassedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqMachineConfigPassedStatus.ToolTip = tooltipMessage;
            }
            else if (result == 2)
            {
                warningStatus += 1;

                this.PrereqMachineConfigWarningImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqMachineConfigWarningStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqMachineConfigWarningStatus.ToolTip = tooltipMessage;
            }
            else
            {
                failedStatus += 1;

                this.PrereqMachineConfigErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqMachineConfigFailedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqMachineConfigFailedStatus.ToolTip = tooltipMessage;
            }

            this.PrereqCurrentOperationTextBlock.Text = string.Format(
                StringResources.PrereqCurrentOperationText,
                successStatus,
                failedStatus,
                warningStatus,
                skipStatus);
        }

        /// <summary>
        /// Update UI status based on the validation result
        /// </summary>
        /// <param name="validationResponse">Validation Response</param>
        private void UpdateUIForIsIISNotConfigured(int result, string tooltipMessage)
        {
            this.PrereqIISPendingStatus.Visibility = System.Windows.Visibility.Collapsed;

            if (result == 0)
            {
                successStatus += 1;

                this.PrereqIISSuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqIISPassedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqIISPassedStatus.ToolTip = tooltipMessage;
            }
            else if (result == 3)
            {
                skipStatus += 1;

                this.PrereqIISWarningImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqIISSkipStatus.Visibility = System.Windows.Visibility.Visible;
                if (SetupParameters.Instance().ServerMode == UnifiedSetupConstants.PSServerMode)
                {
                    this.PrereqIISSkipStatus.ToolTip = ASRResources.StringResources.SkipIISConfigCheckText;
                }
                else
                {
                    this.PrereqIISSkipStatus.ToolTip = tooltipMessage;
                }
            }
            else
            {
                failedStatus += 1;

                this.PrereqIISErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqIISFailedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqIISFailedStatus.ToolTip = tooltipMessage;
            }

            this.PrereqCurrentOperationTextBlock.Text = string.Format(
                StringResources.PrereqCurrentOperationText,
                successStatus,
                failedStatus,
                warningStatus,
                skipStatus);
        }

        /// <summary>
        /// Update UI status based on the validation result
        /// </summary>
        /// <param name="validationResponse">Validation Response</param>
        private void UpdateUIForGroupPoliciesAbsent(int result, string tooltipMessage)
        {
            this.PrereqGroupPolicyPendingStatus.Visibility = System.Windows.Visibility.Collapsed;

            if (result == 0)
            {
                successStatus += 1;

                this.PrereqGroupPolicySuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqGroupPolicyPassedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqGroupPolicyPassedStatus.ToolTip = tooltipMessage;
            }
            else
            {
                failedStatus += 1;

                this.PrereqGroupPolicyErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqGroupPolicyFailedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqGroupPolicyFailedStatus.ToolTip = tooltipMessage;
            }

            this.PrereqCurrentOperationTextBlock.Text = string.Format(
                StringResources.PrereqCurrentOperationText,
                successStatus,
                failedStatus,
                warningStatus,
                skipStatus);
        }

        /// <summary>
        /// Update UI status based on the validation result
        /// </summary>
        /// <param name="validationResponse">Validation Response</param>
        private void UpdateUIForIsTimesync(int result, string tooltipMessage)
        {
            this.PrereqTimeSyncPendingStatus.Visibility = System.Windows.Visibility.Collapsed;

            // Global Time Sync Validation.
            if (result == 0)
            {
                successStatus += 1;

                this.PrereqTimeSyncSuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqTimeSyncPassedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqTimeSyncPassedStatus.ToolTip = tooltipMessage;
            }
            else if (result == 2)
            {
                warningStatus += 1;

                this.PrereqTimeSyncWarningImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqTimeSyncSkipStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqTimeSyncSkipStatus.Text = StringResources.WarningStatusText;
                this.PrereqTimeSyncSkipStatus.ToolTip = tooltipMessage;
            }
            else
            {
                failedStatus += 1;

                this.PrereqTimeSyncErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqTimeSyncFailedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqTimeSyncFailedStatus.ToolTip = tooltipMessage;
            }

            this.PrereqCurrentOperationTextBlock.Text = string.Format(
                StringResources.PrereqCurrentOperationText,
                successStatus,
                failedStatus,
                warningStatus,
                skipStatus);
        }

        /// <summary>
        /// Update UI status based on the validation result
        /// </summary>
        /// <param name="validationResponse">Validation Response</param>
        private void UpdateUIForIsFreeSpaceAvailable(int result, string tooltipMessage)
        {
            this.PrereqSpaceReqPendingStatus.Visibility = System.Windows.Visibility.Collapsed;

            // Global Time Sync Validation.
            if (result == 0)
            {
                successStatus += 1;

                this.PrereqSpaceReqSuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqSpaceReqPassedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqSpaceReqPassedStatus.ToolTip = tooltipMessage;
            }
            else if (result == 2)
            {
                warningStatus += 1;

                this.PrereqSpaceReqWarningImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqSpaceReqWarningStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqSpaceReqWarningStatus.ToolTip = tooltipMessage;
            }
            else
            {
                failedStatus += 1;

                this.PrereqSpaceReqErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqSpaceReqFailedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqSpaceReqFailedStatus.ToolTip = tooltipMessage;
            }

            this.PrereqCurrentOperationTextBlock.Text = string.Format(
                StringResources.PrereqCurrentOperationText,
                successStatus,
                failedStatus,
                warningStatus,
                skipStatus);
        }

        /// <summary>
        /// Update UI status based on the validation result
        /// </summary>
        /// <param name="validationResponse">Validation Response</param>
        private void UpdateUIForIsWIN2K12R2(int result, string tooltipMessage)
        {
            this.PrereqW2K12R2PendingStatus.Visibility = System.Windows.Visibility.Collapsed;

            if (result == 0)
            {
                successStatus += 1;

                this.PrereqW2K12R2SuccessImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqW2K12R2PassedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqW2K12R2PassedStatus.ToolTip = tooltipMessage;
            }
            else
            {
                failedStatus += 1;

                this.PrereqW2K12R2ErrorImage.Visibility = System.Windows.Visibility.Visible;
                this.PrereqW2K12R2FailedStatus.Visibility = System.Windows.Visibility.Visible;
                this.PrereqW2K12R2FailedStatus.ToolTip = tooltipMessage;
            }

            this.PrereqCurrentOperationTextBlock.Text = string.Format(
                StringResources.PrereqCurrentOperationText,
                successStatus,
                failedStatus,
                warningStatus,
                skipStatus);
        }

        /// <summary>
        /// Update buttons and UI at the end of Pre-requisites validaion.
        /// </summary>
        private void UpdateButtonStateandUI()
        {
            this.PrereqShowProgress.Visibility = System.Windows.Visibility.Hidden;
            this.Page.Host.SetPreviousButtonState(true, true);
            this.Page.Host.SetCancelButtonState(true, true);
            this.Rerun.IsEnabled = true;

            successStatus = 0;
            failedStatus = 0;
            warningStatus = 0;
            skipStatus = 0;
        }

        /// <summary>
        /// Update next button when all the Pre-requisites succeed.
        /// </summary>
        private void UpdateNextButtonStateOnSuccess()
        {
            this.Page.Host.SetNextButtonState(true, true);
        }

        /// <summary>
        /// Update next button when any of the Pre-requisites fail.
        /// </summary>
        private void UpdateNextButtonStateOnFailure()
        {
            this.Page.Host.SetNextButtonState(true, false);
        }

        private void UpdateReRunUI()
        {
            // Disable global navigation
            this.Page.Host.DisableGlobalSideBarNavigation();

            this.PrereqCurrentOperationTextBlock.Text = string.Format(StringResources.PrereqCurrentOperationText, successStatus, failedStatus, warningStatus, skipStatus);
            this.Rerun.IsEnabled = false;
            this.PrereqCurrentOperationTextBlock.Visibility = System.Windows.Visibility.Visible;
            this.PrereqShowProgress.Visibility = System.Windows.Visibility.Visible;

            this.PrereqMachineConfigPendingStatus.Visibility = System.Windows.Visibility.Visible;
            this.PrereqMachineConfigPassedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqMachineConfigWarningStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqMachineConfigFailedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqMachineConfigSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqMachineConfigWarningImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqMachineConfigErrorImage.Visibility = System.Windows.Visibility.Collapsed;

            this.PrereqRestartPendingStatus.Visibility = System.Windows.Visibility.Visible;
            this.PrereqRestartPassedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqRestartFailedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqIISSkipStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqRestartSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqRestartErrorImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqIISWarningImage.Visibility = System.Windows.Visibility.Collapsed;

            this.PrereqIISPendingStatus.Visibility = System.Windows.Visibility.Visible;
            this.PrereqIISPassedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqIISFailedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqIISSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqIISErrorImage.Visibility = System.Windows.Visibility.Collapsed;

            this.PrereqGroupPolicyPendingStatus.Visibility = System.Windows.Visibility.Visible;
            this.PrereqGroupPolicyPassedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqGroupPolicyFailedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqGroupPolicySuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqGroupPolicyErrorImage.Visibility = System.Windows.Visibility.Collapsed;

            this.PrereqW2K12R2PendingStatus.Visibility = System.Windows.Visibility.Visible;
            this.PrereqW2K12R2PassedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqW2K12R2FailedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqW2K12R2SuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqW2K12R2ErrorImage.Visibility = System.Windows.Visibility.Collapsed;

            this.PrereqTimeSyncPendingStatus.Visibility = System.Windows.Visibility.Visible;
            this.PrereqTimeSyncPassedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqTimeSyncSkipStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqTimeSyncFailedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqTimeSyncSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqTimeSyncWarningImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqTimeSyncErrorImage.Visibility = System.Windows.Visibility.Collapsed;

            this.PrereqSpaceReqPendingStatus.Visibility = System.Windows.Visibility.Visible;
            this.PrereqSpaceReqPassedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqSpaceReqFailedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqSpaceReqSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqSpaceReqWarningStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqSpaceReqErrorImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqSpaceReqWarningImage.Visibility = System.Windows.Visibility.Collapsed;

            this.PrereqNetworkInterfacePendingStatus.Visibility = System.Windows.Visibility.Visible;
            this.PrereqNetworkInterfacePassedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqNetworkInterfaceWarningStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqNetworkInterfaceSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqNetworkInterfaceWarningImage.Visibility = System.Windows.Visibility.Collapsed;

            this.PrereqPerlCheckPendingStatus.Visibility = System.Windows.Visibility.Visible;
            this.PrereqPerlCheckPassedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqPerlCheckFailedStatus.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqPerlCheckSuccessImage.Visibility = System.Windows.Visibility.Collapsed;
            this.PrereqPerlCheckErrorImage.Visibility = System.Windows.Visibility.Collapsed;

            // Disable Previous and Next buttons
            this.Page.Host.SetPreviousButtonState(true, false);
            this.Page.Host.SetNextButtonState(true, false);
            this.Page.Host.SetCancelButtonState(true, true);
        }

        private void Rerun_Click(object sender, RoutedEventArgs e)
        {
            Trc.Log(LogLevel.Always, "Rerun button is clicked. Validating prerequisites again.");

            // Delete prerequisite status file on re-run.
            ValidationHelper.DeleteFile(
                SetupHelper.SetLogFilePath(UnifiedSetupConstants.PrerequisiteStatusFileName));

            UpdateReRunUI();
            this.Dispatcher.BeginInvoke(DispatcherPriority.Background, new PrerequisiteValidationDelegate(this.TriggerValidationInNewThread));
        }

        private void PrereqStatus_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            try
            {
                string path = sysDrive + @"ProgramData\ASRSetupLogs\PrereqStatusFile.txt";
                if (File.Exists(path))
                {
                    Process.Start(new ProcessStartInfo(path));
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Unable to find" + path);
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Exception occurred: ", ex);
            }
        }

        /// <summary>
        /// Write prerequisite response to user display file.
        /// </summary>
        /// <param name="prereqName">Prerequisite name.</param>
        /// <param name="prereqResponse">Prerequisite response.</param>
        private void WriteToPrereqStatusFile(string prereqName, IntegrityCheck.Response prereqResponse)
        {
            try
            {
                string filepath = SetupHelper.SetLogFilePath(UnifiedSetupConstants.PrerequisiteStatusFileName);
                List<string> lines = new List<string>();

                lines.Add(string.Format("[{0}] = {1}", StringResources.PrereqHeadingPrereqText, prereqName));
                lines.Add(string.Format("{0} = {1}", StringResources.Status, prereqResponse.Result));

                if (!string.IsNullOrEmpty(prereqResponse.Message))
                {
                    lines.Add(string.Format("{0} = {1}", StringResources.Information, prereqResponse.Message));
                }

                if (!string.IsNullOrEmpty(prereqResponse.Causes))
                {
                    lines.Add(string.Format("{0} = {1}", StringResources.Causes, prereqResponse.Causes));
                }

                if (!string.IsNullOrEmpty(prereqResponse.Recommendation))
                {
                    lines.Add(string.Format("{0} = {1}", StringResources.Recommendation, prereqResponse.Recommendation));
                }

                lines.Add(Environment.NewLine);
                Trc.Log(LogLevel.Always, "Writing {0} status information to the log.", prereqName);

                if (File.Exists(filepath))
                {
                    File.AppendAllLines(filepath, lines, Encoding.UTF8);
                }
                else
                {
                    File.WriteAllLines(filepath, lines, Encoding.UTF8);
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "Write to prerequisite status file failed", ex);
            }
        }
    }
}
