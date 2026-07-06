using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;

namespace HidJsonTesterCs
{
    internal sealed class HidJsonClient
    {
        private const ushort Vid = 0x0483;
        private const ushort Pid = 0x5750;
        private const byte ReportTypeJson = 0x01;
        private const int ReportSize = 64;
        private const int PayloadSize = 59;

        public string SendCommand(string json, int timeoutMs)
        {
            using (HidDevice device = HidDevice.Open(Vid, Pid))
            {
                byte seq = ExtractSeq(json);
                WriteJson(device, seq, json);
                return ReadJson(device, timeoutMs);
            }
        }

        private static void WriteJson(HidDevice device, byte seq, string json)
        {
            byte[] raw = Encoding.UTF8.GetBytes(json);
            int total = Math.Max(1, (raw.Length + PayloadSize - 1) / PayloadSize);

            for (int index = 0; index < total; index++)
            {
                int offset = index * PayloadSize;
                int length = Math.Min(PayloadSize, raw.Length - offset);
                byte[] report = new byte[ReportSize];
                report[0] = ReportTypeJson;
                report[1] = seq;
                report[2] = (byte)index;
                report[3] = (byte)total;
                report[4] = (byte)length;
                Buffer.BlockCopy(raw, offset, report, 5, length);
                device.WriteReport(report);
                Thread.Sleep(2);
            }
        }

        private static string ReadJson(HidDevice device, int timeoutMs)
        {
            DateTime deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);
            Dictionary<int, byte[]> chunks = new Dictionary<int, byte[]>();
            int total = 0;

            while (DateTime.UtcNow < deadline)
            {
                int remainingMs = (int)Math.Max(1, (deadline - DateTime.UtcNow).TotalMilliseconds);
                byte[] report = device.ReadReport(Math.Min(250, remainingMs));
                if (report[0] != ReportTypeJson)
                {
                    continue;
                }

                int index = report[2];
                total = report[3];
                int length = report[4];
                if (length < 0 || length > PayloadSize || total <= 0)
                {
                    continue;
                }

                byte[] payload = new byte[length];
                Buffer.BlockCopy(report, 5, payload, 0, length);
                chunks[index] = payload;

                if (chunks.Count >= total)
                {
                    List<byte> all = new List<byte>();
                    for (int i = 0; i < total; i++)
                    {
                        if (!chunks.ContainsKey(i))
                        {
                            throw new TimeoutException("Response chunk tidak lengkap.");
                        }
                        all.AddRange(chunks[i]);
                    }
                    return Encoding.UTF8.GetString(all.ToArray());
                }
            }

            throw new TimeoutException("Timeout menunggu response HID.");
        }

        private static byte ExtractSeq(string json)
        {
            int p = json.IndexOf("\"seq\"");
            if (p < 0)
            {
                return 1;
            }
            p = json.IndexOf(':', p);
            if (p < 0)
            {
                return 1;
            }
            p++;
            while (p < json.Length && Char.IsWhiteSpace(json[p]))
            {
                p++;
            }
            int start = p;
            while (p < json.Length && Char.IsDigit(json[p]))
            {
                p++;
            }
            int value;
            if (Int32.TryParse(json.Substring(start, p - start), out value))
            {
                return (byte)(value & 0xFF);
            }
            return 1;
        }
    }
}
