//---------------------------------------------------------------
//  <copyright file="App.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Page to be used an application UI entry point.
//  </summary>
//
//  History:     03-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using ASRSetupFramework;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Windows;
using System.Windows.Threading;

namespace UnifiedAgentInstaller
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        /// <summary>
        /// Overrides default on startup behavior.
        /// </summary>
        /// <param name="e">Startup arguments.</param>
        protected override void OnStartup(StartupEventArgs e)
        {
            // define application exception handler
            Application.Current.DispatcherUnhandledException += new
               DispatcherUnhandledExceptionEventHandler(
                  this.AppDispatcherUnhandledException);

            // defer other startup processing to base class
            base.OnStartup(e);
        }

        /// <summary>
        /// Handles all exception which are not handled at global level.
        /// </summary>
        /// <param name="sender">Event sender.</param>
        /// <param name="e">Event arguments.</param>
        private void AppDispatcherUnhandledException(
            object sender,
            DispatcherUnhandledExceptionEventArgs e)
        {
            
        }
    }
}
