using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Net;
using System.Net.NetworkInformation;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// JsonConverter for <see cref="IPAddress"/>.
    /// NewtonSoft.Json doesn't natively support <see cref="IPAddress"/>, so
    /// this converter must be attributed to the <see cref="IPAddress"/> members
    /// in the classes used for JSON serialization.
    /// </summary>
    internal class IPAddressJsonConverter : JsonConverter
    {
        public override bool CanRead => true;

        public override bool CanWrite => true;

        public override bool CanConvert(Type objectType)
        {
            return objectType == typeof(IPAddress);
        }

        public override object ReadJson(JsonReader reader, Type objectType, object existingValue, JsonSerializer serializer)
        {
            var ipStr = JToken.Load(reader).ToString();

            if (string.IsNullOrEmpty(ipStr))
                return null;

            return IPAddress.Parse(ipStr);
        }

        public override void WriteJson(JsonWriter writer, object value, JsonSerializer serializer)
        {
            JToken.FromObject(value?.ToString()).WriteTo(writer);
        }
    }

    internal static class JsonUtils
    {
        private static void AddConverters(
            IList<JsonConverter> container, IEnumerable<JsonConverter> converters)
        {
            if (converters == null)
                return;

            foreach (var currConv in converters)
            {
                if (currConv != null)
                    container.Add(currConv);
            }
        }

        public static JsonSerializerSettings GetStandardSerializerSettings(
            bool indent, IEnumerable<JsonConverter> converters = null)
        {
            var toRet = new JsonSerializerSettings
            {
                NullValueHandling = NullValueHandling.Ignore,
                MissingMemberHandling = MissingMemberHandling.Ignore,
                Formatting = indent ? Formatting.Indented : Formatting.None
            };

            AddConverters(toRet.Converters, converters);

            return toRet;
        }

        public static JsonSerializer GetStandardSerializer(
            bool indent, IEnumerable<JsonConverter> converters = null)
        {
            var toRet = new JsonSerializer
            {
                NullValueHandling = NullValueHandling.Ignore,
                MissingMemberHandling = MissingMemberHandling.Ignore,
                Formatting = indent ? Formatting.Indented : Formatting.None
            };

            AddConverters(toRet.Converters, converters);

            return toRet;
        }

        public static IEnumerable<JsonConverter> GetAllKnownConverters()
        {
            return new JsonConverter[]
            {
                new VersionConverter(),
                new IPAddressJsonConverter(),
                new StringEnumConverter()
            };
        }
    }
}
