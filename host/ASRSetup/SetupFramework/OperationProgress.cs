//---------------------------------------------------------------
//  <copyright file="OperationProgress.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  UI aware logic to allow Binding based auto UI updation and action exection.
//  </summary>
//
//  History:     04-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Windows.Data;

namespace ASRSetupFramework
{
    /// <summary>
    /// Enum representing operation status.
    /// </summary>
    public enum OperationStatus
    {
        /// <summary>
        /// Operation completed successfully.
        /// </summary>
        Completed,

        /// <summary>
        /// Operation is waiting.
        /// </summary>
        Waiting,

        /// <summary>
        /// Operation failed.
        /// </summary>
        Failed,

        /// <summary>
        /// Operation completed with warning.
        /// </summary>
        Warning,

        /// <summary>
        /// Operation was cancelled\aborted.
        /// </summary>
        Skipped,

        /// <summary>
        /// Operation is in progress.
        /// </summary>
        InProgress
    }

    /// <summary>
    /// Class representing status of operation.
    /// </summary>
    public class OperationResult
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="OperationResult"/> class.
        /// </summary>
        public OperationResult()
        {
            this.Status = OperationStatus.Completed;
            this.Error = string.Empty;
            this.ProcessExitCode = SetupHelper.UASetupReturnValues.Successful;
        }

        /// <summary>
        /// Gets or sets current status of component.
        /// </summary>
        public OperationStatus Status { get; set; }

        /// <summary>
        /// Gets or sets error occurred during the operation execution.
        /// </summary>
        public string Error { get; set; }

        /// <summary>
        /// Gets or sets the exit code for process.
        /// </summary>
        public SetupHelper.UASetupReturnValues ProcessExitCode { get; set; }
    }

    /// <summary>
    /// Class representing installation status of a component.
    /// </summary>
    public class Operation
    {
        /// <summary>
        /// The delegate used to report status.
        /// </summary>
        public delegate OperationResult PerformOperation();

        /// <summary>
        /// Gets or sets string representation of component to install.
        /// </summary>
        public string Component { get; set; }

        /// <summary>
        /// Gets or sets current status of component.
        /// </summary>
        public OperationStatus Status { get; set; }

        /// <summary>
        /// Gets or sets error if any in component installation.
        /// </summary>
        public string Error { get; set; }

        /// <summary>
        /// Gets or sets the action to perform this operation.
        /// </summary>
        public PerformOperation Action { get; set; }
    }


    /// <summary>
    /// Error button visibility convertor.
    /// </summary>
    public class ButtonShowStatusConvertor : IValueConverter
    {
        /// <summary>
        /// Converts a value.
        /// </summary>
        /// <param name="value">Value to convert.</param>
        /// <param name="targetType">Target type.</param>
        /// <param name="parameter">convertor parameter to use.</param>
        /// <param name="culture">Culture to use.</param>
        /// <returns>Converted value.</returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            Operation operation = value as Operation;

            if (operation != null)
            {
                if (operation.Status == OperationStatus.Failed || 
                    operation.Status == OperationStatus.Warning)
                {
                    return System.Windows.Visibility.Visible;
                }
                else
                {
                    return System.Windows.Visibility.Hidden;
                }
            }

            return false;
        }

        /// <summary>
        /// Converts a value back to original.
        /// </summary>
        /// <param name="value">Value to convert.</param>
        /// <param name="targetType">Target type.</param>
        /// <param name="parameter">convertor parameter to use.</param>
        /// <param name="culture">Culture to use.</param>
        /// <returns>Converted value.</returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    /// <summary>
    /// Progress bar visibility convertor.
    /// </summary>
    public class ProgressBagConvertor : IValueConverter
    {
        /// <summary>
        /// Converts a value.
        /// </summary>
        /// <param name="value">Value to convert.</param>
        /// <param name="targetType">Target type.</param>
        /// <param name="parameter">convertor parameter to use.</param>
        /// <param name="culture">Culture to use.</param>
        /// <returns>Converted value.</returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            Operation operation = value as Operation;

            if (operation != null)
            {
                if (operation.Status == OperationStatus.InProgress)
                {
                    return System.Windows.Visibility.Visible;
                }
                else
                {
                    return System.Windows.Visibility.Hidden;
                }
            }

            return false;
        }

        /// <summary>
        /// Converts a value back to original.
        /// </summary>
        /// <param name="value">Value to convert.</param>
        /// <param name="targetType">Target type.</param>
        /// <param name="parameter">convertor parameter to use.</param>
        /// <param name="culture">Culture to use.</param>
        /// <returns>Converted value.</returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    /// <summary>
    /// Image source convertor.
    /// </summary>
    public class ImageSourceConvertor : IValueConverter
    {
        /// <summary>
        /// Converts a value.
        /// </summary>
        /// <param name="value">Value to convert.</param>
        /// <param name="targetType">Target type.</param>
        /// <param name="parameter">convertor parameter to use.</param>
        /// <param name="culture">Culture to use.</param>
        /// <returns>Converted value.</returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            Operation status = value as Operation;

            if (status != null)
            {
                switch (status.Status)
                {
                    case OperationStatus.Completed:
                        return "pack://application:,,,/ASRSetupFramework;component/Images/smallGreenCheck.png";
                    case OperationStatus.Failed:
                    case OperationStatus.Skipped:
                        return "pack://application:,,,/ASRSetupFramework;component/Images/smallError.png";
                    case OperationStatus.Waiting:
                    case OperationStatus.InProgress:
                        return "pack://application:,,,/ASRSetupFramework;component/Images/infoSmallIcon.png";
                    case OperationStatus.Warning:
                        return "pack://application:,,,/ASRSetupFramework;component/Images/Warning.png";
                }
            }

            return false;
        }

        /// <summary>
        /// Converts a value back to original.
        /// </summary>
        /// <param name="value">Value to convert.</param>
        /// <param name="targetType">Target type.</param>
        /// <param name="parameter">convertor parameter to use.</param>
        /// <param name="culture">Culture to use.</param>
        /// <returns>Converted value.</returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}
