using System;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing
{
    /// <summary>
    /// Represents a row in the InMageTelemetryPSV2 MDS table. When serialized in
    /// JSON, it will be consumed by the parser in the MARS for upload.
    /// </summary>
    public class InMageTelemetryPSV2Row
    {
        public DateTime PreciseTimeStamp;
        public string CSHostId;
        public string SrcAgentVer;
        public long SysTime;
        public string LastMoveFile;
        public ulong LastMoveFileProcTime;
        public ulong LastMoveFileModTime;
        public ulong SrcFldSize;
        public ulong SrcNumOfFiles;
        public string SrcFirstFile;
        public ulong SrcFF_ModTime;
        public string SrcLastFile;
        public ulong SrcLF_ModTime;
        public ulong SrcNumOfTagFile;
        public string SrcFirstTagFileName;
        public ulong SrcFT_ModTime;
        public string SrcLastTagFileName;
        public ulong SrcLT_ModTime;
        public ulong TgtFldSize;
        public ulong TgtNumOfFiles;
        public string TgtFirstFile;
        public ulong TgtFF_ModTime;
        public string TgtLastFile;
        public ulong TgtLF_ModTime;
        public ulong TgtNumOfTagFile;
        public string TgtFirstTagFileName;
        public ulong TgtFT_ModTime;
        public string TgtLastTagFileName;
        public ulong TgtLT_ModTime;
        public int MessageType;
        public string HostId;
        public string DiskId;
        public string PSHostId;
        public string PsAgentVer;
        public long SysBootTime;
        public long SvAgtHB;
        public long AppAgtHB;
        public long DiffThrotLimit;
        public long ResyncThrotLimit;
        public long ResyncFldSize;
        public decimal VolUsageLimit;
        public long CacheFldSize;
        public ulong UsedVolSize;
        public ulong FreeVolSize;
        public string PairState;

        public static InMageTelemetryPSV2Row BuildDefaultRow()
        {
            var toRet = new InMageTelemetryPSV2Row
            {
                PreciseTimeStamp = DateTime.UtcNow,
                CSHostId = string.Empty,
                SrcAgentVer = string.Empty,
                SysTime = 0,
                MessageType = 3,
                HostId = string.Empty,
                DiskId = string.Empty,
                PSHostId = string.Empty,
                PsAgentVer = string.Empty,
                SysBootTime = 0,
                SvAgtHB = 0,
                AppAgtHB = 0,
                DiffThrotLimit = 0,
                ResyncThrotLimit = 0,
                ResyncFldSize = 0,
                VolUsageLimit = 0,
                CacheFldSize = 0,
                UsedVolSize = 0,
                FreeVolSize = 0,
                LastMoveFile = string.Empty,
                LastMoveFileProcTime = 0,
                SrcFldSize = 0,
                SrcNumOfFiles = 0,
                SrcFirstFile = string.Empty,
                SrcFF_ModTime = 0,
                SrcLastFile = string.Empty,
                SrcLF_ModTime = 0,
                SrcNumOfTagFile = 0,
                SrcFirstTagFileName = string.Empty,
                SrcFT_ModTime = 0,
                SrcLastTagFileName = string.Empty,
                SrcLT_ModTime = 0,
                TgtFldSize = 0,
                TgtNumOfFiles = 0,
                TgtFirstFile = string.Empty,
                TgtFF_ModTime = 0,
                TgtLastFile = string.Empty,
                TgtLF_ModTime = 0,
                TgtNumOfTagFile = 0,
                TgtFirstTagFileName = string.Empty,
                TgtFT_ModTime = 0,
                TgtLastTagFileName = string.Empty,
                TgtLT_ModTime = 0
            };

            return toRet;
        }

        /// <summary>
        /// Wrapper class around the MDS rows to simulate the JSON objects from source
        /// </summary>
        internal class InMageLogUnit
        {
            public object Map;
        }
    }
}
