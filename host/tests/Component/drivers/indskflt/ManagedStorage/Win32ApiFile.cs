//-----------------------------------------------------------------------
// <copyright file="Win32ApiFile.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
namespace Microsoft.Test.Storages
{
    using System;
    using System.ComponentModel;
    using System.Globalization;
    using System.IO;
    using System.Threading;
    using System.Threading.Tasks;

    using Microsoft.Test.Utils;
    using Microsoft.Test.Utils.NativeMacros;
    using Microsoft.Win32.SafeHandles;
    using System.Runtime.InteropServices;

    // TO DO : Should flush data in the end?!
    // TO DO : Change the name to more appropriate one to avoid exposing implementation.  Say, File
    public class Win32ApiFile : IReadableStorage, IWritableStorage
    {
        public string FileName { get; private set; }
        public ShareModes SharingModes { get; private set; }
        public AccessMasks AccessMasks { get; private set; }
        public CreationDisposition CreationDisposition { get; private set; }
        public FileFlagsAndAttributes FlagsAndAttributes { get; private set; }

        protected static readonly ShareModes DEFAULT_SHARE_MODES = ShareModes.Read | ShareModes.Write;
        protected static readonly AccessMasks DEFAULT_ACCESS_MASKS = AccessMasks.Read | AccessMasks.Write;
        protected static readonly CreationDisposition DEFAULT_CREATE_DISPOSITION = CreationDisposition.OpenAlways;
        protected static readonly FileFlagsAndAttributes DEFAULT_FILE_FLAGS_AND_ATTRIBS = FileFlagsAndAttributes.Normal;

        protected SafeFileHandle fileHandle;
        private bool isSpecialFile;

        #region Constructors

        public Win32ApiFile(string fileName, long? initFileSize)
            : this(fileName, DEFAULT_FILE_FLAGS_AND_ATTRIBS, initFileSize)
        {
        }

        public Win32ApiFile(string fileName, FileFlagsAndAttributes flagsAndAttributes, long? initFileSize)
            : this(fileName, DEFAULT_SHARE_MODES, DEFAULT_ACCESS_MASKS, DEFAULT_CREATE_DISPOSITION, flagsAndAttributes, initFileSize)
        {
        }

