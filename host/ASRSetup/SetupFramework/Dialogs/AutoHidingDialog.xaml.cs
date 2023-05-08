//---------------------------------------------------------------
//  <copyright file="AutoHidingDialog.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Interaction logic for a dialog which hides when user clicks outside it.
//  </summary>
//
//  History:     03-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace ASRSetupFramework
{
    /// <summary>
    /// Interaction logic for AutoHidingDialog.xaml
    /// </summary>
    public partial class AutoHidingDialog : Window
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="AutoHidingDialog"/> class.
        /// </summary>
        /// <param name="Y">Position of dialog on Y axis</param>
        /// <param name="X">Position of dialog on X axis.</param>
        /// <param name="data">Text to show on dialog.</param>
        public AutoHidingDialog(int Y, int X, string data)
        {
            InitializeComponent();
            dialogWindow.Left = X;
            dialogWindow.Top = Y - dialogWindow.Height;
            this.txtData.Document.Blocks.Add(new Paragraph(new Run(data)));
        }

        /// <summary>
        /// Raises the System.Windows.Window.Deactivated event.
        /// </summary>
        /// <param name="e">An System.EventArgs that contains the event data.</param>
        protected override void OnDeactivated(EventArgs e)
        {
            base.OnDeactivated(e);
            this.Close();
        }
    }
}
