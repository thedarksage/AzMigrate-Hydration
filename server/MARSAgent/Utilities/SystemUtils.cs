using LoggerInterface;
using MarsAgent.LoggerInterface;
using System;
using System.IO;
using System.Management;

namespace MarsAgent.Utilities
{
    public class SystemUtils
    {
        /// <summary>
        /// Get Bios Id of the system
        /// </summary>
        /// <returns>Bios id string</returns>
        public static string GetBiosId()
        {
            string biosId = string.Empty;

            const string WmiScope = @"\\.\root\cimv2";
            const string WmiQuery = "SELECT * from Win32_ComputerSystemProduct";

            try
            {
                using (var searcher = new ManagementObjectSearcher(WmiScope, WmiQuery))
                using (var resultObjColl = searcher.Get())
                using (var objEnum = resultObjColl.GetEnumerator())
                {
                    if (!objEnum.MoveNext())
                    {
                        throw new InvalidDataException(
                            "Empty WMI object collection is returned for Win32_ComputerSystemProduct query");
                    }

                    using (var currWmiObj = objEnum.Current)
                    {
                        biosId = (string)currWmiObj.GetPropertyValue("UUID");
                    }
                }
            }
            catch (Exception e)
            {
                Logger.Instance.LogException(CallInfo.Site(), e);
            }

            return biosId;
        }
    }
}
