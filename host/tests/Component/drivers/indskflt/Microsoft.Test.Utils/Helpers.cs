using Microsoft.Test.IOGen.IOParamGenerators;
using Microsoft.Test.IOGen.Storages.Utils.NativeMacros;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.Net;
using System.Text.RegularExpressions;
using System.Xml.Linq;

namespace Microsoft.Test.IOGen.Utils
{
    public static class BasicHelper
    {
        public static uint GetValueForPercentage(uint totalValue, uint percentage)
        {
            return totalValue * percentage / 100;
        }

        public static void Swap<T>(IList<T> list, int ind1, int ind2)
        {
            T temp = list[ind1];
            list[ind1] = list[ind2];
            list[ind2] = temp;
        }
    }

    public static class XmlToLinqHelper
    {
        public static XAttribute GetAttribute(this IEnumerable<XAttribute> attributeList, string localName)
        {
            return attributeList.FirstOrDefault(currAttrib => currAttrib.Name.LocalName.Equals(localName, StringComparison.OrdinalIgnoreCase));
        }
    }

    public static class IEnumerableHelper
    {
        public static bool IsNullOrEmpty<T>(this IEnumerable<T> list)
        {
            return list == null || !list.Any();
        }

        public static IEnumerable<T> Randomize<T>(this IEnumerable<T> list)
        {
            if (list.IsNullOrEmpty())
                return list;

            Random rand = new Random();
            return list.OrderBy(currElement => rand.Next());
        }
    }

    public static class StringHelper
    {
        public static string RemoveWhiteSpaces(this string textToEdit)
        {
            if (textToEdit == null)
                return null;

            return Regex.Replace(textToEdit, @"\s", string.Empty);
        }

        public static string GetFormattedString(string format, params object[] args)
        {
            return string.Format(CultureInfo.InvariantCulture, format, args);
        }
    }

    public static class PropertyHelper
    {
        public static void EnsureNonEmptyString(this string value, string propertyName)
        {
            if (string.IsNullOrEmpty(value))
                throw new Exception(string.Format(CultureInfo.InvariantCulture, "{0} shouldn't be empty", propertyName));
        }

        public static void EnsureNonZeroValue(this ulong value, string propertyName)
        {
            if (value == 0)
                throw new Exception(string.Format(CultureInfo.InvariantCulture, "{0} shouldn't be zero", propertyName));
        }
        public static void EnsureNonZeroValue(this uint value, string propertyName)
        {
            if (value == 0)
                throw new Exception(string.Format(CultureInfo.InvariantCulture, "{0} shouldn't be zero", propertyName));
        }
        public static void EnsureNonZeroValue(this double value, string propertyName)
        {
            if (value == 0)
                throw new Exception(string.Format(CultureInfo.InvariantCulture, "{0} shouldn't be zero", propertyName));
        }
    }

    public static class IOPatternHelper
    {
        #region Validation helpers

        public static void Validate(params IValidatablePatternElement[] elements)
        {
            Validate(elements.AsEnumerable());
        }

        public static void Validate(params IEnumerable<IValidatablePatternElement>[] elementCollections)
        {
            IEnumerable<IValidatablePatternElement> flattenEnum = elementCollections.SelectMany(currColl => currColl);
            Validate(flattenEnum);
        }

        public static void Validate(IEnumerable<IValidatablePatternElement> elements)
        {
            foreach (var currElement in elements)
                currElement.Validate();
        }

        #endregion

        #region Name distinction checkers

        public static void EnsureNameDistinction(bool ignoreCase, params NamedObject[] elements)
        {
            EnsureNameDistinction(ignoreCase, elements.AsEnumerable());
        }

        public static void EnsureNameDistinction(bool ignoreCase, IEnumerable<NamedObject> elements)
        {
            StringComparer comparer = ignoreCase ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;

            var names = elements.Select(currElement => currElement.Name).ToList();

            if (names.Any(currName => string.IsNullOrEmpty(currName)))
                throw new Exception("Names shouldn't be empty");

            var distinctNamesCnt = names.Distinct(comparer).Count();
            if (distinctNamesCnt < names.Count)
                throw new Exception("Names should be distinct");
        }

        public static void EnsureNameDistinction(bool ignoreCase, params IEnumerable<NamedObject>[] elementCollections)
        {
            foreach (var currCollection in elementCollections)
                EnsureNameDistinction(ignoreCase, currCollection);
        }

        #endregion

        #region IOVariations Helpers

        public static FileFlagsAndAttributes GetFileFlagsAndAttributes(this IOVariations variations)
        {
            FileFlagsAndAttributes toRet = FileFlagsAndAttributes.Normal;

            if (variations.HasFlag(IOVariations.Async))
                toRet |= FileFlagsAndAttributes.Overlapped;
            if (variations.HasFlag(IOVariations.Unbuffered))
                toRet |= FileFlagsAndAttributes.NoBuffering;
            if (variations.HasFlag(IOVariations.WriteThrough))
                toRet |= FileFlagsAndAttributes.WriteThrough;

            return toRet;
        }

