//-----------------------------------------------------------------------
// <copyright file="Page.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary> Class to describe a page
// </summary>
//-----------------------------------------------------------------------
namespace ASRSetupFramework
{
    using System;

    /// <summary>
    /// The Page class is defined so that common implementations 
    /// can be used in different flow scenario, such as Server, UI, etc. 
    /// </summary>
    public class Page
    {
        /// <summary>
        /// Unique page ID.
        /// </summary>
        private string pageId;

        /// <summary>
        /// Logic to progress to next page.
        /// </summary>
        private string nextPageDelegateId;

        /// <summary>
        /// Next page argument.
        /// </summary>
        private string nextPageArgument;

        /// <summary>
        /// Previous page delegate.
        /// </summary>
        private string previousPageDelegateId;

        /// <summary>
        /// Setup stage to be shown on sidebar.
        /// </summary>
        private string setupStage;

        /// <summary>
        /// Previous page argument.
        /// </summary>
        private string previousPageArgument;

        /// <summary>
        /// Help page guid.
        /// </summary>
        private Guid helpGuid;

        /// <summary>
        /// Base page instance.
        /// </summary>
        private IPageHost host;

        /// <summary>
        /// Page UI object instance.
        /// </summary>
        private IPageUI pageUI;

        /// <summary>
        /// Initializes a new instance of the <see cref="Page"/> class.
        /// </summary>
        /// <param name="factory">The factory object.</param>
        /// <param name="host">The host page.</param>
        /// <param name="page">The current UI page.</param>
        public Page(Factory factory, IPageHost host, PagesPage page)
        {
            if (factory == null)
            {
                throw new ArgumentNullException(factory.ToString());
            }

            if (host == null)
            {
                throw new ArgumentException(host.ToString());
            }

            if (page == null)
            {
                throw new ArgumentException(page.ToString());
            }

            this.host = host;
            this.pageId = page.Id;
            this.nextPageDelegateId = page.NextPage.delegateId;
            this.nextPageArgument = page.NextPage.Value;
            this.previousPageDelegateId = page.PreviousPage.delegateId;
            this.previousPageArgument = page.PreviousPage.Value;
            this.helpGuid = new Guid(page.HelpPage.Value);
            this.setupStage = page.SetupStage.Value;

            Type controlType = System.Reflection.Assembly.GetCallingAssembly().GetType(page.Implementation);
            this.pageUI = factory.CreatePage(this, controlType);
        }

        /// <summary>
        /// Gets or sets the Progress Phase
        /// </summary>
        public double ProgressPhase
        {
            get;
            set;
        }

        /// <summary>
        /// Gets the help GUID.
        /// </summary>
        /// <value>The help GUID.</value>
        public Guid HelpGuid
        {
            get
            {
                return this.helpGuid;
            }
        }

        /// <summary>
        /// Gets the SetupStage.
        /// </summary>
        public string SetupStage
        {
            get
            {
                return this.setupStage;
            }
        }

        /// <summary>
        /// Gets the page ID.
        /// </summary>
        /// <value>The unique id.</value>
        public string Id
        {
            get
            {
                return this.pageId;
            }
        }

        /// <summary>
        /// Gets the host.
        /// </summary>
        /// <value>The host object.</value>
        public IPageHost Host
        {
            get { return this.host; }
        }

        /// <summary>
        /// Gets the page pageUI.
        /// </summary>
        /// <value>The pageUI object.</value>
        public IPageUI PageUI
        {
            get { return this.pageUI; }
        }

        /// <summary>
        /// Gets the next page delegate id.
        /// </summary>
        /// <value>The next page delegate id.</value>
        public string NextPageDelegateId
        {
            get { return this.nextPageDelegateId; }
        }

        /// <summary>
        /// Gets the next page argument.
        /// </summary>
        /// <value>The next page argument.</value>
        public string NextPageArgument
        {
            get { return this.nextPageArgument; }
        }

        /// <summary>
        /// Gets the previous page delegate id.
        /// </summary>
        /// <value>The previous page delegate id.</value>
        public string PreviousPageDelegateId
        {
            get { return this.previousPageDelegateId; }
        }

        /// <summary>
        /// Gets the previous page argument.
        /// </summary>
        /// <value>The previous page argument.</value>
        public string PreviousPageArgument
        {
            get { return this.previousPageArgument; }
        }
    }
}
