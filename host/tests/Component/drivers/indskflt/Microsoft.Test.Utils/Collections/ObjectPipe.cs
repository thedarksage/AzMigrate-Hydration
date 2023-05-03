using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace Microsoft.Test.IOGen.Utils.Collections
{
    internal class ObjectPipe<T> : BlockingCollection<T>
    {
        public ReadOnlyDictionary<string, object> Attributes { get; private set; }

        private static readonly Dictionary<string, object> EmptyDictionary = new Dictionary<string, object>(0);

        public ObjectPipe()
            : this(EmptyDictionary)
        {
        }

        public ObjectPipe(int boundedCapacity)
            : this(boundedCapacity, EmptyDictionary)
        {
        }

        public ObjectPipe(IDictionary<string, object> dictionary)
            : base(new ConcurrentQueue<T>())
        {
            var nonNullDict = dictionary == null ? EmptyDictionary : dictionary;
            this.Attributes = new ReadOnlyDictionary<string, object>(nonNullDict);
        }

        public ObjectPipe(int boundedCapacity, IDictionary<string, object> dictionary)
            : base(new ConcurrentQueue<T>(), boundedCapacity)
        {
            var nonNullDict = dictionary == null ? EmptyDictionary : dictionary;
            this.Attributes = new ReadOnlyDictionary<string, object>(nonNullDict);
        }
    }
}