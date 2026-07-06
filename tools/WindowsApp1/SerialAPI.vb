Imports Microsoft.VisualBasic
Imports System.Runtime.InteropServices

Public Class NativeAPI
    <DllImport("cbs64.dll", CallingConvention:=CallingConvention.Cdecl)>
    Public Shared Function OpenSerial(portName As String, baudRate As Integer) As Boolean
    End Function
End Class
