using System;
using System.Threading;

namespace Microsoft.Test.IOGen.Utils.Collections
{
    public class ObjectPipeWriter<T>
    {
        internal ObjectPipe<T> ObjectPipe { get; private set; }

        internal ObjectPipeWriter(ObjectPipe<T> objectPipe)
        {
            this.ObjectPipe = objectPipe;
        }

        public void InformWritesCompletion()
        {
            this.ObjectPipe.CompleteAdding();
        }

        public bool IsWriteCompleted { get { return this.ObjectPipe.IsAddingCompleted; } }

        public void WriteNext(T toWriteObject)
        {
            this.ObjectPipe.Add(toWriteObject);
        }

        public void WriteNext(T toWriteObject, CancellationToken ct)
        {
            this.ObjectPipe.Add(toWriteObject, ct);
        }

        public bool TryWriteNext(T toWriteObject)
        {
            return this.ObjectPipe.TryAdd(toWriteObject);
        }

        public bool TryWriteNext(T toWriteObject, int millisecondsTimeout)
        {
            return this.ObjectPipe.TryAdd(toWriteObject, millisecondsTimeout);
        }

        public bool TryWriteNext(T toWriteObject, TimeSpan timeout)
        {
            return this.ObjectPipe.TryAdd(toWriteObject, timeout);
        }

        public bool TryWriteNext(T toWriteObject, int millisecondsTimeout, CancellationToken ct)
        {
            return this.ObjectPipe.TryAdd(toWriteObject, millisecondsTimeout, ct);
        }
    }
}