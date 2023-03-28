//---------------------------------------------------------------
//  <copyright file="ExtensionMethods.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Generic extension routines.
//  </summary>
//
//  History:     03-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace ASRSetupFramework
{
    public static class ExtensionMethods
    {
        /// <summary>
        /// Converts string value into Enum.
        /// </summary>
        /// <typeparam name="T">Generic type parameter.</typeparam>
        /// <param name="value">Value to convert into struct.</param>
        /// <returns>Enum value.</returns>
        public static T? AsEnum<T>(this string value)
            where T : struct
        {
            return string.IsNullOrEmpty(value) ? null : (T?)Enum.Parse(typeof(T), value, true);
        }

        /// <summary>
        /// Tries to convert string into enum.
        /// </summary>
        /// <typeparam name="T">Generic type parameter.</typeparam>
        /// <param name="value">Value to convert into struct.</param>
        /// <param name="enumVal">Enum value.</param>
        /// <returns>True if conversion succeeded, false otherwise.</returns>
        public static bool TryAsEnum<T>(this string value, out T enumVal)
            where T : struct
        {
            try
            {
                enumVal = value.AsEnum<T>().Value;
                return true;
            }
            catch
            {
                enumVal = default(T);
                return false;
            }
        }
    }
}
