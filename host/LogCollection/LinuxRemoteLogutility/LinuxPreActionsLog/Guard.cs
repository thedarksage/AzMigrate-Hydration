//------------------------------------------------------------
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace LinuxCommunicationFramework
{
    /// <summary>
    /// Implements the common guard methods for argument validations
    /// </summary>
    public static class Guard
    {
        /// <summary>
        /// Checks an argument to ensure it isn't null.
        /// </summary>
        /// <param name="argument">The argument value to check.</param>
        /// <param name="argumentName">The name of the argument.</param>
        public static void RequiresNotNull(object argument, string argumentName)
        {
            if (argument == null)
            {
                throw new ArgumentNullException(argumentName, "argument can't be null");
            }
        }

        /// <summary>
        /// Checks a string argument to ensure it isn't null or whitespace.
        /// </summary>
        /// <param name="argument">The argument value to check.</param>
        /// <param name="argumentName">The name of the argument.</param>
        public static void RequiresNotNullOrWhiteSpace(string argument, string argumentName)
        {
            Guard.RequiresNotNull(argument, argumentName);
            if (string.IsNullOrWhiteSpace(argument))
            {
                throw new ArgumentException("string can't be whitespace", argumentName);
            }
        }

        /// <summary>
        /// Checks a list argument to ensure it isn't null or empty.
        /// </summary>
        /// <param name="argumentList">The argument value to check.</param>
        /// <param name="argumentName">The name of the argument.</param>
        public static void RequiresNotNullOrEmpty<T>(IEnumerable<T> argumentList, string argumentName)
        {
            Guard.RequiresNotNull(argumentList, argumentName);
            if (!argumentList.Any())
            {
                throw new ArgumentException("list can't be empty", argumentName);
            }
        }

        /// <summary>
        /// Checks an argument to ensure that its value is not zero or negative.
        /// </summary>
        /// <param name="argument">The value of the argument.</param>
        /// <param name="argumentName">The name of the argument for diagnostic purposes.</param>
        public static void RequiresPositiveValue(int argument, string argumentName)
        {
            if (argument <= 0)
            {
                throw new ArgumentOutOfRangeException(argumentName, "argument can't be negative or zero, actual value: " + argument);
            }
        }

        /// <summary>
        /// Checks an argument to ensure that its value is not negative.
        /// </summary>
        /// <param name="argument">The value of the argument.</param>
        /// <param name="argumentName">The name of the argument for diagnostic purposes.</param>
        public static void RequiresNonNegativeValue(int argument, string argumentName)
        {
            if (argument < 0)
            {
                throw new ArgumentOutOfRangeException(argumentName, "argument can't be negative, actual value: " + argument);
            }
        }
    }
}
