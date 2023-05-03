namespace ExchangeValidation
{
    using System;
    using System.Runtime.InteropServices;

    [StructLayout(LayoutKind.Sequential)]
    public struct StorageInfo
    {
        public string storePath;
        public string storeVsnapPath;
        public string logPrefix;
    }
}

