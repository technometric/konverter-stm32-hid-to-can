Imports System.Runtime.InteropServices

Public Module SerialAPI
    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Public Function OpenSerial(portName As String, baudRate As UInteger) As Boolean
    End Function
    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Public Sub CloseSerial()
    End Sub
    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Private Function getMonitoring(id As Integer) As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Private Function readMonitoring(id As Integer) As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Private Function sendDataDummy(jsonDummy As String) As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Public Function handleAksi(id As Integer, aksi As String) As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Private Function getVersion() As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Public Function openStm32Converter() As Boolean
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Public Sub closeStm32Converter()
    End Sub

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Private Function stm32SendJson(jsonText As String) As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Private Function stm32Ping() As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Private Function stm32GetSensor(node As Integer, sensor As String, channel As Integer) As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Private Function stm32GetAll(node As Integer) As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Private Function stm32ScanAll() As IntPtr
    End Function

    Private Function PtrToAnsiString(ptr As IntPtr) As String
        If ptr = IntPtr.Zero Then
            Return "Nothing"
        End If
        Return Marshal.PtrToStringAnsi(ptr)
    End Function

    Public Function GetVersi() As String
        Dim ptr As IntPtr = getVersion()
        Return PtrToAnsiString(ptr)
    End Function

    Public Function GetMonitoringJson(id As Integer) As String
        Dim ptr As IntPtr = getMonitoring(id)
        Return PtrToAnsiString(ptr)
    End Function

    Public Function ReadMonitoringJson(id As Integer) As String
        Dim ptr As IntPtr = readMonitoring(id)
        Return PtrToAnsiString(ptr)
    End Function

    Public Function sendAksi(id As Integer, aksi As String) As String
        Dim ptr As IntPtr = handleAksi(id, aksi)
        Return PtrToAnsiString(ptr)
    End Function


    Public Function SendDummy(jsonDummy As String) As String
        Dim ptr As IntPtr = sendDataDummy(jsonDummy)
        Return PtrToAnsiString(ptr)
    End Function

    Public Function OpenStm32ConverterDevice() As Boolean
        Return openStm32Converter()
    End Function

    Public Sub CloseStm32ConverterDevice()
        closeStm32Converter()
    End Sub

    Public Function Stm32SendJsonText(jsonText As String) As String
        Dim ptr As IntPtr = stm32SendJson(jsonText)
        Return PtrToAnsiString(ptr)
    End Function

    Public Function Stm32PingJson() As String
        Dim ptr As IntPtr = stm32Ping()
        Return PtrToAnsiString(ptr)
    End Function

    Public Function Stm32GetSensorJson(node As Integer, sensor As String, Optional channel As Integer = 1) As String
        Dim ptr As IntPtr = stm32GetSensor(node, sensor, channel)
        Return PtrToAnsiString(ptr)
    End Function

    Public Function Stm32GetAllJson(node As Integer) As String
        Dim ptr As IntPtr = stm32GetAll(node)
        Return PtrToAnsiString(ptr)
    End Function

    Public Function Stm32ScanAllJson() As String
        Dim ptr As IntPtr = stm32ScanAll()
        Return PtrToAnsiString(ptr)
    End Function

    ' ===============================
    ' 1️⃣ Delegate Callback Definitions
    ' ===============================
    <UnmanagedFunctionPointer(CallingConvention.StdCall)>
    Public Delegate Sub OnCommandSentCallback(command As String, hexData As String)

    <UnmanagedFunctionPointer(CallingConvention.StdCall)>
    Public Delegate Sub OnResponseReceivedCallback(command As String, response As String)

    <UnmanagedFunctionPointer(CallingConvention.StdCall)>
    Public Delegate Sub OnProgressCallback(current As Integer, total As Integer, status As String)

    <UnmanagedFunctionPointer(CallingConvention.StdCall)>
    Public Delegate Sub OnErrorCallback(errorMsg As String)

    <UnmanagedFunctionPointer(CallingConvention.StdCall)>
    Public Delegate Sub OnRetryCallback(command As String, retryCount As Integer, maxRetries As Integer)

    ' ===============================
    ' 2️⃣ Struct UploadCallbacks (HARUS SAMA URUTANNYA DENGAN C++)
    ' ===============================
    <StructLayout(LayoutKind.Sequential)>
    Public Structure UploadCallbacks
        Public onCommandSent As OnCommandSentCallback
        Public onResponseReceived As OnResponseReceivedCallback
        Public onProgress As OnProgressCallback
        Public onError As OnErrorCallback
        Public onRetry As OnRetryCallback
    End Structure

    <StructLayout(LayoutKind.Sequential)>
    Public Structure DownloadCallbacks
        Public onCommandSent As OnCommandSentCallback
        Public onResponseReceived As OnResponseReceivedCallback
        Public onProgress As OnProgressCallback
        Public onError As OnErrorCallback
        Public onRetry As OnRetryCallback
    End Structure
    ' ===============================
    ' 3️⃣ Import Fungsi dari DLL
    ' ===============================
    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Public Function handleUploadWithCallback(
        ByVal jsonPayload As String,
        ByRef callbacks As UploadCallbacks
    ) As IntPtr
    End Function

    <DllImport("cbs32.dll", CallingConvention:=CallingConvention.StdCall, CharSet:=CharSet.Ansi)>
    Public Function handleDownloadWithCallback(
        ByVal jsonPayload As String,
        ByRef callbacks As DownloadCallbacks
    ) As Integer
    End Function

End Module

