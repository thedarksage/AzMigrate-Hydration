namespace ASRSetupFramework
{
    using System;
    
    /// <summary>
    /// WPF factory class
    /// </summary>
    public class WpfFactory : Factory
    {
        /// <summary>
        /// Create a new host page.
        /// </summary>
        /// <returns>New host page</returns>
        public override IPageHost CreateHost()
        {
            return new WpfFormsHostPage();
        }

        /// <summary>
        /// Creates a new UI page.
        /// </summary>
        /// <param name="page">Page to be loaded.</param>
        /// <param name="type">Type of page.</param>
        /// <returns>IPageUI object.</returns>
        public override IPageUI CreatePage(Page page, Type type)
        {
            if (type == null)
            {
                throw new ArgumentNullException("type");
            }

            if (page == null)
            {
                throw new ArgumentNullException("page");
            }

            return (IPageUI)Activator.CreateInstance(type, page);
        }
    }
}
