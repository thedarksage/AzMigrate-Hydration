namespace ASRSetupFramework
{
    using System;
    using System.Diagnostics.CodeAnalysis;
    using System.Windows.Controls;
    using ASRResources;
    using WpfResources;

    /// <summary>
    /// Base class for all wizard custom pages 
    /// A BasePageForWpfControls is a User Control, compliant to IPageUI 
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.OrderingRules",
        "SA1201:ElementsMustAppearInTheCorrectOrder",
        Justification = "Methods and properties sorted on basis of implemented interface.")]
    public partial class BasePageForWpfControls : UserControl, IPageUI
    {
        /// <summary>
        /// Backing store for the corresponding IPageUI property 
        /// </summary>
        private readonly Page page;

        /// <summary>
        /// Backing store for the corresponding IPageUI property 
        /// </summary>
        private readonly double progressPhase;

        /// <summary>
        /// Backing store for the corresponding IPageUI property 
        /// </summary>
        private string pageTitle;

        /// <summary>
        /// Backing store for the corresponding property 
        /// </summary>
        private bool isBasePageInitialized;

        #region Constructors
        /// <summary>
        /// Initializes a new instance of the BasePageForWpfControls class.
        /// </summary>
        /// <param name="page">The page to be loaded.</param>
        /// <param name="title">The page title.</param>
        /// <param name="progressPhase">The progress phase.</param>
        public BasePageForWpfControls(Page page, string title, double progressPhase)
        {
            this.page = page;
            this.pageTitle = title;
            this.progressPhase = progressPhase;
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="BasePageForWpfControls"/> class.
        /// </summary>
        public BasePageForWpfControls()
        {
        }
        #endregion

        /// <summary>
        /// Gets the underlying Page instance 
        /// </summary>
        public ASRSetupFramework.Page Page
        {
            get { return this.page; }
        }
        
        /// <summary>
        /// Writes the property bag. 
        /// The default implementation does nothing. 
        /// </summary>
        /// <param name="propertyBag">The property bag.</param>
        public virtual void WritePropertyBag(PropertyBagDictionary propertyBag)
        {
        }

        #region IPageUI Members

        /// <summary>
        /// Validates the page.
        /// The default implementation always returns true for 'validated'. 
        /// </summary>
        /// <param name="navType">Navigation type</param>
        /// <param name="pageId">Page being navigated to.</param>
        /// <returns>true if valid</returns>
        public virtual bool ValidatePage(PageNavigation.Navigation navType, string pageId = null)
        {
            return true;
        }

        /// <summary>
        /// Initializes the page.
        /// The default implementation always mark the page as initialized. 
        /// </summary>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Security", "CA2119:SealMethodsThatSatisfyPrivateInterfaces", Justification = "Invalid FXCOP error. Interface not private"), STAThread]
        public virtual void InitializePage()
        {
            this.isBasePageInitialized = true;
        }

        /// <summary>
        /// Gets or sets a value indicating whether this instance is initialized.
        /// </summary>
        /// <value>
        ///     <c>true</c> if this instance is initialized; otherwise, <c>false</c>.
        /// </value>
        public bool IsBasePageInitialized
        {
            get { return this.isBasePageInitialized; }
            protected set { this.isBasePageInitialized = value; }
        }

        /// <summary>
        /// Gets the page's title.
        /// </summary>
        /// <value>The title.</value>
        public string Title
        {
            get { return this.pageTitle; }
        }

        /// <summary>
        /// Gets the progress phase of the current page
        /// </summary>
        public double ProgressPhase
        {
            get { return this.progressPhase; }
        }

        /// <summary>
        /// Enters the page.
        /// </summary>
        public virtual void EnterPage()
        {
            Trc.Log(LogLevel.Info, string.Format("Enter {0}", this.ToString()));
            
            // Mark this page as active on sidebar.
            this.page.Host.ActivateSetupStage(this.page.SetupStage);

            // Need to get to the host page so we can access it's public functions
            IPageHost host = this.page.Host;    
    
            // Set the previous page button to false if there is no previous button
            host.SetPreviousButtonState(true, this.page.PreviousPageArgument != null);

            // Set the next page button to "Finish" if there is no next page
            host.SetNextButtonState(
                true,
                true, 
                this.page.NextPageArgument == null ?
                    StringResources.FinishButtonText : 

                    // Set to 'Next' when there are other pages to move to.
                    StringResources.NextButtonText);

            // Reset the cancel button to enabled
            host.SetCancelButtonState(true, true);
        }

        /// <summary>
        /// Exits the page.
        /// The default implementation writes the property bag. 
        /// </summary>
        public virtual void ExitPage()
        {
            Trc.Log(LogLevel.Info, string.Format("Exit {0}", this.ToString()));
            this.page.Host.EnableNavigation(this.page.SetupStage);
            this.page.Host.DeactivateSetupStage(this.page.SetupStage);
            this.WritePropertyBag(PropertyBagDictionary.Instance);
        }

        /// <summary>
        /// Disable clickable navigation for page on sidebar.
        /// </summary>
        public virtual void DisableSidebarNavigationForPage()
        {
            this.page.Host.DisableNavigation(this.page.SetupStage);
        }

        /// <summary>
        /// Marks current page as operation completed.
        /// </summary>
        public virtual void MarkThisStageAsCompleted()
        {
            this.page.Host.MarkStageAsCompleted(this.page.SetupStage);
        }

        /// <summary>
        /// Hide\Disable the stage passed as an argument.
        /// </summary>
        public virtual void HideSidebarStageforPage(string stage)
        {
            this.page.Host.HideSidebarStage(stage);
        }

        /// <summary>
        /// Show\Enable the stage passed as an argument.
        /// </summary>
        public virtual void ShowSidebarStageforPage(string stage)
        {
            this.page.Host.ShowSidebarStage(stage);
        }
        #endregion

        #region Other virtuals

        /// <summary>
        /// Notifies a base page that the user has clicked 'Previous' in the host page area 
        /// The default implementation always returns true. 
        /// Override to provide custom functionality when Previous button is pressed.
        /// </summary>
        /// <returns>True if it is possible to proceed with previous, false otherwise</returns>
        public virtual bool OnPrevious()
        {
            return true;
        }

        /// <summary>
        /// Notifies a base page that the user has clicked 'Next' in the host page area. 
        /// The default implementation always returns true. 
        /// Override to provide custom functionality when Next button is pressed.
        /// </summary>
        /// <returns>True if it is possible to proceed with next, false otherwise</returns>
        public virtual bool OnNext()
        {
            return true;
        }

        /// <summary>
        /// Notifies a base page that the user has clicked 'Cancel' in the host page area 
        /// The default implementation always returns true. 
        /// Override to provide custom functionality when Cancel button is pressed.
        /// </summary>
        /// <returns>True if it is possible to proceed with cancel, false otherwise</returns>
        public virtual bool OnCancel()
        {
            return true; 
        }

        /// <summary>
        /// Notifies a base page that the user has clicked 'Close' or Alt-F4 in the main window
        /// The default implementation always returns true. 
        /// Override to provide custom functionality when the main window is closed.
        /// </summary>
        /// <returns>True if it is possible to proceed with closing, false otherwise</returns>
        public virtual bool CanClose()
        {
            return true; 
        }
        #endregion // Other protected virtuals
    }
}
