using System.Diagnostics.CodeAnalysis;
using System.Windows;
using ASRSetupFramework;

namespace WpfResources
{
    /// <summary>
    /// WPFResourceDictionary accessor.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.DocumentationRules",
        "SA1600:ElementsMustBeDocumented",
        Justification = "Resource dictionary references")]
    public class WPFResourceDictionary
    {
        private static ResourceDictionary resources;

        static WPFResourceDictionary()
        {
            try
            {
                resources = ResourcesHelper.LoadInternal("WPFResourceDictionary.xaml");
            }
            catch
            {
                resources = new ResourceDictionary();
            }
        }

        #region Styles
        public static Style SetupStageButton
        {
            get
            {
                return (Style)resources["SetupStageButton"];
            }
        }

        public static Style ButtonHyperLinkStyle
        {
            get
            {
                return (Style)resources["HyperLinkButton"];
            }
        }
        #endregion
    }
}
