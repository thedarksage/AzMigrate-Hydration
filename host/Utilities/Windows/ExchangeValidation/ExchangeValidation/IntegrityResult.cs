namespace ExchangeValidation
{
    using System;
    using System.Runtime.InteropServices;

    [StructLayout(LayoutKind.Sequential)]
    public struct IntegrityResult
    {
        public string logFile;
        public string logIntegrity;
        public string dbFile;
        public string dbIntegrity;
    }
}

