using Microsoft.Test.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Win32.SafeHandles;

namespace Microsoft.Test.IOCTLs
{
    public class IOCTLController : IOCTLWrapper
    {
        // TODO-SanKumar-1806: Add support for any size arrays in Input and/or Outputs.
        // i.e. SizeOf(T) != buffer size given by the user. One possibility is to
        // take extra bytes to copy in the end as a parameter.

        public override void SendIOCTL(UInt32 IOControlCode, Object input, ref Object output)
        {
            IntPtr inputBuffer = IntPtr.Zero, outputBuffer = IntPtr.Zero;
            int inBuffSize = 0, outBuffSize = 0;

            try
            {
                if (input != null)
                {
                    inBuffSize = Marshal.SizeOf(input);
                    inputBuffer = Marshal.AllocHGlobal(inBuffSize);
                    Marshal.StructureToPtr(input, inputBuffer, fDeleteOld: false);
                }

                if (output != null)
                {
                    outBuffSize = Marshal.SizeOf(output);
                    outputBuffer = Marshal.AllocHGlobal(outBuffSize);
                }

                uint bytesReturned;
                SendIOCTLRaw(IOControlCode, inputBuffer, (uint)inBuffSize, outputBuffer, (uint)outBuffSize, out bytesReturned);

                if (output != null)
                    output = Marshal.PtrToStructure(outputBuffer, output.GetType());
            }
            finally
            {
                Marshal.FreeHGlobal(inputBuffer);
                Marshal.FreeHGlobal(outputBuffer);
            }
        }

        public override void SendIOCTLRaw(UInt32 IOControlCode, IntPtr inputBuffer, uint inputBufferSize, IntPtr outputBuffer, uint outputBufferSize, out uint bytesReturned)
        {
            try
            {
                unsafe
                {
                    if (!NativeWrapper.DeviceIoControl(
                        m_hInmageDevice,
                        IOControlCode,
                        inputBuffer,
                        inputBufferSize,
                        outputBuffer,
                        outputBufferSize,
                        out bytesReturned,
                        null))
                    {
                        throw new Win32Exception();
                    }
                }
            }
            catch (Win32Exception we)
            {
                Console.WriteLine("Error No : {0}", we.ErrorCode);
                Console.WriteLine("ErrorMessage {0}", we.Message);
                throw;
            }
            catch (Exception ex)
            {
                throw new Exception("Failed to invoke DeviceIoControl " + IOControlCode, ex);
            }
        }
    }
}