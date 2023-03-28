using Microsoft.Test.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Test.Utils.NativeMacros;
using Microsoft.Win32.SafeHandles;

namespace Microsoft.Test.IOCTLs
{
    /// <summary>
    /// 
    /// </summary>
    public abstract class IOCTLWrapper
    {
        protected SafeFileHandle m_hInmageDevice;
        
        protected string InMageVolumeFilterDevice { get; private set; }

        public static readonly string INMAGE_DISK_FILTER_DOS_DEVICE_NAME = @"\\.\InMageDiskFilter";

        protected static readonly ShareModes DEFAULT_SHARE_MODES = ShareModes.Read | ShareModes.Write;
        protected static readonly AccessMasks DEFAULT_ACCESS_MASKS = AccessMasks.Read | AccessMasks.Write;
        protected static readonly CreationDisposition DEFAULT_CREATE_DISPOSITION = CreationDisposition.OpenExisting;

        protected static readonly FileFlagsAndAttributes DEFAULT_FILE_FLAGS_AND_ATTRIBS = FileFlagsAndAttributes.Normal;


        protected IOCTLWrapper()
            : this(INMAGE_DISK_FILTER_DOS_DEVICE_NAME)
        {
        }

        protected IOCTLWrapper(string inMageVolumeFilterDevice)
            : this(inMageVolumeFilterDevice, DEFAULT_FILE_FLAGS_AND_ATTRIBS)
        {
        }

        protected IOCTLWrapper(string inMageVolumeFilterDevice, FileFlagsAndAttributes flagsAndAttributes)
            : this(inMageVolumeFilterDevice, DEFAULT_SHARE_MODES, DEFAULT_ACCESS_MASKS, DEFAULT_CREATE_DISPOSITION, flagsAndAttributes)
        {
        }

        protected IOCTLWrapper(string inMageVolumeFilterDevice, ShareModes shareModes, AccessMasks accessMasks, CreationDisposition creationDisposition, FileFlagsAndAttributes flagsAndAttributes)
        {
            this.InMageVolumeFilterDevice = inMageVolumeFilterDevice;
            this.m_hInmageDevice = NativeWrapper.CreateFile(inMageVolumeFilterDevice, accessMasks, shareModes, IntPtr.Zero, creationDisposition, flagsAndAttributes, new SafeFileHandle(IntPtr.Zero, false));
            if (this.m_hInmageDevice.IsInvalid)
            {
                string exceptionMessage = string.Format(CultureInfo.InvariantCulture,
                    "Exception occured during CreateFile() native call for file name : {0}", InMageVolumeFilterDevice);
                throw new Exception(exceptionMessage, new Win32Exception());
            }
       }

       public abstract void SendIOCTL(UInt32 IOControlCode, Object input, ref Object output);

       public abstract void SendIOCTLRaw(UInt32 IOControlCode, IntPtr inputBuffer, uint inputBufferSize, IntPtr outputBuffer, uint outputBufferSize, out uint bytesReturned);
    }
}