using System;
using System.Collections.Generic;
using System.Runtime.Serialization;
using System.IO;
using System.Linq;
using System.Text;

namespace ASRSetupFramework
{
    /// <summary>
    /// Serializable class for error jsons
    /// </summary>
    [DataContract]
    public class InstallerException
    {
        [DataMember(Name = "error_name", IsRequired = true)]
        public string error_name { get; set; }

        [DataMember(Name = "error_params", IsRequired = false)]
        public IDictionary<string, string> error_params { get; set; }

        [DataMember(Name = "default_message", IsRequired = true)]
        public string default_message { get; set; }

        public InstallerException(string ErrorName, IDictionary<string, string> ErrorParams, string DefaultMessage)
        {
            error_name = ErrorName;
            error_params = ErrorParams;
            default_message = DefaultMessage;
        }

        public override string ToString()
        {
            return string.Format("error_name: {0} , default_message: {1} , error_params: {2}",
            error_name, default_message,
            error_params == null ? string.Empty : string.Join(";", error_params.Select(x => x.Key + "=" + x.Value).ToArray()));
        }
    }

    [DataContract]
    public class ExtensionErrors
    {
        /// <summary>
        /// Gets or sets the details of extension errors. Includes extension, installer and
        /// configurator errors.
        /// </summary>
        [DataMember(Name = "Errors", IsRequired = true)]
        public IList<InstallerException> Errors { get; set; }
    }
}
