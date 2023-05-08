using System.Globalization;
using System.Resources;

namespace ASRResources
{
    /// <summary>
    /// Class to access Resources from Resx files.
    /// </summary>
    public class RawResources
    {
        /// <summary>
        /// Resx file accessor.
        /// </summary>
        private static ResourceManager resources;

        /// <summary>
        /// Initializes static members of the RawResources class.
        /// </summary>
        static RawResources()
        {
            if (resources == null)
            {
                resources = new ResourceManager("ASRResources.Resources", typeof(StringResources).Assembly);
            }
        }

        /// <summary>
        /// Returns page tag value from Resource dictionary.
        /// </summary>
        /// <typeparam name="T">Generic datatype.</typeparam>
        /// <param name="tag">Tag to retreive for.</param>
        /// <returns>Object from Resource dictionary.</returns>
        public static T GetPageResource<T>(string tag)
        {
            try
            {
                return (T)resources.GetObject(tag, System.Globalization.CultureInfo.CurrentUICulture);
            }
            catch
            {
                return (T)resources.GetObject(tag, new CultureInfo("en-US"));
            }
        }
    }
}
