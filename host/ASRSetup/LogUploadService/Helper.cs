using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace Microsoft.ASRSetup.LogUploadService
{
    /// <summary>
    /// Json properties. 
    /// </summary>
    public class JsonProperties
    {
        /// <summary>
        /// Gets or sets the last successful upload time.
        /// </summary>
        public string LastUploadTime { get; set; }
    }

    /// <summary>
    /// Json helper.
    /// </summary>
    public sealed class JsonHelper
    {
        /// <summary>
        /// Instance of Json helper.
        /// </summary>
        private static JsonHelper instance;

        /// <summary>
        /// Lock object.
        /// </summary>
        private static object objLock = new object();

        /// <summary>
        /// private constructor.
        /// </summary>
        private JsonHelper()
        {
        }

        /// <summary>
        /// Creates an instance. 
        /// </summary>
        /// <returns></returns>
        public static JsonHelper Instance()
        {
            if (instance == null)
            {
                lock (objLock)
                {
                    if (instance == null)
                    {
                        instance = new JsonHelper();
                    }
                }
            }

            return instance;
        }

        /// <summary>
        /// Add content to Json file.
        /// </summary>
        /// <param name="filePath">file to create</param>
        /// <param name="obj">object to serialze</param>
        public void AddContent(string filePath, Object obj)
        {
            using (StreamWriter sw = new StreamWriter(filePath, false))
            {
                JsonSerializer serializer = new JsonSerializer();
                serializer.Serialize(sw, obj);
            }
        }
    }

    /// <summary>
    /// Push Install Json properties. 
    /// </summary>
    public class PushInstallJsonProperties
    {
        /// <summary>
        /// Gets or sets the message.
        /// </summary>
        public string Message { get; set; }
    }
}
