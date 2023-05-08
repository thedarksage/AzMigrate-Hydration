/*++

Copyright (c) 2014  Microsoft Corporation

Module Name:

LinuxClient.Cache.cs

Abstract:

This file provides caching for Linux Client Instances.

Author:

Dharmendra Modi(dmodi) 01-03-2014

--*/

using System;
using System.Globalization;
using System.Runtime.Caching;

namespace LinuxCommunicationFramework
{
    /// <summary>
    /// Enum to specify a Linux Target Varient
    /// </summary>
    public enum LinuxTargetVariant
    {
        /// <summary>
        /// 
        /// </summary>
        Default = 0,

        /// <summary>
        /// 
        /// </summary>
        Ubuntu1310,

        /// <summary>
        /// 
        /// </summary>
        Ubuntu1404,

        /// <summary>
        /// 
        /// </summary>
        RHEL60,

        /// <summary>
        /// 
        /// </summary>
        RHEL55,

        /// <summary>
        /// 
        /// </summary>
        CentOS60,

        /// <summary>
        /// 
        /// </summary>
        CentOS55
    }

    public partial class LinuxClient
    {
        private static readonly MemoryCache LinuxClientCache = MemoryCache.Default;

        //As of now we are continuing with the Default CacheItemPolicy
        private static readonly CacheItemPolicy ItemPolicy = new CacheItemPolicy();

        /// <summary>
        /// Function to Get a Linux Client Instance. If you are not sure which LinuxTargetVariant to specify, you probably
        /// need to pass LinuxTargetVariant.Default as linuxVariant
        /// 
        /// It looks in its cache for the object existence if present returns the same object 
        /// In case the object is not present it is created and added to the cache.
        /// </summary>
        /// <param name="sshTargetConnectionParams"></param>
        /// <param name="linuxVariant">Linux Target Variant</param>
        /// <returns>LinuxClient object</returns>
        public static LinuxClient GetInstance(SSHTargetConnectionParams sshTargetConnectionParams,
            LinuxTargetVariant linuxVariant)
        {
            return GetInstance(sshTargetConnectionParams, linuxVariant, true);
        }

        /// <summary>
        /// Function to Get a Linux Client Instance. If you are not sure which LinuxTargetVariant to specify, you probably
        /// need to pass LinuxTargetVariant.Default as linuxVariant
        /// 
        /// It looks in its cache for the object existence if present returns the same object 
        /// In case the object is not present it is created and added to the cache.
        /// </summary>
        /// <param name="sshTargetConnectionParams"></param>
        /// <param name="linuxVariant">Linux Target Variant</param>
        /// <param name="cacheClient">True if caching can be done for Linux Client else false</param>
        /// <returns>LinuxClient object</returns>
        public static LinuxClient GetInstance(SSHTargetConnectionParams sshTargetConnectionParams,
            LinuxTargetVariant linuxVariant, bool cacheClient)
        {
            Guard.RequiresNotNullOrWhiteSpace(sshTargetConnectionParams.CommunicationIP, "SSHTargetConnectionParams.CommunicationIP");
            Guard.RequiresNotNullOrWhiteSpace(sshTargetConnectionParams.User, "SSHTargetConnectionParams.User");
            Guard.RequiresNotNullOrWhiteSpace(sshTargetConnectionParams.AuthValue, "SSHTargetConnectionParams.AuthValue");

            string cacheKey = CreateCacheKey(sshTargetConnectionParams.CommunicationIP, sshTargetConnectionParams.User);
            var cacheItem = LinuxClientCache.GetCacheItem(cacheKey);

            LinuxClient linuxClient;

            if (cacheClient)
            {
                if (null != cacheItem &&
                    null != (linuxClient = cacheItem.Value as LinuxClient))
                {
                    return linuxClient;
                }
            }

            //Fetch the correct Linux Variant
            linuxClient = CreateLinuxClient(sshTargetConnectionParams, linuxVariant);
            if (!cacheClient) return linuxClient;
            
            var item = new CacheItem(cacheKey, linuxClient);

            LinuxClientCache.Add(item, ItemPolicy);

            return linuxClient;
        }

        /// <summary>
        /// Function to Get a Linux Client Instance
        /// 
        /// It looks in its cache for the object existence if present returns the same object 
        /// In case the object is not present it is created and added to the cache.
        /// </summary>
        /// <param name="sshTargetConnectionParams"></param>
        /// <returns>LinuxClient object</returns>
        public static LinuxClient GetInstance(SSHTargetConnectionParams sshTargetConnectionParams)
        {
            return GetInstance(sshTargetConnectionParams, LinuxTargetVariant.Default);
        }

        private static string CreateCacheKey(string IPAddress, string user)
        {
            string cacheKey = IPAddress + "#" + user;

            return cacheKey;
        }

        private static LinuxClient CreateLinuxClient(SSHTargetConnectionParams sshTargetConnectionParams,
            LinuxTargetVariant linuxVariant)
        {
            LinuxClient linuxClient = null;
            switch (linuxVariant)
            {
                case LinuxTargetVariant.Default:
                    linuxClient = new LinuxClient(sshTargetConnectionParams);
                    break;
                    //case LinuxTargetVariant.RHEL60:
                    //Keep on adding as and when needed.
                    //Example : Create a definition public class RHEL60Client : LinuxClient {...}
                    //Override the function of interest.
                    //linuxClient = new RHEL60Client(windowsHostInfo, sshTargetConnectionParams);
                    //break;
                default:
                    throw new NotImplementedException(linuxVariant.ToString());
            }

            return linuxClient;
        }
    }
}
