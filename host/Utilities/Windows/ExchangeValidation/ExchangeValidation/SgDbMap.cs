namespace ExchangeValidation
{
    using System;
    using System.Runtime.InteropServices;

    [StructLayout(LayoutKind.Sequential)]
    public struct SgDbMap
    {
        public string tgtSgPath;
        public string tgtDbPath;
        public string tgtLogDrive;
        public string tgtDbDrive;
    }
}

