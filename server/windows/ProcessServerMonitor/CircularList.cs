using System;
using System.Collections;
using System.Collections.Generic;

namespace ProcessServerMonitor
{
    internal class CircularList<T> : IEnumerable<T>, IEnumerator<T>
    {
        private T[] records;
        private int index;
        private bool rotated;
        private int enumIndex;

        /// <summary>
        /// Constructor that initializes the list
        /// with given number of elements.
        /// </summary>
        public CircularList(int numItems)
        {
            if (numItems <= 0)
            {
                throw new ArgumentOutOfRangeException("numItems", "Number of items should be greater than 0");
            }

            records = new T[numItems];
            index = 0;
            rotated = false;
            enumIndex = -1;
        }

        /// <summary>
        /// Gets or Sets the record value at the current index.
        /// </summary>
        public T CurrentValue
        {
            get { return records[index]; }
            set { records[index] = value; }
        }

        public T LatestValue
        {
            get { return (index - 1 == -1) ? records[Count - 1] : records[index - 1]; }
        }

        /// <summary>
        /// Returns the count of the number of added records, up to
        /// and including the total number of records in the collection.
        /// </summary>
        public int Count
        {
            get { return rotated ? records.Length : index; }
        }

        /// <summary>
        /// Returns the length of the list.
        /// </summary>
        public int Length
        {
            get { return records.Length; }
        }

        /// <summary>
        /// Gets/sets the value at the specified index.
        /// </summary>
        public T this[int index]
        {
            get
            {
                RangeCheck(index);
                return records[index];
            }
            set
            {
                RangeCheck(index);
                records[index] = value;
            }
        }

        /// <summary>
        /// Moves to the next item or wraps to the first item.
        /// </summary>
        public void Next()
        {
            if (++index == records.Length)
            {
                index = 0;
                rotated = true;
            }
        }

        /// <summary>
        /// Clears the list, resets the current index to the 
        /// beginning of the list and resets the rotated flag to false.
        /// </summary>
        public void Clear()
        {
            index = 0;
            records.Initialize();
            rotated = false;
        }

        /// <summary>
        /// Internal indexer range check helper.  Throws
        /// ArgumentOutOfRange exception if the index is not valid.
        /// </summary>
        protected void RangeCheck(int index)
        {
            if (index < 0)
            {
                throw new ArgumentOutOfRangeException("index", "Indexer cannot be less than 0.");
            }

            if (index >= records.Length)
            {
                throw new ArgumentOutOfRangeException("index", "Indexer cannot be greater than or equal to the number if records in the collection.");
            }
        }

        // Interface implementations

        /* TODO : sisunkar : 1905. Return an enumerator object each time they are called.
         * The enumerator object must contain the enumIndex in them and throw if the collection is modified.
         * This is needed if the data structure would be used in outer and inner loops as well
         * as to be iterated in multiple threads
         */
        public IEnumerator<T> GetEnumerator()
        {
            return this;
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return this;
        }

        public T Current
        {
            get { return this[enumIndex]; }
        }

        public void Dispose()
        {
        }

        object IEnumerator.Current
        {
            get { return this[enumIndex]; }
        }

        // TODO: sisunkar - 1905 : MoveNext() and Reset() should throw
        // InvalidOperationException(), if the collection was modified
        public bool MoveNext()
        {
            ++enumIndex;
            bool ret = enumIndex < Count;

            /* TODO : sisunkar - 1904:
             * Enumerators don't reset themselves. According to the pattern, the user has to invoke Reset() explicitly and until then, return false. Refer https://docs.microsoft.com/en-us/dotnet/api/system.collections.ienumerator.movenext?view=netframework-4.7.2#System_Collections_IEnumerator_MoveNext
             */
            if (!ret)
            {
                Reset();
            }

            return ret;
        }

        public void Reset()
        {
            enumIndex = -1;
        }
    }
}