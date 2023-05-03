//---------------------------------------------------------------
//  <copyright file="LaunchPage.xaml.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Default launch page for configuration.
//  </summary>
//
//  History:     04-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using ASRResources;
using ASRSetupFramework;

namespace UnifiedAgentConfigurator
{
    /// <summary>
    /// Interaction logic for LaunchPage.xaml
    /// </summary>
    public partial class LaunchPage : BasePageForWpfControls
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="LaunchPage"/> class.
        /// </summary>
        public LaunchPage()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LaunchPage"/> class.
        /// </summary>
        /// <param name="page">Setup framework base page.</param>
        public LaunchPage(ASRSetupFramework.Page page)
            : base(page, StringResources.registration, 1)
        {
            this.InitializeComponent();
        }
    }
}