        public static IOVariations GetIOVariations(this string str, bool ignoreCase)
        {
            return string.IsNullOrEmpty(str) ? IOVariations.None : (IOVariations)Enum.Parse(typeof(IOVariations), str, ignoreCase);
        }

        #endregion

        #region TimeStamp Helper

        public static TimeSpan GetTimeStamp(this string str)
        {
            if (str.IsNullOrEmpty())
                return TimeSpan.Zero;

            // TO DO : Add support for hours, minutes, seconds, etc.
            int seconds = int.Parse(str, CultureInfo.InvariantCulture);
            return new TimeSpan(0, 0, seconds);
        }

        public static string GetExpandedExpression(this TimeSpan interval)
        {
            // TO DO : Add support for hours, minutes, seconds, etc.
            return interval.TotalSeconds.ToString(CultureInfo.InvariantCulture);
        }

        #endregion
    }

    public enum PowerOfBytes : byte
    {
        Bytes = 0,
        KB = 10,
        MB = 20,
        GB = 30,
        TB = 40
    }

    [Serializable]
    public class Bytes
    {
        public ulong TotalBytes { get; private set; }

        public decimal Value { get; private set; }
        public PowerOfBytes Power { get; private set; }

        public Bytes(decimal value, PowerOfBytes power)
        {
            this.Value = value;
            this.Power = power;

            //if (this.Power == PowerOfBytes.Bytes
            //    && Math.Ceiling(this.Value) != Math.Floor(this.Value))
            //{
            //    throw new Exception("Value shouldn't have decimal value, if Power is Bytes");
            //}

            checked
            {
                this.TotalBytes = (ulong)(this.Value * (1UL << (byte)this.Power));
            }
        }

        public override string ToString()
        {
            return string.Format(CultureInfo.InvariantCulture, "{0} {1}", this.Value, this.Power);
        }

        public static Bytes GetBytes(string str)
        {
            // Example strings:
            // 10 GB
            // 5.3 mb
            // 100

            str = str.Trim();
            int endOfNumber = str.Length;
            // If no suffices are present, then it's bytes by default
            PowerOfBytes power = PowerOfBytes.Bytes;

            foreach (var currPower in (PowerOfBytes[])Enum.GetValues(typeof(PowerOfBytes)))
            {
                var currPowerName = currPower.ToString();
                // TODO: Parsing is done twice.  Can use str.LastIndexOf() below.
                // But this double parsing is easily readable and could be kept as such.
                if (str.EndsWith(currPowerName, StringComparison.OrdinalIgnoreCase))
                {
                    power = currPower;
                    endOfNumber = str.LastIndexOf(currPowerName, StringComparison.OrdinalIgnoreCase);
                    break;
                }
            }

            string valueStr = str.Substring(0, endOfNumber).TrimEnd();
            decimal value = decimal.Parse(valueStr, CultureInfo.InvariantCulture);

            return new Bytes(value, power);
        }

        public static implicit operator ulong(Bytes bytes)
        {
            return bytes.TotalBytes;
        }

        public static Tuple<Bytes, TimeSpan> GetBytesPerInterval(string bytesPerIntervalStr)
        {
            // Example strings:
            // 100 mb / 0:10:05
            // 50 / 01:0:00
            // 1 GB
            // (default time span is 1 sec)

            if (string.IsNullOrWhiteSpace(bytesPerIntervalStr))
                return null;

            var splitBpi = bytesPerIntervalStr.Split(
                new char[] { '/' }, StringSplitOptions.RemoveEmptyEntries).Select(currStr => currStr.Trim()).ToArray();
            if (splitBpi.Length > 2)
            {
                throw new FormatException(string.Format(CultureInfo.InvariantCulture,
                    "Input string : {0} is invalid\n" +
                    "Format of BytesPerInterval is either [Bytes/Timespan] or [Bytes] (1 sec default)", bytesPerIntervalStr));
            }
            else
            {
                var bytes = Bytes.GetBytes(splitBpi.First());
                var interval = TimeSpan.FromSeconds(1);
                if (splitBpi.Length == 2)
                    interval = TimeSpan.Parse(splitBpi[1], CultureInfo.InvariantCulture);

                return new Tuple<Bytes, TimeSpan>(bytes, interval);
            }
        }
    }

    #region Commented

    //public static class UnmanagedHelper
    //{
    //    // TO DO : If not performant, use memcpy
    //    public unsafe static void Copy(byte* source, uint sourceIndex, byte* destination, uint destinationIndex, uint length)
    //    {
    //        const uint ULONG_SIZE = sizeof(ulong);

    //        source += sourceIndex;
    //        destination += destinationIndex;

    //        ulong* ulongSrcPtr = (ulong*)source, ulongDestPtr = (ulong*)destination;
    //        uint ulongCopyIterations = length / ULONG_SIZE + (length % ULONG_SIZE == 0 ? 1U : 0);

