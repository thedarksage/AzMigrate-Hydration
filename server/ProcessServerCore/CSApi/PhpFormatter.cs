using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Caching;
using System.Runtime.Serialization;
using System.Text;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi
{
    /// <summary>
    /// Formatter to serialize and unserialize in PHP format
    /// </summary>
    internal sealed class PhpFormatter : IFormatter
    {
        private readonly CacheItemPolicy m_defaultCachePolicy, m_noSlidingExpirationCachePolicy;
        private readonly bool m_serializeObjectAsArray;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="serializeObjectAsArray">Serialize an object as PHP array</param>
        public PhpFormatter(bool serializeObjectAsArray)
            : this(serializeObjectAsArray, Settings.Default.PhpFormatter_CacheSlidingExpiration)
        {
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="serializeObjectAsArray">Serialize an object as PHP array</param>
        /// <param name="defaultCacheSlidingExpiration">
        /// Expiry time for cached serializer actions of complex types
        /// </param>
        public PhpFormatter(bool serializeObjectAsArray, TimeSpan defaultCacheSlidingExpiration)
        {
            // TODO-SanKumar-1906: When enabling this, we should also be ensuring
            // that two different cache items must be maintained for each category
            // 1) object as PHP array 2) object as PHP object, since cache is
            // singleton for the process
            if (!serializeObjectAsArray)
                throw new NotImplementedException("serializeObjectAsArray");

            m_serializeObjectAsArray = serializeObjectAsArray;

            // Used for complex data types, since it could've been an one-time use
            m_defaultCachePolicy = new CacheItemPolicy()
            {
                SlidingExpiration = defaultCacheSlidingExpiration
            };

            // Used for basic data types
            m_noSlidingExpirationCachePolicy = new CacheItemPolicy();
        }

        #region Unimportant IFormatter methods

        /// <summary>
        /// Serialization Binder (Not implemented)
        /// </summary>
        public SerializationBinder Binder
        {
            get
            {
                throw new NotImplementedException();
            }
            set
            {
                throw new NotImplementedException();
            }
        }

        /// <summary>
        /// Streaming Context (Not implemented)
        /// </summary>
        public StreamingContext Context
        {
            get
            {
                throw new NotImplementedException();
            }
            set
            {
                throw new NotImplementedException();
            }
        }

        /// <summary>
        /// Surrogate Selector (Not implemented)
        /// </summary>
        public ISurrogateSelector SurrogateSelector
        {
            get
            {
                throw new NotImplementedException();
            }
            set
            {
                throw new NotImplementedException();
            }
        }

        #endregion Unimportant IFormatter methods

        /// <summary>
        /// Deserialize stream containing PHP serialized string into object
        /// </summary>
        /// <param name="serializationStream">Stream containing PHP serialized string</param>
        /// <returns>Deserialized object</returns>
        public object Deserialize(Stream serializationStream)
        {
            // TODO-SanKumar-1904:
            throw new NotImplementedException();
        }

        /// <summary>
        /// PHP serialize the object into the stream
        /// </summary>
        /// <param name="serializationStream">Stream to write the serialized string</param>
        /// <param name="graph">Object to be serialized</param>
        public void Serialize(Stream serializationStream, object graph)
        {
            // TODO-SanKumar-1903: Move buffer size to settings
            using (var sw = new FormattingStreamWriter(serializationStream, CultureInfo.InvariantCulture, Encoding.UTF8, 4096, leaveOpen: true) { AutoFlush = false })
            {
                if (graph == null)
                {
                    sw.Write("N;"); // PHP null
                    return;
                }

                var graphType = graph.GetType();
                GetSerializerAction(graphType)(sw, graph);
            }
        }

        private readonly HashSet<Type> m_phpIntegerTypes = new HashSet<Type>() { typeof(Int16), typeof(Int32), typeof(Int64), typeof(UInt16), typeof(UInt32), typeof(UInt64) };
        private readonly Type[] m_phpFloatTypes = new[] { typeof(float), typeof(double), typeof(decimal) };

        // TODO-SanKumar-1903: Recursive usage of objects would lead to stack overflow.
        private Action<StreamWriter, object> GetSerializerAction(Type objectType)
        {
            Action<StreamWriter, object> toRet;
            CacheItemPolicy cacheItemPolicy;

            // TODO-SanKumar-1903: Big string. Is there any way to optimize this?!
            string cacheKey = objectType.AssemblyQualifiedName;

            // Try retrieving the parsing action from the cache first, if present
            var cachedValue = MemoryCache.Default.Get(cacheKey);
            if (cachedValue != null)
                return cachedValue as Action<StreamWriter, object>;

            Type iROCollectionType;

            if (objectType == typeof(string))
            {
                toRet = (sw, o) =>
                {
                    // Non-printable chars such as \n are literally serialized,
                    // i.e. a newline would be found in the serialized string.
                    // So, need to escape anything in the string.
                    if (o != null)
                        sw.Write("s:{0}:\"{1}\";", (o as string).Length, o);
                    else
                        sw.Write("N;");
                };

                cacheItemPolicy = m_noSlidingExpirationCachePolicy;
            }
            else if (m_phpIntegerTypes.Contains(objectType))
            {
                toRet = (sw, o) => sw.Write("i:{0};", o);
                cacheItemPolicy = m_noSlidingExpirationCachePolicy;
            }
            else if (objectType == typeof(bool))
            {
                toRet = (sw, o) => sw.Write("b:{0};", (bool)o ? 1 : 0);
                cacheItemPolicy = m_noSlidingExpirationCachePolicy;
            }
            else if (m_phpFloatTypes.Contains(objectType))
            {
                // Always in fixed point format with two decimal digits.
                // More than enough for our usage. Otherwise, becomes complex
                // if scientific notation gets involved.
                toRet = (sw, o) => sw.Write("d:{0:F2};", o);
                cacheItemPolicy = m_noSlidingExpirationCachePolicy;
            }
            // If the object is a collection
            else if ((iROCollectionType = objectType.GetInterface("System.Collections.Generic.IReadOnlyCollection`1")) != null)
            {
                var collElementType = iROCollectionType.GenericTypeArguments[0];

                // If the object is a collection of key-value pairs (ex: dictionary)
                if (collElementType.IsGenericType &&
                    collElementType.GetGenericTypeDefinition() == typeof(KeyValuePair<,>))
                {
                    var keyType = collElementType.GenericTypeArguments[0];
                    Action<StreamWriter, object> keySerAction;

                    if (keyType == typeof(string) || m_phpIntegerTypes.Contains(keyType))
                    {
                        keySerAction = GetSerializerAction(keyType);
                    }
                    else if (keyType == typeof(bool) || m_phpFloatTypes.Contains(keyType))
                    {
                        // As per documentation: http://php.net/manual/en/language.types.array.php
                        // floats and bools will be converted to int keys.
                        // TODO-SanKumar-1905: Rounding of float values would lead to duplicate keys - not handled.
                        var intSerAction = GetSerializerAction(typeof(Int64));
                        keySerAction = (sw, o) => intSerAction(sw, Convert.ToInt64(o));
                    }
                    else
                    {
                        throw new NotSupportedException(keyType + " is not supported for associative php array");
                    }

                    var keyPropInfo = collElementType.GetProperty("Key");
                    var valPropInfo = collElementType.GetProperty("Value");

                    toRet = (sw, o) =>
                    {
                        sw.Write("a:{0}:{{", iROCollectionType.GetProperty("Count").GetValue(o));

                        foreach (var currItem in (o as IEnumerable))
                        {
                            keySerAction(sw, keyPropInfo.GetValue(currItem));

                            // TODO-SanKumar-1904: If value type is well-known,
                            // we could cache it instead of dynamically trying
                            // to fetch the action everytime.
                            object value = valPropInfo.GetValue(currItem);
                            GetSerializerAction(value.GetType())(sw, value);
                        }
                        sw.Write('}');
                    };
                }
                else
                {
                    var intSerAction = GetSerializerAction(typeof(int));

                    toRet = (sw, o) =>
                    {
                        int ind = 0;

                        sw.Write("a:{0}:{{", iROCollectionType.GetProperty("Count").GetValue(o));
                        foreach (var currItem in (o as IEnumerable))
                        {
                            intSerAction(sw, ind++);
                            GetSerializerAction(currItem.GetType())(sw, currItem);
                        }
                        sw.Write('}');
                    };
                }

                cacheItemPolicy = m_defaultCachePolicy;
            }
            // Serialize each member of a .Net object
            else
            {
                // Refer https://docs.microsoft.com/en-us/dotnet/standard/serialization/steps-in-the-serialization-process

                // Not checking, if this.SurrogateSelector could take up serializing this object.

                if (!objectType.IsSerializable)
                    throw new SerializationException("The object is not marked as [Serializable]");

                // Not checking, if the object implements ISerializable and take up serializing itself.

                var nonStaticProps = objectType.GetProperties(BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Instance);
                var nonStaticFields = objectType.GetFields(BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Instance);

                var memberSerializerActions = new List<Action<StreamWriter, object>>(nonStaticFields.Length + nonStaticProps.Length);
                var stringSerAction = GetSerializerAction(typeof(string));

                foreach (var currPropInfo in nonStaticProps)
                {
                    if (!currPropInfo.CanRead)
                        continue;

                    var memSerAction = currPropInfo.PropertyType == typeof(string) ?
                        stringSerAction :
                        GetSerializerAction(currPropInfo.PropertyType);

                    memberSerializerActions.Add((sw, o) =>
                    {
                        stringSerAction(sw, currPropInfo.Name);
                        memSerAction(sw, currPropInfo.GetValue(o));
                    });
                }

                // TODO-SanKumar-1906: Avoid backing fields of auto properties
                // Roslync provides AssociatedPropertyOrEvent
                foreach (var currFieldInfo in nonStaticFields)
                {
                    // If a member has [NonSerialized] attribute, ignore it.
                    // Note that, this attribute can't be set to a property.
                    if (currFieldInfo.GetCustomAttribute<NonSerializedAttribute>() != null)
                        continue;

                    var memSerAction = currFieldInfo.FieldType == typeof(string) ?
                        stringSerAction :
                        GetSerializerAction(currFieldInfo.FieldType);

                    memberSerializerActions.Add((sw, o) =>
                    {
                        stringSerAction(sw, currFieldInfo.Name);
                        memSerAction(sw, currFieldInfo.GetValue(o));
                    });
                }

                memberSerializerActions.TrimExcess();
                toRet = (sw, o) =>
                {
                    if (o == null)
                    {
                        sw.Write("N;");
                    }
                    else
                    {
                        Debug.Assert(m_serializeObjectAsArray);

                        sw.Write("a:{0}:{{", memberSerializerActions.Count);
                        memberSerializerActions.ForEach(currAction => currAction(sw, o));
                        sw.Write('}');
                    }
                };
                cacheItemPolicy = m_defaultCachePolicy;
            }

            // Cache the newly generated serialization action for the passed type.
            var cachedItem = MemoryCache.Default.AddOrGetExisting(cacheKey, toRet, cacheItemPolicy);

            return (cachedItem ?? toRet) as Action<StreamWriter, object>;
        }
    }
}
