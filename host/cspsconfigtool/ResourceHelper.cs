using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Globalization;
using System.Resources;

namespace cspsconfigtool
{
    public class ResourceHelper
    {        
        static ResourceManager resourceManager;

        public static CultureInfo DefaultCulture { private set; get; }
        public static CultureInfo CurrentCulture { private set; get; }

        static ResourceHelper()
        {
            DefaultCulture = new CultureInfo("en-US");
            CurrentCulture = DefaultCulture;
            resourceManager = new ResourceManager("cspsconfigtool.Lang.Res", typeof(ResourceHelper).Assembly);
        }

        public static void SetLanguage(string language)
        {
            if (language == "English")
            {
                CurrentCulture = new CultureInfo("en-US");
            }
            else
            {
                throw new Exception("Language Not Supported");
            }
        }

        public static string GetStringResourceValue(string resourceName)
        {
            return resourceManager.GetString(resourceName, CurrentCulture);
        }

        public static string Netfailure_GenericFailure
        {
            get
            {
               
                return resourceManager.GetString("Netfailure_GenericFailure", CurrentCulture);

            }
        }

        public static string Netfailure_NameResolutionFailure
        {
            get
            {
                return resourceManager.GetString("Netfailure_NameResolutionFailure", CurrentCulture);
            }
        }

        public static string Netfailure_ProxyAuthenticationRequired
        {
            get
            {
                return resourceManager.GetString("Netfailure_ProxyAuthenticationRequired", CurrentCulture);
            }
        }

        public static string Netfailure_ProxyNameResolutionFailure
        {
            get
            {
                return resourceManager.GetString("Netfailure_ProxyNameResolutionFailure", CurrentCulture);
            }
        }

        public static string Netfailure_RequestProhibitedByProxy
        {
            get
            {
                return resourceManager.GetString("Netfailure_RequestProhibitedByProxy", CurrentCulture);
            }
        }

        public static string InvalidProxySettings
        {
            get
            {
                return resourceManager.GetString("InvalidProxySettings", CurrentCulture);
            }
        }
    }
}
