using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Net;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native
{
    /// <summary>
    ///   Wrapper for API's in iphlpapi.dll
    /// </summary>
    internal class NetworkApi : IDisposable
    {
        private bool m_disposed = false;

        // IPV4 Address
        private const int AF_INET = 2;
        private const int ERROR_INSUFFICIENT_BUFFER = 122;

        public enum TCP_TABLE_CLASS
        {
            TCP_TABLE_BASIC_LISTENER,
            TCP_TABLE_BASIC_CONNECTIONS,
            TCP_TABLE_BASIC_ALL,
            TCP_TABLE_OWNER_PID_LISTENER,
            TCP_TABLE_OWNER_PID_CONNECTIONS,
            TCP_TABLE_OWNER_PID_ALL,
            TCP_TABLE_OWNER_MODULE_LISTENER,
            TCP_TABLE_OWNER_MODULE_CONNECTIONS,
            TCP_TABLE_OWNER_MODULE_ALL
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct MIB_TCPROW_OWNER_MODULE
        {
            [MarshalAs(UnmanagedType.U4)]
            private uint state;

            [MarshalAs(UnmanagedType.U4)]
            private uint localAddress;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
            private byte[] localPort;

            [MarshalAs(UnmanagedType.U4)]
            private uint remoteAddress;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
            private byte[] remotePort;

            [MarshalAs(UnmanagedType.U4)]
            private uint processId;

            private ulong createTimestamp;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
            private ulong[] owningModuleInfo;

            public TcpState State
            {
                get { return (TcpState)state; }
            }

            public IPAddress LocalIPAddress
            {
                get { return new IPAddress(localAddress); }
            }

            public ushort LocalPort
            {
                get
                {
                    return BitConverter.ToUInt16(
                        new byte[2] { localPort[1], localPort[0] }, 0);
                }
            }

            public IPAddress RemoteIPAddress
            {
                get { return new IPAddress(remoteAddress); }
            }

            public ushort RemotePort
            {
                get
                {
                    return BitConverter.ToUInt16(
                        new byte[2] { remotePort[1], remotePort[0] }, 0);
                }
            }

            public uint ProcessId
            {
                get { return processId; }
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct MIB_TCPTABLE_OWNER_MODULE
        {
            public uint dwNumEntries;
            private MIB_TCPROW_OWNER_MODULE table;
        }

        /// <summary>
        /// Gets all listening TCP endpoint details on local computer
        /// </summary>
        /// <param name="sorted">Flag to sort</param>
        /// <returns>List of TCP endpoint details</returns>
        public static List<TcpData> GetListeningTCPConnections(bool sorted)
        {
            List<TcpData> tcpInfo = new List<TcpData>();

            IntPtr tcpTable = IntPtr.Zero;
            int tcpTableLength = 0;

            var ret = NativeMethods.GetExtendedTcpTable(
                tcpTable, ref tcpTableLength, sorted,
                AF_INET, TCP_TABLE_CLASS.TCP_TABLE_OWNER_MODULE_LISTENER, 0);

            if (ret != 0 && ret == ERROR_INSUFFICIENT_BUFFER)
            {
                try
                {
                    tcpTable = Marshal.AllocHGlobal(tcpTableLength);

                    if (NativeMethods.GetExtendedTcpTable(
                        tcpTable, ref tcpTableLength, sorted, AF_INET,
                        TCP_TABLE_CLASS.TCP_TABLE_OWNER_MODULE_LISTENER, 0) == 0)
                    {
                        MIB_TCPTABLE_OWNER_MODULE table =
                            (MIB_TCPTABLE_OWNER_MODULE)Marshal.PtrToStructure(
                                tcpTable, typeof(MIB_TCPTABLE_OWNER_MODULE));

                        IntPtr rowPtr = new IntPtr(
                            (long)tcpTable + (long)Marshal.OffsetOf(
                                typeof(MIB_TCPTABLE_OWNER_MODULE), "table"));

                        for (int i = 0; i < table.dwNumEntries - 1; i++)
                        {
                            var row = (MIB_TCPROW_OWNER_MODULE)Marshal.PtrToStructure(
                                rowPtr, typeof(MIB_TCPROW_OWNER_MODULE));

                            tcpInfo.Add(new TcpData(row));

                            // Move to the next entry
                            rowPtr = new IntPtr(
                                rowPtr.ToInt64() + Marshal.SizeOf(typeof(MIB_TCPROW_OWNER_MODULE)));
                        }
                    }
                    else
                    {
                        int lastError = Marshal.GetLastWin32Error();
                        var win32Ex = new Win32Exception(lastError);
                        Tracers.Misc.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Failed to fetch information of TCP connections with error {0} - {1}",
                            lastError,
                            win32Ex.Message);
                    }
                }
                finally
                {
                    if (tcpTable != IntPtr.Zero)
                    {
                        // Free allocated memory space
                        Marshal.FreeHGlobal(tcpTable);
                        tcpTable = IntPtr.Zero;
                    }
                }
            }
            else
            {
                Tracers.Misc.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Failed to get TCP listening connections with return code {0}",
                            ret);
            }

            return tcpInfo;
        }

        public class TcpData
        {
            private readonly Process m_process;

            public IPEndPoint LocalEndPoint { get; private set; }

            public IPEndPoint RemoteEndPoint { get; private set; }

            public TcpState State { get; private set; }

            public int ProcessId { get { return m_process.Id; } }

            public string ProcessName { get { return m_process.ProcessName; } }

            public TcpData(MIB_TCPROW_OWNER_MODULE row)
            {
                m_process = Process.GetProcessById((int)row.ProcessId);
                State = row.State;
                LocalEndPoint = new IPEndPoint(row.LocalIPAddress, row.LocalPort);
                RemoteEndPoint = new IPEndPoint(row.RemoteIPAddress, row.RemotePort);
            }
        }

        public static class NativeMethods
        {
            [DllImport("iphlpapi.dll", SetLastError = true)]
            public static extern uint GetExtendedTcpTable(
                IntPtr pTcpTable,
                ref int dwOutBufLen,
                bool sort,
                int ipVersion,
                TCP_TABLE_CLASS tblClass,
                uint reserved = 0);
        }

        #region IDisposable Support

        protected virtual void Dispose(bool disposing)
        {
            if(m_disposed)
                return;

            if (disposing) { }

            m_disposed = true;
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        #endregion
    }
}
