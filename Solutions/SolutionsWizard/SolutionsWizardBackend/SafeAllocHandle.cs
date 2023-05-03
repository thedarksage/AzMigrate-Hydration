using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using Microsoft.Win32.SafeHandles;

namespace Com.Inmage.Security
{
    internal sealed class SafeAllocHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        internal SafeAllocHandle()
            : base(true)
        {
        }

        override protected bool ReleaseHandle()
        {
            // Deallocate the unmanaged memory.
            Marshal.FreeCoTaskMem(handle);
            return true;
        }

        internal static SafeAllocHandle AllocateHandle(object structure)
        {            
            SafeAllocHandle safeAllocHandle = new SafeAllocHandle();
            int structSize = Marshal.SizeOf(structure);
            RuntimeHelpers.PrepareConstrainedRegions();
            try { }
            finally
            {
                IntPtr mem = Marshal.AllocCoTaskMem(structSize);
                safeAllocHandle.SetHandle(mem);
            }
            Marshal.StructureToPtr(structure, safeAllocHandle.handle, false);
            return safeAllocHandle;
        }
    }
}
