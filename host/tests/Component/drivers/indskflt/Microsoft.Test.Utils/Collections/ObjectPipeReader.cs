using System;
using System.Collections.Generic;
using System.Threading;

namespace Microsoft.Test.IOGen.Utils.Collections
{
    public class ObjectPipeReader<T>
    {
        internal ObjectPipe<T> ObjectPipe { get; private set; }

        internal ObjectPipeReader(ObjectPipe<T> objectPipe)
        {
            this.ObjectPipe = objectPipe;
        }

        public bool HasReachedEndOfPipe { get { return this.ObjectPipe.IsCompleted; } }

        public T ReadNext()
        {
            return this.ObjectPipe.Take();
        }

        public T ReadNext(CancellationToken ct)
        {
            return this.ObjectPipe.Take(ct);
        }

        public bool TryReadNext(out T nextObject)
        {
            return this.ObjectPipe.TryTake(out nextObject);
        }

        public bool TryReadNext(out T nextObject, int millisecondsTimeout)
        {
            return this.ObjectPipe.TryTake(out nextObject, millisecondsTimeout);
        }

        public bool TryReadNext(out T nextObject, TimeSpan timeout)
        {
            return this.ObjectPipe.TryTake(out nextObject, timeout);
        }

        public bool TryReadNext(out T nextObject, int millisecondsTimeout, CancellationToken ct)
        {
            return this.ObjectPipe.TryTake(out nextObject, millisecondsTimeout, ct);
        }

        public IEnumerable<T> EnumerateTillEndOfPipe()
        {
            return this.ObjectPipe.GetConsumingEnumerable();
        }

        public IEnumerable<T> EnumerateTillEndOfPipe(CancellationToken ct)
        {
            return this.ObjectPipe.GetConsumingEnumerable(ct);
        }
    }
}