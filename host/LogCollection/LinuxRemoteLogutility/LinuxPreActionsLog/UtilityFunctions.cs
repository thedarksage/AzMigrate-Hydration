/*++

Copyright (c) 2014  Microsoft Corporation

Module Name:

UtilityFunctions.cs

Abstract:

This file provides the basic helper functions.

Author:

Dharmendra Modi(dmodi) 01-03-2014

--*/

using System;

namespace LinuxCommunicationFramework
{
    public static class UtilityFunctions
    {
        /// <summary>
        /// Checks whether an object is null.
        /// In case of string makes sure string is not null, empty, containing whitespace
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name="obj"></param>
        /// <param name="paramName"></param>
        public static void ValidateNonNullOrWhitespace<T>(T obj, string paramName)
        {
            if (null == obj)
            {
                throw new ArgumentNullException(paramName);
            }
            else if (typeof(string) == obj.GetType())
            {
                if (string.IsNullOrWhiteSpace(obj.ToString()))
                {
                    throw new ArgumentException(paramName);
                }
            }
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="msg"></param>
        public static void LogMethodEntry()
        {
            
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="msg"></param>
        public static void LogDebug(string msg)
        {

        }
    }
}
