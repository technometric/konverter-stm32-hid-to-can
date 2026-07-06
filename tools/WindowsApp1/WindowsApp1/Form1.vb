Imports System.Runtime.InteropServices
Imports Newtonsoft.Json
Imports Newtonsoft.Json.Linq
Public Class Form1
    Dim hold = False
    Dim commStatus As Integer
    Dim uploadCb As New SerialAPI.UploadCallbacks
    Private btnStm32Connect As Button
    Private btnStm32Ping As Button
    Private btnStm32GetAll As Button
    Private btnStm32ScanAll As Button
    Private cmbStm32Sensor As ComboBox
    Private numStm32Pt100Channel As NumericUpDown
    Private btnStm32GetSensor As Button
    Private btnStm32GetId As Button
    Private btnStm32Activate As Button
    Private btnStm32License As Button

    Sub Delay(ms As Integer)
        Dim t As DateTime = DateTime.Now.AddMilliseconds(ms)
        Do While DateTime.Now < t
            Application.DoEvents()
        Loop
    End Sub

    Private Sub Button1_Click(sender As Object, e As EventArgs) Handles Button1.Click
        commStatus = OpenSerial(TextBox3.Text, 115200)
        TextBox2.Text = $"Com Status {commStatus}"
        If commStatus = 0 Then
            Dim strVersi = GetVersi()
            TextBox2.Text = strVersi
            Dim root As JObject = JObject.Parse(strVersi)
            Try
                Dim versi As String = root("versi").ToString()
                Label1.Text = $"Connected to Modul versi {versi}"
            Catch ex As Exception
                ' Menangani error saat runtime
                Label1.Text = "Gagal membaca versi modul."
            End Try

            Button1.Enabled = False
                Button2.Enabled = True
            Button3.Enabled = True
            Button4.Enabled = True
            Button5.Enabled = True
            Button6.Enabled = True
            Button7.Enabled = True
            Button8.Enabled = True
            Button9.Enabled = True
            Button10.Enabled = True
        Else
            Label1.Text = "Disconnected"
        End If
    End Sub

    Private Sub Button2_Click(sender As Object, e As EventArgs) Handles Button2.Click
        If commStatus = 0 Then
            commStatus = 1
            Label1.Text = "Disconnected"
            Button1.Enabled = True
            Button2.Enabled = False
            Button3.Enabled = False
            Button4.Enabled = False
            Button5.Enabled = False
            Button6.Enabled = False
            Button7.Enabled = False
            Button8.Enabled = False
            Button9.Enabled = False
            CloseSerial()
            CloseStm32ConverterDevice()
            Button10.Enabled = False
        End If
    End Sub

    Private Sub Form1_Load(sender As Object, e As EventArgs) Handles MyBase.Load
        Button2.Enabled = False
        Button3.Enabled = False
        Button4.Enabled = False
        Button5.Enabled = False
        Button6.Enabled = False
        Button7.Enabled = False
        Button8.Enabled = False
        Button9.Enabled = False
        Button10.Enabled = False
        AddStm32Buttons()
    End Sub

    Private Sub AddStm32Buttons()
        btnStm32Connect = New Button()
        btnStm32Connect.Location = New Point(400, 46)
        btnStm32Connect.Size = New Size(105, 23)
        btnStm32Connect.Text = "STM32 Connect"
        AddHandler btnStm32Connect.Click, AddressOf BtnStm32Connect_Click
        Controls.Add(btnStm32Connect)

        btnStm32Ping = New Button()
        btnStm32Ping.Location = New Point(511, 46)
        btnStm32Ping.Size = New Size(86, 23)
        btnStm32Ping.Text = "STM32 Ping"
        AddHandler btnStm32Ping.Click, AddressOf BtnStm32Ping_Click
        Controls.Add(btnStm32Ping)

        btnStm32GetAll = New Button()
        btnStm32GetAll.Location = New Point(400, 75)
        btnStm32GetAll.Size = New Size(105, 23)
        btnStm32GetAll.Text = "STM32 GetAll"
        AddHandler btnStm32GetAll.Click, AddressOf BtnStm32GetAll_Click
        Controls.Add(btnStm32GetAll)

        btnStm32ScanAll = New Button()
        btnStm32ScanAll.Location = New Point(511, 75)
        btnStm32ScanAll.Size = New Size(86, 23)
        btnStm32ScanAll.Text = "STM32 Scan"
        AddHandler btnStm32ScanAll.Click, AddressOf BtnStm32ScanAll_Click
        Controls.Add(btnStm32ScanAll)

        cmbStm32Sensor = New ComboBox()
        cmbStm32Sensor.Location = New Point(603, 46)
        cmbStm32Sensor.Size = New Size(100, 21)
        cmbStm32Sensor.DropDownStyle = ComboBoxStyle.DropDownList
        cmbStm32Sensor.Items.AddRange(New Object() {"flow", "steam", "ph", "kwh", "turbidity", "cod", "bod", "tds", "pt100"})
        cmbStm32Sensor.SelectedIndex = 0
        AddHandler cmbStm32Sensor.SelectedIndexChanged, AddressOf CmbStm32Sensor_SelectedIndexChanged
        Controls.Add(cmbStm32Sensor)

        numStm32Pt100Channel = New NumericUpDown()
        numStm32Pt100Channel.Location = New Point(709, 46)
        numStm32Pt100Channel.Size = New Size(45, 20)
        numStm32Pt100Channel.Minimum = 1
        numStm32Pt100Channel.Maximum = 8
        numStm32Pt100Channel.Value = 1
        numStm32Pt100Channel.Enabled = False
        Controls.Add(numStm32Pt100Channel)

        btnStm32GetSensor = New Button()
        btnStm32GetSensor.Location = New Point(603, 75)
        btnStm32GetSensor.Size = New Size(151, 23)
        btnStm32GetSensor.Text = "STM32 Get Sensor"
        AddHandler btnStm32GetSensor.Click, AddressOf BtnStm32GetSensor_Click
        Controls.Add(btnStm32GetSensor)

        btnStm32GetId = New Button()
        btnStm32GetId.Location = New Point(760, 156)
        btnStm32GetId.Size = New Size(65, 23)
        btnStm32GetId.Text = "Get ID"
        AddHandler btnStm32GetId.Click, AddressOf BtnStm32GetId_Click
        Controls.Add(btnStm32GetId)

        btnStm32Activate = New Button()
        btnStm32Activate.Location = New Point(831, 156)
        btnStm32Activate.Size = New Size(64, 23)
        btnStm32Activate.Text = "Activate"
        AddHandler btnStm32Activate.Click, AddressOf BtnStm32Activate_Click
        Controls.Add(btnStm32Activate)

        btnStm32License = New Button()
        btnStm32License.Location = New Point(760, 185)
        btnStm32License.Size = New Size(135, 23)
        btnStm32License.Text = "License Status"
        AddHandler btnStm32License.Click, AddressOf BtnStm32License_Click
        Controls.Add(btnStm32License)
    End Sub

    Private Function SelectedNodeId() As Integer
        Dim nodeId As Integer = 1
        Integer.TryParse(TextBox4.Text, nodeId)
        If nodeId < 1 Then nodeId = 1
        If nodeId > 48 Then nodeId = 48
        Return nodeId
    End Function

    Private Function SelectedSensorName() As String
        If cmbStm32Sensor Is Nothing OrElse cmbStm32Sensor.SelectedItem Is Nothing Then
            Return "flow"
        End If
        Return cmbStm32Sensor.SelectedItem.ToString()
    End Function

    Private Function SelectedPt100Channel() As Integer
        If numStm32Pt100Channel Is Nothing Then
            Return 1
        End If
        Return CInt(numStm32Pt100Channel.Value)
    End Function

    Private Function Stm32Command(cmd As JObject) As String
        Return Stm32SendJsonText(cmd.ToString(Newtonsoft.Json.Formatting.None))
    End Function

    Private Sub CmbStm32Sensor_SelectedIndexChanged(sender As Object, e As EventArgs)
        If numStm32Pt100Channel IsNot Nothing Then
            numStm32Pt100Channel.Enabled = (SelectedSensorName() = "pt100")
        End If
    End Sub

    Private Sub BtnStm32Connect_Click(sender As Object, e As EventArgs)
        If OpenStm32ConverterDevice() Then
            TextBox2.Text = Stm32PingJson()
            Label1.Text = "Connected to STM32 HID converter"
        Else
            TextBox2.Text = "{""ok"":false,""err"":""stm32_hid_not_found""}"
            Label1.Text = "STM32 HID not found"
        End If
    End Sub

    Private Sub BtnStm32Ping_Click(sender As Object, e As EventArgs)
        TextBox2.Text = Stm32PingJson()
    End Sub

    Private Sub BtnStm32GetAll_Click(sender As Object, e As EventArgs)
        TextBox2.Text = Stm32GetAllJson(SelectedNodeId())
    End Sub

    Private Sub BtnStm32GetSensor_Click(sender As Object, e As EventArgs)
        TextBox2.Text = Stm32GetSensorJson(SelectedNodeId(), SelectedSensorName(), SelectedPt100Channel())
    End Sub

    Private Sub BtnStm32ScanAll_Click(sender As Object, e As EventArgs)
        TextBox2.Text = Stm32ScanAllJson()
    End Sub

    Private Sub BtnStm32GetId_Click(sender As Object, e As EventArgs)
        Dim passcode As String = InputBox("Masukkan passcode untuk melihat UID device:", "STM32 Device ID")
        If passcode = "" Then Return

        Dim cmd As New JObject()
        cmd("cmd") = "device_info"
        cmd("passcode") = passcode
        TextBox2.Text = Stm32Command(cmd)
    End Sub

    Private Sub BtnStm32Activate_Click(sender As Object, e As EventArgs)
        Dim token As String = InputBox("Masukkan token aktivasi STM32:", "STM32 Activate")
        If token = "" Then Return

        Dim cmd As New JObject()
        cmd("cmd") = "activate"
        cmd("token") = token.Trim().ToUpper()
        TextBox2.Text = Stm32Command(cmd)
    End Sub

    Private Sub BtnStm32License_Click(sender As Object, e As EventArgs)
        Dim cmd As New JObject()
        cmd("cmd") = "license_status"
        TextBox2.Text = Stm32Command(cmd)
    End Sub

    Private Sub Form1_FormClosing(sender As Object, e As FormClosingEventArgs) Handles MyBase.FormClosing
        CloseStm32ConverterDevice()
    End Sub

    Private Sub Button3_Click(sender As Object, e As EventArgs) Handles Button3.Click
        hold = False
        If Not Timer1.Enabled Then
            Timer1.Enabled = True
            Label5.Text = "Monitoring On"
        Else
            Timer1.Enabled = False
            Label5.Text = "Monitoring Off"
        End If
    End Sub

    Private Sub Button4_Click(sender As Object, e As EventArgs) Handles Button4.Click
        TextBox2.Text = SendDummy(TextBox1.Text)
    End Sub

    Private Sub Button5_Click(sender As Object, e As EventArgs) Handles Button5.Click
        If commStatus = 0 Then
            hold = True
            TextBox2.Text = sendAksi(1, "WT")
            Dim root As JObject = JObject.Parse(TextBox2.Text)
            Try
                Dim status As String = root("status").ToString()
                Label4.Text = $"Response STOP: {status}"
            Catch ex As Exception
                ' Menangani error saat runtime
                Label4.Text = "Bad Response"
            End Try
        End If
    End Sub

    Private Sub Button6_Click(sender As Object, e As EventArgs) Handles Button6.Click
        If commStatus = 0 Then
            hold = True
            TextBox2.Text = sendAksi(1, "WR")
            Dim root As JObject = JObject.Parse(TextBox2.Text)
            Try
                Dim status As String = root("status").ToString()
                Label4.Text = $"Response STOP: {status}"
            Catch ex As Exception
                ' Menangani error saat runtime
                Label4.Text = "Bad Response"
            End Try
        End If
    End Sub

    Private Sub Button7_Click(sender As Object, e As EventArgs) Handles Button7.Click
        If commStatus = 0 Then
            hold = True
            TextBox2.Text = sendAksi(1, "WQ")
            Dim root As JObject = JObject.Parse(TextBox2.Text)
            Try
                Dim status As String = root("status").ToString()
                Label4.Text = $"Response STOP: {status}"
            Catch ex As Exception
                ' Menangani error saat runtime
                Label4.Text = "Bad Response"
            End Try
        End If
    End Sub

    Private Sub Button8_Click(sender As Object, e As EventArgs) Handles Button8.Click
        If commStatus = 0 Then
            hold = True
            TextBox2.Text = sendAksi(1, "WM")
            Dim root As JObject = JObject.Parse(TextBox2.Text)
            Try
                Dim status As String = root("status").ToString()
                Label4.Text = $"Response STOP: {status}"
            Catch ex As Exception
                ' Menangani error saat runtime
                Label4.Text = "Bad Response"
            End Try
        End If
    End Sub

    Private Sub Button9_Click(sender As Object, e As EventArgs) Handles Button9.Click
        hold = True
        TextBox2.Text = ""
        Label5.Text = "Monitoring Off"
        'Dim jsonDownload As String = "{
        '    ""id"": ""01"",
        '    ""progId"": ""001"",
        '    ""progName"": ""TestProgram"",
        '    ""data"": [
        '        { ""stepNumber"": 1, ""stepId"": 1, ""stepValue"": [1000,0,0] },
        '        { ""stepNumber"": 2, ""stepId"": 14, ""stepValue"": [10,15,0] },
        '        { ""stepNumber"": 3, ""stepId"": 22, ""stepValue"": [10,0,0] }
        '    ]
        '}"
        Dim jsonText As String = RichTextBox1.Text.Trim()

        If IsJsonValid(jsonText) Then
            'MessageBox.Show("JSON valid ✅", "Status", MessageBoxButtons.OK, MessageBoxIcon.Information)
            TextBox2.Text = "JSON valid ✅"
            Dim cb As New SerialAPI.DownloadCallbacks
            cb.onCommandSent = AddressOf OnCommandSent
            cb.onResponseReceived = AddressOf OnResponseReceived
            cb.onProgress = AddressOf OnProgress
            cb.onError = AddressOf OnError
            cb.onRetry = AddressOf OnRetry

            Label5.Text = "Downloading..."
            ProgressBar1.Value = 0

            Dim ret As Integer = SerialAPI.handleDownloadWithCallback(jsonText, cb)

            If ret = 0 Then
                Label5.Text = "✅ Download success"
            Else
                Label5.Text = $"❌ Download failed: Code {ret}"
            End If
        Else
                MessageBox.Show("JSON tidak valid ❌", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error)
        End If

        Label5.Text = "Monitoring On"
        Timer2.Enabled = True
    End Sub

    Private Sub Button10_Click(sender As Object, e As EventArgs) Handles Button10.Click
        hold = True
        TextBox2.Text = ""
        Label5.Text = "Monitoring Off"
        Dim jsonUpload As String = $"{{
            ""id"": ""{TextBox4.Text}"",
            ""progId"": ""{TextBox5.Text}"",
            ""stepCount"": 3
        }}"
        uploadCb = New SerialAPI.UploadCallbacks()
        uploadCb.onCommandSent = AddressOf OnCommandSent
        uploadCb.onResponseReceived = AddressOf OnResponseReceived
        uploadCb.onProgress = AddressOf OnProgress
        uploadCb.onError = AddressOf OnError
        uploadCb.onRetry = AddressOf OnRetry

        Label6.Text = "Uploading..."
        ProgressBar1.Value = 0

        Dim ptr As IntPtr = SerialAPI.handleUploadWithCallback(jsonUpload, uploadCb)
        Dim resultJson As String = Marshal.PtrToStringAnsi(ptr)

        TextBox2.Text = resultJson
        Marshal.FreeCoTaskMem(ptr)
        Label5.Text = "Monitoring On"
        Timer2.Enabled = True
    End Sub

    ' ================================================================
    '  CALLBACK IMPLEMENTATION (SHARED UNTUK UPLOAD & DOWNLOAD)
    ' ================================================================
    Private Sub OnCommandSent(cmd As String, hex As String)
        SafeInvoke(Sub() Console.WriteLine($"[TX] {cmd} → {hex}"))
    End Sub

    Private Sub OnResponseReceived(cmd As String, resp As String)
        SafeInvoke(Sub() Console.WriteLine($"[ACK] {cmd} → {resp}"))
    End Sub

    Private Sub OnProgress(current As Integer, total As Integer, status As String)
        SafeInvoke(Sub()
                       ProgressBar1.Maximum = total
                       ProgressBar1.Value = Math.Min(current, total)
                       Label6.Text = $"{current}/{total}"
                       Label5.Text = status
                   End Sub)
    End Sub

    Private Sub OnError(msg As String)
        SafeInvoke(Sub()
                       Label5.Text = "❌ " & msg
                       Console.WriteLine("ERROR: " & msg)
                   End Sub)
    End Sub

    Private Sub OnRetry(cmd As String, retryCount As Integer, maxRetries As Integer)
        SafeInvoke(Sub() Console.WriteLine($"⚠️ Retry {cmd}: {retryCount}/{maxRetries}"))
    End Sub

    ' ================================================================
    '  Helper agar callback thread aman untuk update UI
    ' ================================================================
    Private Sub SafeInvoke(action As Action)
        If Me.InvokeRequired Then
            Me.Invoke(action)
        Else
            action()
        End If
    End Sub

    Private Function IsJsonValid(json As String) As Boolean
        If String.IsNullOrEmpty(json) Then
            Return False
        End If

        Try
            JToken.Parse(json)
            Return True
        Catch ex As JsonReaderException
            Return False
        End Try
    End Function

    Private Sub Timer1_Tick(sender As Object, e As EventArgs) Handles Timer1.Tick
        Timer1.Enabled = False
        If commStatus = 0 And Not hold Then
            'Dim jsonStr As String = GetMonitoringJson(1)
            TextBox2.Text = ReadMonitoringJson(1)
            Timer1.Interval = 100
            Timer1.Enabled = True
        Else
            Timer1.Enabled = False
        End If
    End Sub

    Private Sub Button11_Click(sender As Object, e As EventArgs) Handles Button11.Click
        TextBox2.Text = ReadMonitoringJson(1)
    End Sub

    Private Sub Timer2_Tick(sender As Object, e As EventArgs) Handles Timer2.Tick
        hold = False
        Timer2.Enabled = False
        Timer1.Interval = 100
        Timer1.Enabled = True
    End Sub
End Class