    //        for (uint i = 0; i < ulongCopyIterations; ++i, ++ulongSrcPtr, ++ulongDestPtr)
    //            *ulongDestPtr = *ulongSrcPtr;

    //        uint bytesCopied = ulongCopyIterations * ULONG_SIZE;
    //        source += bytesCopied;
    //        destination += bytesCopied;
    //        uint byteCopyIterations = length - bytesCopied;

    //        for (uint j = 0; j < byteCopyIterations; ++j, ++source, ++destination)
    //            *destination = *source;
    //    }
    //} 

    #endregion

    public static class BufferHelper
    {
        // TO DO : If this is non-performant, use memset
        // From the benchmark indicated here - http://stackoverflow.com/questions/1897555/what-is-the-equivalent-of-memset-in-c,
        // for loop itself seems to be faster
        public static void Fill(byte[] buffer, byte byteToFill)
        {
            for (int i = 0; i < buffer.Length; i++)
                buffer[i] = byteToFill;
        }
    }

    public static class NetworkHelper
    {
        public static bool IsLocalMachine(string hostNameOrAddress)
        {
            var dnsEntry = Dns.GetHostEntry(hostNameOrAddress ?? string.Empty);
            var dnsNameOfLocalComputer = Dns.GetHostEntry(IPAddress.IPv6Loopback).HostName;

            // TO DO : Is case-sensitive comparison needed?
            return string.Equals(dnsEntry.HostName, dnsNameOfLocalComputer, StringComparison.Ordinal);
        }
    }

    public static class IOParamHelper
    {
        public static TIOParam GetCastedIOParam<TIOParam>(IIOParam ioParam)
            where TIOParam : class, IIOParam
        {
            if (ioParam == null)
                throw new ArgumentNullException("ioParam");

            var castedIOParam = ioParam as TIOParam;
            if (castedIOParam == null)
            {
                throw new InvalidCastException(string.Format(CultureInfo.InvariantCulture,
                    "Unexpected IOParam type : {0}.  Expected IOParam type : {1}",
                    ioParam.GetType(), typeof(TIOParam)));
            }

            return castedIOParam;
        }
    }

    public static class EventLogHelper
    {
        public static void LogInEventLog(this EventLog evtLog, EventLogEntryType entryType, string format, params object[] args)
        {
            if (evtLog != null)
                evtLog.WriteEntry(StringHelper.GetFormattedString(format, args), entryType);
        }
    }

    /// <summary>
    /// Provides extension methods to System.Random class for generating random values for other primitive data types
    /// </summary>
    public static class RandomHelper
    {
        // Complete Pseudo-Randomness is not assured!
        // TODO: The probability is not equally distributed to all the numbers in the sample space.  Correct it!!
        // i.e.  Probability of finding a region is equally distributed say X
        // Probability of finding a number in a region (with N numbers) is X / N
        // This is not a problem for the regions in  between, since N is same for all = uint.Max
        // But, for Start(with N-A numbers) and End(with N-B numbers) region, the probability is more than the others,
        // X / N-A and X / N - B respectively (more than the regions in middle and also mismatching between themselves)

        // TODO: Another method suggested by LoganatR (Seems closer to complete pseudo-randomness than the existing one)
        // Create a random ulong (by generating random int32 twice, one for first 32 bit and  for the last 32 bit)
        // Then scale down the random number to the range between minvalue and maxValue
        public static ulong Next(this Random rand, ulong minValue, ulong maxValue)
        {
            // uint.Max * uint.Max = ulong.Max => Split the sample space into uint.Max sub regions of size uint.Max
            uint minValueRegion = (uint)(minValue / uint.MaxValue);
            uint maxValueRegion = (uint)(maxValue / uint.MaxValue);
            if (maxValueRegion == uint.MaxValue)
                --maxValueRegion;

            uint randRegion = rand.Next(minValueRegion, maxValueRegion + 1);

            uint minValueWithinRegion = randRegion == minValueRegion ? (uint)(minValue % uint.MaxValue) : 0;
            uint maxValueWithinRegion = randRegion == maxValueRegion ? (uint)(maxValue % uint.MaxValue) : uint.MaxValue;
            uint randValueWithinRegion = rand.Next(minValueWithinRegion, maxValueWithinRegion);

            return (ulong)uint.MaxValue * randRegion + randValueWithinRegion;
        }

        // Complete Pseudo-Randomness assured
        public static uint Next(this Random rand, uint minValue, uint maxValue)
        {
            var randInt = rand.Next(ToInt32(minValue), ToInt32(maxValue));
            return ToUInt32(randInt);
        }

        // The following functions convert int to uint and viceversa, such that int.max = uint.max and int.min = uint.min
        private static int ToInt32(uint value)
        {
            return (int)((long)value + int.MinValue);
        }

        private static uint ToUInt32(int value)
        {
            return (uint)((long)value - int.MinValue);
        }
    }
}