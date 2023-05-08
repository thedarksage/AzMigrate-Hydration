using Microsoft.Win32.SafeHandles;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Test.IOCTLs
{
    public class IOCTLStructures
    {
        /// <summary>          
        /// To get Driver version.          
        /// </summary>          
        [StructLayout(LayoutKind.Sequential)]
        public struct DRIVER_VERSION
        {
            public ushort ulDrMajorVersion;
            public ushort ulDrMinorVersion;
            public ushort ulDrMinorVersion2;
            public ushort ulDrMinorVersion3;
            public ushort ulPrMajorVersion;
            public ushort ulPrMinorVersion;
            public ushort ulPrMinorVersion2;
            public ushort ulPrBuildNumber;
        }


        [Flags]
        public enum ShutDownNotifyFlags : uint
        {
            /// <summary>
            /// To disable filtering in data mode
            /// </summary>
            SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING = 0x00000001,

            /// <summary>
            /// To disable filtering in data file mode
            /// </summary>
            SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILES = 0x00000002
        }

        /// <summary>          
        /// To register svagents with driver         
        /// </summary>          
        [StructLayout(LayoutKind.Sequential)]
        public struct SHUTDOWN_NOTIFY_INPUT
        {
            /// <summary>
            /// Flags to tack the state.
            /// </summary>
            public ShutDownNotifyFlags Flags;

            /// <summary>              
            /// For alignment purposes.              
            /// </summary>  
            public uint Reserved;
        }

        [Flags]
        public enum ShutDownNotifyDataFlags : uint
        {
            SND_FLAGS_COMMAND_SENT = 0x0001,
            SND_FLAGS_COMMAND_COMPLETED = 0x0002
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct SHUTDOWN_NOTIFY_DATA
        {
            /// <summary>
            /// Handle of the driver
            /// </summary>
            public SafeFileHandle VFCtrlDevice;

            /// <summary>
            /// Specifies the event passed in overlapped IO for completion notification
            /// </summary>
            public IntPtr Event;

            /// <summary>
            /// Flags to tack the state.
            /// </summary>
            public ShutDownNotifyDataFlags Flags;

            /// <summary>
            /// To send overlapped IO to driver for shtudown notification.
            /// </summary>
            public NativeOverlapped Overlapped;
        }

        [Flags]
        public enum ProcessStartNotifyFlags : uint
        {
            PROCESS_START_NOTIFY_INPUT_FLAGS_DATA_FILES_AWARE = 0x0001,
            PROCESS_START_NOTIFY_INPUT_FLAGS_64BIT_PROCESS = 0x0002
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct PROCESS_START_NOTIFY_INPUT
        {
            /// <summary>
            /// Flags to tack the state.
            /// </summary>
            public ProcessStartNotifyFlags Flags;

            /// <summary>              
            /// For alignment purposes.              
            /// </summary>  
            public uint Reserved;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CLEAR_DIFFERENTIALS_DATA
        {
            public Guid VolumeGuid;
            public string MountPoint;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct START_FILTERING_DATA
        {
            public Guid VolumeGuid;
            public string MountPoint;
        }

        [Flags]
        public enum StopFilteingFlags
        {
            STOP_FILTERING_FLAGS_DELETE_BITMAP = 0x0001,
            STOP_ALL_FILTERING_FLAGS = 0x0002
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct STOP_FILTERING_DATA
        {
            public Guid VolumeGuid;
            public StopFilteingFlags Flags;
            public uint Reserved;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct RESYNC_START_INPUT
        {
            public Guid VolumeGuid;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct RESYNC_START_OUTPUT
        {
            public ulong TimeInHundNanoSecondsFromJan1601;
            public ulong SequenceNumber;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct RESYNC_END_INPUT
        {
            public Guid VolumeGuid;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct RESYNC_END_OUTPUT
        {
            public ulong TimeInHundNanoSecondsFromJan1601;
            public ulong SequenceNumber;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct TSSNSEQID
        {
            public ulong ContinuationId;
            public ulong SequenceNumber;
            public ulong TimeStamp;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct TSSNSEQIDV2
        {
            public ulong ContinuationId;
            public ulong SequenceNumber;
            public ulong TimeStamp;
        }


        [StructLayout(LayoutKind.Sequential)]
        public struct  VOLUME_DB_EVENT_INFO
        {
            public Guid        VolumeGuid;
            public SafeFileHandle      hEvent;                             // 0x48
                                                            // 0x50 for 64Bit 0x4C for 32bit
        } 

        [StructLayout(LayoutKind.Sequential)]
        public struct GET_DB_TRANS_DATA
        {
            public Guid VolumeGuid;
            public bool CommitPreviousTrans;
            public bool CommitCurrentTrans;
            public bool PrintDetails;
            public bool PollForDirtyBlocks;
            public bool WaitIfTSO;
            public bool ResetResync;
            public ulong LevelOfDetail;
            public ulong WaitTimeBeforeCurrentTransCommit;
            public ulong RunTimeInMilliSeconds;
            public ulong PollIntervalInMilliSeconds;
            public ulong NumDirtyBlocks;
            public string MountPoint;
            // Required for Sync
            public bool sync;
            public bool resync_req;
            public bool skip_step_one;
            public uint BufferSize;
            public SafeFileHandle FileSource;
            public string SourceName;
            public SafeFileHandle FileDest;
            public string DestName;
            public Guid DestVolumeGUID;
            public string DestMountPoint;
            TSSNSEQID FCSequenceNumber;
            TSSNSEQID LCSequenceNumber;
            TSSNSEQIDV2 FCSequenceNumberV2;
            TSSNSEQIDV2 LCSequenceNumberV2;
            string chpBuff;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct COMMIT_DB_TRANS_DATA
        {
            public Guid VolumeGuid;
            public string MountPoint;
            public bool ResetResync;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct SET_RESYNC_REQUIRED_DATA
        {
            public Guid VolumeGuid;
            public string MountPoint;
            public ulong ErrorCode;
            public ulong ErrorStatus;
        }

        [Flags]
        public enum CommitTransactionFlags
        {
            COMMIT_TRANSACTION_FLAG_RESET_RESYNC_REQUIRED_FLAG = 0x00000001
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct COMMIT_TRANSACTION
        {
            public Guid VolumeGuid;                     // 0x00
            public ulong TransactionID;                 // 0x48
            public CommitTransactionFlags Flags;        // 0x50
            public int Reserved;                        // 0x54
        }

        [Flags]
        public enum DirtyBlockFlags : uint
        {
            UDIRTY_BLOCK_FLAG_START_OF_SPLIT_CHANGE = 0x00000001,
            UDIRTY_BLOCK_FLAG_PART_OF_SPLIT_CHANGE = 0x00000002,
            UDIRTY_BLOCK_FLAG_END_OF_SPLIT_CHANGE = 0x00000004,
            UDIRTY_BLOCK_FLAG_DATA_FILE = 0x00000008,
            UDIRTY_BLOCK_FLAG_SVD_STREAM = 0x00000010,
            UDIRTY_BLOCK_FLAG_TSO_FILE = 0x00000020,

            UDIRTY_BLOCK_FLAG_VOLUME_RESYNC_REQUIRED = 0x80000000,

            UDIRTY_BLOCK_HEADER_SIZE = 0x200,        // 512 Bytes
            UDIRTY_BLOCK_MAX_ERROR_STRING_SIZE = 0x80,   // 128 Bytes
            UDIRTY_BLOCK_TAGS_SIZE = 0xE00,  // 7 * 512 Bytes
            UDIRTY_BLOCK_MAX_FILE_NAME = 0x6FF
        }

        [Flags]
        public enum etWriteOrderState
        {
            ecWriteOrderStateUnInitialized = 0,
            ecWriteOrderStateBitmap = 1,
            ecWriteOrderStateMetadata = 2,
            ecWriteOrderStateData = 3
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct STREAM_REC_HDR_4B
        {
            ushort usStreamRecType;    // 0x00
            int ucFlags;            // 0x02
            int ucLength;           // 0x03 Length includes size of this header too.
            // 0x04
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct TIME_STAMP_TAG_V2
        {
            STREAM_REC_HDR_4B Header;
            ulong Reserved;
            ulong ullSequenceNumber;
            ulong TimeInHundNanoSecondsFromJan1601;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct DATA_SOURCE_TAG
        {
            STREAM_REC_HDR_4B Header;         // 0x00
            ulong ulDataSource;           // 0x04
        }

        // Common structure name used earlier but as part of supporting Global Sequence number for 32-bit systems
        // it has made to use V2 structures and kept other structure for backward compatiablility.
        [StructLayout(LayoutKind.Sequential)]
        public struct UDIRTY_BLOCK_V2
        {
            struct header
            {
                public int TransactionID;
                public ulong ulicbChanges;
                public int cChanges;
                public int ulTotalChangesPending;
                public int ulicbTotalChangesPending;
                public DirtyBlockFlags ulFlags;
                public int ulSequenceIDforSplitIO;
                public int ulBufferSize;
                public int usMaxNumberOfBuffers;
                public int usNumberOfBuffers;
                public int ulcbChangesInStream;
                public int ulcbBufferArraySize;
                // resync flags
                public int ulOutOfSyncCount;
                public string ErrorStringForResync;
                public int ulOutOfSyncErrorCode;
                public int liOutOfSyncTimeStamp;
                public etWriteOrderState eWOState;
                public int ulDataSource;
                public int ullPrevEndSequenceNumber;
                public int ullPrevEndTimeStamp;
                public int ulPrevSequenceIDforSplitIO;
            }
            public int BufferReservedForHeader;
            struct TagList
            {
                STREAM_REC_HDR_4B TagStartOfList;
                STREAM_REC_HDR_4B TagPadding;
                TIME_STAMP_TAG_V2 TagTimeStampOfFirstChange;
                TIME_STAMP_TAG_V2 TagTimeStampOfLastChange;
                DATA_SOURCE_TAG TagDataSource;
                STREAM_REC_HDR_4B TagEndOfList;
            }
            struct DataFile
            {
                public int usLength; // Filename length in bytes not including NULL
                public string FileName;
            }
            
            public string BufferForTags;
            public ulong ChangeOffsetArray;
            public ulong ChangeLengthArray;
            public ulong TimeDeltaArray;
            public ulong SequenceNumberDeltaArray;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct PENDING_TRANSACTION
        {
            //LIST_ENTRY  ListEntry;
            public ulong ullTransactionID;
            public bool bResetResync;
            public Guid VolumeGUID;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct PRINT_STATISTICS_DATA
        {
            public Guid VolumeGUID;
            public string MountPoint;
        }
        public struct ADDITIONAL_STATS_INFO
        {
            public Guid VolumeGuid;
            public string MountPoint;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct GLOBAL_TIMESTAMP
        {
            public ulong TimeInHundNanoSecondsFromJan1601;
            public ulong ullSequenceNumber;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct GET_VOLUME_SIZE
        {
            public Guid VolumeGuid;
            public string MountPoint;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct GET_VOLUME_FLAGS
        {
            public Guid VolumeGuid;
            public string MountPoint;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct GET_FILTERING_STATE
        {
            public Guid VolumeGuid;
            public string MountPoint;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct DATA_MODE_DATA
        {
            public Guid VolumeGuid;
            public string MountPoint;
        }

        [Flags]
        public enum NOTIFY_SYSTEM_STATE_FLAGS : uint
        {
            ssFlagsAreBitmapFilesEncryptedOnDisk =  1 << 0
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct NOTIFY_SYSTEM_STATE_INPUT
        {
            public UInt32 Flags;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NOTIFY_SYSTEM_STATE_OUTPUT
        {
            public UInt32 ResultFlags;
        }
    }
}