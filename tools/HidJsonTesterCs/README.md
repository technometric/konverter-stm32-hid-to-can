# HID JSON Tester C#

Visual Studio WinForms tester untuk USB Custom HID STM32.

- Target: .NET Framework 4.0
- Platform: x86
- Cocok untuk Windows XP SP3, Windows 7, dan Windows baru
- Tanpa NuGet / library eksternal
- Akses HID langsung via `hid.dll`, `setupapi.dll`, `kernel32.dll`

Device default:

```text
VID = 0x0483
PID = 0x5750
Report = 64 byte tanpa Report ID
Windows write buffer = 65 byte, byte pertama 0x00
```

Buka `HidJsonTesterCs.sln` di Visual Studio, build `Release|x86`, lalu jalankan.
