using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace HidJsonTesterCs
{
    internal sealed class HidDevice : IDisposable
    {
        private const int DIGCF_PRESENT = 0x00000002;
        private const int DIGCF_DEVICEINTERFACE = 0x00000010;
        private const uint GENERIC_READ = 0x80000000;
        private const uint GENERIC_WRITE = 0x40000000;
        private const uint FILE_SHARE_READ = 0x00000001;
        private const uint FILE_SHARE_WRITE = 0x00000002;
        private const uint OPEN_EXISTING = 3;

        private SafeFileHandle _handle;
        private FileStream _stream;

        public static HidDevice Open(ushort vendorId, ushort productId)
        {
            string path = FindDevicePath(vendorId, productId);
            if (path == null)
            {
                throw new InvalidOperationException("HID device VID/PID tidak ditemukan.");
            }

            SafeFileHandle handle = CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, IntPtr.Zero, OPEN_EXISTING, 0, IntPtr.Zero);

            if (handle == null || handle.IsInvalid)
            {
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Gagal membuka HID device.");
            }

            HidDevice device = new HidDevice();
            device._handle = handle;
            device._stream = new FileStream(handle, FileAccess.ReadWrite, 65, false);
            return device;
        }

        public void WriteReport(byte[] report64)
        {
            if (report64 == null || report64.Length != 64)
            {
                throw new ArgumentException("Report harus 64 byte.");
            }

            byte[] buffer = new byte[65];
            buffer[0] = 0; // no report ID
            Buffer.BlockCopy(report64, 0, buffer, 1, 64);
            _stream.Write(buffer, 0, buffer.Length);
            _stream.Flush();
        }

        public byte[] ReadReport(int timeoutMs)
        {
            byte[] buffer = new byte[65];
            IAsyncResult asyncResult = _stream.BeginRead(buffer, 0, buffer.Length, null, null);
            if (!asyncResult.AsyncWaitHandle.WaitOne(timeoutMs))
            {
                Dispose();
                throw new TimeoutException("Timeout membaca HID report.");
            }

            int read = _stream.EndRead(asyncResult);
            if (read <= 0)
            {
                throw new IOException("HID read returned 0 byte.");
            }

            byte[] report = new byte[64];
            if (read >= 65 && buffer[0] == 0)
            {
                Buffer.BlockCopy(buffer, 1, report, 0, 64);
            }
            else
            {
                Buffer.BlockCopy(buffer, 0, report, 0, Math.Min(64, read));
            }
            return report;
        }

        public void Dispose()
        {
            if (_stream != null)
            {
                _stream.Dispose();
                _stream = null;
            }
            if (_handle != null)
            {
                _handle.Dispose();
                _handle = null;
            }
        }

        private static string FindDevicePath(ushort vendorId, ushort productId)
        {
            Guid hidGuid;
            HidD_GetHidGuid(out hidGuid);

            IntPtr infoSet = SetupDiGetClassDevs(ref hidGuid, IntPtr.Zero, IntPtr.Zero,
                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
            if (infoSet == new IntPtr(-1))
            {
                throw new Win32Exception(Marshal.GetLastWin32Error(), "SetupDiGetClassDevs failed.");
            }

            try
            {
                SP_DEVICE_INTERFACE_DATA data = new SP_DEVICE_INTERFACE_DATA();
                data.cbSize = Marshal.SizeOf(typeof(SP_DEVICE_INTERFACE_DATA));

                for (uint index = 0; SetupDiEnumDeviceInterfaces(infoSet, IntPtr.Zero, ref hidGuid, index, ref data); index++)
                {
                    int requiredSize = 0;
                    SetupDiGetDeviceInterfaceDetail(infoSet, ref data, IntPtr.Zero, 0, ref requiredSize, IntPtr.Zero);

                    IntPtr detailBuffer = Marshal.AllocHGlobal(requiredSize);
                    try
                    {
                        Marshal.WriteInt32(detailBuffer, IntPtr.Size == 8 ? 8 : 6);
                        if (!SetupDiGetDeviceInterfaceDetail(infoSet, ref data, detailBuffer, requiredSize, ref requiredSize, IntPtr.Zero))
                        {
                            continue;
                        }

                        IntPtr pathPtr = new IntPtr(detailBuffer.ToInt64() + 4);
                        string path = Marshal.PtrToStringAuto(pathPtr);
                        if (MatchesVidPid(path, vendorId, productId))
                        {
                            return path;
                        }
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(detailBuffer);
                    }
                }
            }
            finally
            {
                SetupDiDestroyDeviceInfoList(infoSet);
            }

            return null;
        }

        private static bool MatchesVidPid(string path, ushort vendorId, ushort productId)
        {
            if (path == null)
            {
                return false;
            }

            string text = path.ToLowerInvariant();
            string vid = "vid_" + vendorId.ToString("x4");
            string pid = "pid_" + productId.ToString("x4");
            return text.IndexOf(vid) >= 0 && text.IndexOf(pid) >= 0;
        }

        [DllImport("hid.dll")]
        private static extern void HidD_GetHidGuid(out Guid hidGuid);

        [DllImport("setupapi.dll", SetLastError = true)]
        private static extern IntPtr SetupDiGetClassDevs(ref Guid classGuid, IntPtr enumerator,
            IntPtr hwndParent, int flags);

        [DllImport("setupapi.dll", SetLastError = true)]
        private static extern bool SetupDiEnumDeviceInterfaces(IntPtr deviceInfoSet, IntPtr deviceInfoData,
            ref Guid interfaceClassGuid, uint memberIndex, ref SP_DEVICE_INTERFACE_DATA deviceInterfaceData);

        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr deviceInfoSet,
            ref SP_DEVICE_INTERFACE_DATA deviceInterfaceData, IntPtr deviceInterfaceDetailData,
            int deviceInterfaceDetailDataSize, ref int requiredSize, IntPtr deviceInfoData);

        [DllImport("setupapi.dll", SetLastError = true)]
        private static extern bool SetupDiDestroyDeviceInfoList(IntPtr deviceInfoSet);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern SafeFileHandle CreateFile(string fileName, uint desiredAccess,
            uint shareMode, IntPtr securityAttributes, uint creationDisposition,
            uint flagsAndAttributes, IntPtr templateFile);

        [StructLayout(LayoutKind.Sequential)]
        private struct SP_DEVICE_INTERFACE_DATA
        {
            public int cbSize;
            public Guid InterfaceClassGuid;
            public int Flags;
            public IntPtr Reserved;
        }
    }
}