        public Win32ApiFile(string fileName, ShareModes shareModes, AccessMasks accessMasks, CreationDisposition creationDisposition, FileFlagsAndAttributes flagsAndAttributes, long? initFileSize)
        {
            this.FileName = fileName;
            this.SharingModes = shareModes;
            this.AccessMasks = accessMasks;
            this.CreationDisposition = creationDisposition;
            this.FlagsAndAttributes = flagsAndAttributes;
            this.isSpecialFile = this.FileName.StartsWith(@"\\.\", StringComparison.Ordinal);

            // For non-special file names, create the directory of the file, if it doesn't exist
            if (!this.isSpecialFile)
            {
                this.FileName = Path.GetFullPath(this.FileName);
                var directoryPath = Path.GetDirectoryName(this.FileName);
                if (!Directory.Exists(directoryPath))
                    Directory.CreateDirectory(directoryPath);
            }

            this.fileHandle = NativeWrapper.CreateFile(
                fileName, this.AccessMasks, this.SharingModes, IntPtr.Zero, this.CreationDisposition, this.FlagsAndAttributes, new SafeFileHandle(IntPtr.Zero, false));

            if (this.fileHandle.IsInvalid)
            {
                string exceptionMessage = string.Format(CultureInfo.InvariantCulture,
                    "Exception occured during CreateFile() native call for file name : {0}", this.FileName);
                throw new Exception(exceptionMessage, new Win32Exception());
            }

            // If the create disposition is set to use an existing file, this will expand or truncate that file
            // If the create disposition is set to overwrite / truncate an existing file or create a new file, at this point
            // the file size will be zero and so trivially this will be a file expansion operation
            if (!this.isSpecialFile && initFileSize != null)
            {
                long newFilePointer;
                if (!NativeWrapper.SetFilePointerEx(this.fileHandle, (long)initFileSize, out newFilePointer, MoveMethod.FileBegin))
                {
                    string exceptionMessage = string.Format(CultureInfo.InvariantCulture,
                        "Exception on trying to set the file pointer to the new EOF (initFileSize) : {0} for file name : {1}",
                        initFileSize, this.FileName);
                    throw new Exception(exceptionMessage, new Win32Exception());
                }

                if (!NativeWrapper.SetEndOfFile(this.fileHandle))
                {
                    string exceptionMessage = string.Format(CultureInfo.InvariantCulture,
                        "Exception on trying to set the new EOF (initFileSize) : {0} for file name : {1}",
                        initFileSize, this.FileName);
                    throw new Exception(exceptionMessage, new Win32Exception());
                }

                // TODO: Set VDL to EOF (Can take the flag for this setting as a constructor parameter)
                // Using VDL would improve random IO perf, since the unallocated blocks are emulated as 0 by the FS (in turn, no I/Os)
            }

            this.IsInAsyncMode = this.FlagsAndAttributes.HasFlag(FileFlagsAndAttributes.Overlapped);

            if (this.IsInAsyncMode)
            {
                // Binds the file handle to thread pool, so that File Handle is associated with Thread pool's I/O Completion Ports
                ThreadPool.BindHandle(this.fileHandle);
            }
        }

        public bool SetFilePointer(long ulOffset)
        {
            long newFilePointer;
            return NativeWrapper.SetFilePointerEx(this.fileHandle, ulOffset, out newFilePointer, MoveMethod.FileBegin);
        }

        #endregion

        #region IReadableStorage, WritableStorage members

        public bool IsInAsyncMode { get; private set; }

        public uint Read(byte[] buffer, uint numberOfBytesToRead, ulong storageOffset)
        {
            return this.PerformIO(IOType.Read, readIOCall, buffer, numberOfBytesToRead, storageOffset);
        }

        public Task<uint> ReadAsync(byte[] buffer, uint numberOfBytesToRead, ulong storageOffset)
        {
            return this.ReadAsync(buffer, numberOfBytesToRead, storageOffset, CancellationToken.None);
        }

        public Task<uint> ReadAsync(byte[] buffer, uint numberOfBytesToRead, ulong storageOffset, CancellationToken ct)
        {
            return this.PerformIOAsync(IOType.Read, readIOCall, buffer, numberOfBytesToRead, storageOffset, ct);
        }

        public uint Write(byte[] buffer, uint numberOfBytesToWrite, ulong storageOffset)
        {
            return this.PerformIO(IOType.Write, writeIOCall, buffer, numberOfBytesToWrite, storageOffset);
        }

        public Task<uint> WriteAsync(byte[] buffer, uint numberOfBytesToWrite, ulong storageOffset)
        {
            return this.WriteAsync(buffer, numberOfBytesToWrite, storageOffset, CancellationToken.None);
        }

        public Task<uint> WriteAsync(byte[] buffer, uint numberOfBytesToWrite, ulong storageOffset, CancellationToken ct)
        {
            return this.PerformIOAsync(IOType.Write, writeIOCall, buffer, numberOfBytesToWrite, storageOffset, ct);
        }

        #endregion

        private enum IOType
        {
            Read,
            Write
        }

        unsafe private delegate bool Win32FileIODelegate(
            SafeFileHandle fileHandle, byte[] buffer, uint numberOfBytesIn, uint* pNumberOfBytesOut, NativeOverlapped* pNativeOvlpd);

        unsafe private static Win32FileIODelegate readIOCall =
            (fileHandle, buffer, numberOfBytesIn, pNumberOfBytesOut, pNativeOvlpd) =>
                NativeWrapper.ReadFile(fileHandle, buffer, numberOfBytesIn, pNumberOfBytesOut, pNativeOvlpd);

        unsafe private static Win32FileIODelegate writeIOCall =
            (fileHandle, buffer, numberOfBytesIn, pNumberOfBytesOut, pNativeOvlpd) =>
                NativeWrapper.WriteFile(fileHandle, buffer, numberOfBytesIn, pNumberOfBytesOut, pNativeOvlpd);

        private uint PerformIO(IOType ioType, Win32FileIODelegate win32IOCall, byte[] buffer, uint numberOfBytesIn, ulong storageOffset)
        {
            if (this.IsInAsyncMode)
                throw new NotSupportedException("File handle is opened in asynchronous mode");

            uint numberOfBytesOut;
            bool success;
            Overlapped ovlpd = new Overlapped((int)storageOffset, (int)(storageOffset >> 32), IntPtr.Zero, null);

            unsafe
            {
                NativeOverlapped* pNativeOvlpd = ovlpd.Pack(null, buffer);

                try
                {
                    success = win32IOCall(this.fileHandle, buffer, numberOfBytesIn, &numberOfBytesOut, pNativeOvlpd);

                    if (!success)
                    {
                        Console.WriteLine("Error No : " + Marshal.GetLastWin32Error());
                        string exceptionMessage = string.Format(CultureInfo.InvariantCulture,
                            "Exception occured during synchronous {0}File() native call for file name : {1}", ioType, this.FileName);
                        throw new Exception(exceptionMessage, new Win32Exception());
                    }
                }
                finally
                {
                    Overlapped.Free(pNativeOvlpd);
                }
            }

            // numberOfBytesOut can be lesser than numberOfBytesIn
            return numberOfBytesOut;
        }

        private Task<uint> PerformIOAsync(IOType ioType, Win32FileIODelegate win32IOCall, byte[] buffer, uint numberOfBytes, ulong storageOffset, CancellationToken ct)
        {
            // TO DO: Investigate on what's the optimal task creation option
            return Task<uint>.Factory.FromAsync(
                BeginPerformIO, EndPerformIO, buffer, numberOfBytes, storageOffset,
                new Tuple<IOType, Win32FileIODelegate, CancellationToken>(ioType, win32IOCall, ct));
        }

        #region Async I/O

        private class Win32APIAsyncIOImpl : IAsyncResult
        {
            public object AsyncState { get; private set; }
            public WaitHandle AsyncWaitHandle { get; private set; }
            public bool CompletedSynchronously { get; private set; }
            public bool IsCompleted { get; private set; }

            private AsyncCallback completionCallback;

            public class AsyncData
            {
                public IOType ioType;
                public Win32Error error;
                public uint numBytes;
            }

            public Win32APIAsyncIOImpl(AsyncCallback completionCallback, IOType ioType)
            {
                this.completionCallback = completionCallback;
                this.AsyncState = new AsyncData() { ioType = ioType };
            }

            public void InformIOCompletion(uint errorCode, uint numBytes, bool operatedSynchronously)
            {
                this.CompletedSynchronously = operatedSynchronously;
                this.IsCompleted = true;
                AsyncData currData = this.AsyncState as AsyncData;
                currData.error = (Win32Error)errorCode;
                currData.numBytes = numBytes;

                if (this.completionCallback != null)
                    this.completionCallback(this);
            }
        }

        private IAsyncResult BeginPerformIO(byte[] buffer, uint numberOfBytesIn, ulong storageOffset, AsyncCallback completionCallback, object state)
        {
            uint numberOfBytesOut;
            bool success;
            var currState = state as Tuple<IOType, Win32FileIODelegate, CancellationToken>;
            IOType ioType = currState.Item1;
            Win32FileIODelegate win32IOCall = currState.Item2;
            CancellationToken ct = currState.Item3;

            // TO DO : Add async I/O cancellation support via CancelIoEx()
            if (!ct.Equals(CancellationToken.None))
                throw new NotImplementedException();

            Win32APIAsyncIOImpl toRetResult = new Win32APIAsyncIOImpl(completionCallback, ioType);
            Overlapped ovlpd = new Overlapped((int)storageOffset, (int)(storageOffset >> 32), IntPtr.Zero, toRetResult);

            unsafe
            {
                NativeOverlapped* pNativeOvlpd = ovlpd.Pack(this.IOCompletionCallback, buffer);
                try
                {
                    success = win32IOCall(this.fileHandle, buffer, numberOfBytesIn, &numberOfBytesOut, pNativeOvlpd);
                }
                catch
                {
                    Overlapped.Free(pNativeOvlpd);
                    throw;
                }

                if (!success)
                {
                    Win32Error lastError = NativeWrapper.GetLastWin32Error();
                    if (lastError != Win32Error.IOPending)
                    {
                        // Error occured
                        Overlapped.Free(pNativeOvlpd);
                        toRetResult.InformIOCompletion((uint)lastError, uint.MaxValue, true);
                    }

                    // If not,
                    // I/O is being performed Asynchronously
                    // Memory pointed by pNativeOvlpd is still in use by system, free it ONLY at the I/O completion callback
                }
                // If not,
                // I/O has been performed Synchronously
                // Memory pointed by pNativeOvlpd is still in use by system, free it ONLY at the I/O completion callback
            }

            return toRetResult;
        }

        private uint EndPerformIO(IAsyncResult ar)
        {
            var currResultData = ar.AsyncState as Win32APIAsyncIOImpl.AsyncData;
            if (currResultData.error != Win32Error.Success)
            {
                string exceptionMessage = string.Format(CultureInfo.InvariantCulture,
                    "Exception occured during asynchronous {0}File() native call executed {2} for file name : {1}",
                    currResultData.ioType, this.FileName, ar.CompletedSynchronously ? "synchronously" : "asynchronously");
                throw new Exception(exceptionMessage, new Win32Exception((int)currResultData.error));
            }

            return currResultData.numBytes;
        }

        // This is the callback invoked by the I/O Completion ports
        unsafe private void IOCompletionCallback(uint errorCode, uint numBytes, NativeOverlapped* pOverlap)
        {
            Overlapped ovlpd = Overlapped.Unpack(pOverlap);
            Win32APIAsyncIOImpl currResult = ovlpd.AsyncResult as Win32APIAsyncIOImpl;
            Overlapped.Free(pOverlap);
            currResult.InformIOCompletion(errorCode, numBytes, false);
        }

        #endregion

        #region Dispose Pattern

        private bool isDisposed = false;
        private object disposingLock = new Object();

        // TO DO : Complete the Dispose pattern by throwing ObjectDisposedException for function calls after Disposal.  Follow the same for anywhere the Dispose pattern is utilized.

        protected virtual void Dispose(bool isDisposing)
        {
            lock (this.disposingLock)
            {
                if (this.isDisposed)
                    return;

                if (isDisposing)
                {
                    if (this.fileHandle != null)
                        this.fileHandle.Dispose();
                }

                this.isDisposed = true;
            }
        }

        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        public void Close()
        {
            this.Dispose();
        }

        ~Win32ApiFile()
        {
            this.Dispose(false);
        }

        #endregion
    }
}