using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using System.IO;

namespace InMage.APIHelpers
{
    public class Utils
    {
        private static readonly object lockObject = new object();

        /// <summary>
        /// Parses a string and converts it to corresponding Enum.
        /// If spaces exixts in the string, it removes them and compares with the given Enum type ignoring case
        /// Also if the value is null or empty, it returns the Enum whose value is 0(default)
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name="value"></param>
        /// <returns></returns>
        public static T ParseEnum<T>(string value)
        {
            try
            {
                value = value.Replace(" ", string.Empty);
                if (string.IsNullOrEmpty(value))
                {
                    return (T)(0 as object);
                }

                return (T)Enum.Parse(typeof(T), value, true);
            }
            catch
            {
                return (T)(0 as object);
            }
        }

        public static IPAddress ParseIP(string value)
        {
            try
            {
                if (!string.IsNullOrEmpty(value))
                {
                    return IPAddress.Parse(value);
                }

                return null;
            }
            catch
            {
                return null;
            }
        }

        public static Guid ParseGuid(string value)
        {
            try
            {
                if (!string.IsNullOrEmpty(value.Replace("-", string.Empty)))
                {
                    return new Guid(value);
                }

                return Guid.Empty;
            }
            catch
            {
                return Guid.Empty;
            }
        }


        public static OperationStatus ParseDeleteStatus(string deleteStatus)
        {
            OperationStatus status = OperationStatus.Unknown;
            if (!String.IsNullOrEmpty(deleteStatus))
            {
                if (String.Compare(deleteStatus, "Delete Success", StringComparison.OrdinalIgnoreCase) == 0)
                {
                    status = OperationStatus.Completed;
                }
                else if (String.Compare(deleteStatus, "Delete Pending", StringComparison.OrdinalIgnoreCase) == 0)
                {
                    status = OperationStatus.Pending;
                }
                else if (String.Compare(deleteStatus, "Delete Failed", StringComparison.OrdinalIgnoreCase) == 0)
                {
                    status = OperationStatus.Failed;
                }
            }
            return status;
        }

        public static DateTime? UnixTimeStampToDateTime(string unixTimeStamp)
        {
            long timeStamp;
            if (long.TryParse(unixTimeStamp, out timeStamp))
            {
                DateTime dt = new DateTime(1970, 1, 1, 0, 0, 0, 0, System.DateTimeKind.Utc);
                dt = dt.AddSeconds(timeStamp);
                return dt;
            }
            return null;
        }

        public static long DateTimeToUnixTimeStamp(DateTime dt)
        {
            try
            {
                TimeSpan timeSpan = dt.Subtract(new DateTime(1970, 1, 1, 0, 0, 0, System.DateTimeKind.Utc));
                return (long)timeSpan.TotalSeconds;
            }
            catch
            {
                return 0;
            }
        }

        internal static string FilterRequestXML(string requestXML)
        {
            string formattedRequestXML = null;
            if (!String.IsNullOrEmpty(requestXML))
            {
                string pattern = @"<Parameter Name=""Password"" Value=""\S*""";                
                string replaceValue = @"<Parameter Name=""Password"" Value=""*""";
                System.Text.RegularExpressions.Regex regex = new System.Text.RegularExpressions.Regex(pattern);
                formattedRequestXML = regex.Replace(requestXML, replaceValue);
            }
            return formattedRequestXML;
        }

        internal static bool WriteLog(string logFilePath, string data)
        {
            try
            {
                string filePath = logFilePath;
                if (String.IsNullOrEmpty(filePath))
                {
                    if (HttpCommunication.CXSecurityInfo.GetInstalledComponent() == HttpCommunication.InMageComponent.CX)
                    {
                        filePath = @"C:\home\svsystems\var\InMageAPILibrary.log";
                    }
                }
                if (!String.IsNullOrEmpty(filePath))
                {
                    lock (lockObject)
                    {
                        using (StreamWriter writer = File.AppendText(filePath))
                        {
                            writer.WriteLine(String.Format("{0,-23}{1}", DateTime.Now, data));
                        }
                    }
                    return true;
                }
                return false;
            }
            catch 
            {
                return false;
            }
        }
    }
}
