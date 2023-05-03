//---------------------------------------------------------------
//  <copyright file="LogUpload.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements service start/stop.
//  </summary>
//
//  History:     26-Sep-2017   bhayachi     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Text;
using System.Timers;
using ASRSetupFramework;

namespace Microsoft.ASRSetup.LogUploadService
{
    public partial class LogsUploadService : ServiceBase
    {
        private Timer timer;

        public LogsUploadService()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Implements Service start operation.
        /// </summary>
        protected override void OnStart(string[] args)
        {
            timer = new Timer();
            this.timer.Interval = 1800000;
            this.timer.Elapsed += new ElapsedEventHandler(this.timerElapsed);
            timer.Enabled = true;

            Trc.Log(LogLevel.Always, "Service started.");
        }

        /// <summary>
        /// Function to be implemented when time interval elapses.
        /// </summary>
        private void timerElapsed(object sender, ElapsedEventArgs e)
        {
            timer.Enabled = false;

            Operationprocessor opprocessor = new Operationprocessor();
            opprocessor.UploadLogs();
        }

        /// <summary>
        /// Implements Service stop operation.
        /// </summary>
        protected override void OnStop()
        {
            timer.Enabled = false;
            Trc.Log(LogLevel.Always, "Service stopped.");
        }
    }
}
