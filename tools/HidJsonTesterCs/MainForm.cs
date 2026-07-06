using System;
using System.Drawing;
using System.Threading;
using System.Windows.Forms;

namespace HidJsonTesterCs
{
    public class MainForm : Form
    {
        private TextBox _nodeText;
        private TextBox _sensorText;
        private TextBox _channelText;
        private TextBox _logText;
        private int _seq = 1;

        public MainForm()
        {
            Text = "HID JSON Tester - STM32";
            Width = 820;
            Height = 560;
            StartPosition = FormStartPosition.CenterScreen;
            Font = new Font("Tahoma", 9.0f);

            Label nodeLabel = new Label();
            nodeLabel.Text = "Node";
            nodeLabel.Left = 12;
            nodeLabel.Top = 16;
            nodeLabel.Width = 40;
            Controls.Add(nodeLabel);

            _nodeText = new TextBox();
            _nodeText.Text = "7";
            _nodeText.Left = 55;
            _nodeText.Top = 12;
            _nodeText.Width = 45;
            Controls.Add(_nodeText);

            Label sensorLabel = new Label();
            sensorLabel.Text = "Sensor";
            sensorLabel.Left = 112;
            sensorLabel.Top = 16;
            sensorLabel.Width = 50;
            Controls.Add(sensorLabel);

            _sensorText = new TextBox();
            _sensorText.Text = "ph";
            _sensorText.Left = 165;
            _sensorText.Top = 12;
            _sensorText.Width = 80;
            Controls.Add(_sensorText);

            Label chLabel = new Label();
            chLabel.Text = "Ch";
            chLabel.Left = 257;
            chLabel.Top = 16;
            chLabel.Width = 25;
            Controls.Add(chLabel);

            _channelText = new TextBox();
            _channelText.Text = "1";
            _channelText.Left = 285;
            _channelText.Top = 12;
            _channelText.Width = 35;
            Controls.Add(_channelText);

            AddButton("Ping", 335, 10, delegate { Send("{\"seq\":" + NextSeq() + ",\"cmd\":\"ping\"}"); });
            AddButton("Get", 410, 10, delegate { Send(BuildGetJson()); });
            AddButton("Get All", 485, 10, delegate { Send("{\"seq\":" + NextSeq() + ",\"cmd\":\"get_all\",\"node\":" + NodeText() + "}"); });
            AddButton("Scan All", 575, 10, delegate { Send("{\"seq\":" + NextSeq() + ",\"cmd\":\"scan_all\"}"); });
            AddButton("Clear", 675, 10, delegate { _logText.Clear(); });

            _logText = new TextBox();
            _logText.Left = 12;
            _logText.Top = 50;
            _logText.Width = 780;
            _logText.Height = 455;
            _logText.Multiline = true;
            _logText.ScrollBars = ScrollBars.Both;
            _logText.WordWrap = false;
            _logText.Font = new Font("Consolas", 9.0f);
            Controls.Add(_logText);
        }

        private void AddButton(string text, int left, int top, EventHandler handler)
        {
            Button button = new Button();
            button.Text = text;
            button.Left = left;
            button.Top = top;
            button.Width = 80;
            button.Height = 28;
            button.Click += handler;
            Controls.Add(button);
        }

        private string BuildGetJson()
        {
            string sensor = _sensorText.Text.Trim();
            string json = "{\"seq\":" + NextSeq() + ",\"cmd\":\"get\",\"node\":" + NodeText() + ",\"sensor\":\"" + Escape(sensor) + "\"";
            if (sensor.ToLowerInvariant() == "pt100")
            {
                json += ",\"ch\":" + ChannelText();
            }
            json += "}";
            return json;
        }

        private string NodeText()
        {
            int value;
            if (!Int32.TryParse(_nodeText.Text, out value))
            {
                value = 1;
            }
            return value.ToString();
        }

        private string ChannelText()
        {
            int value;
            if (!Int32.TryParse(_channelText.Text, out value))
            {
                value = 1;
            }
            return value.ToString();
        }

        private int NextSeq()
        {
            int value = _seq++;
            if (_seq > 250)
            {
                _seq = 1;
            }
            return value;
        }

        private static string Escape(string value)
        {
            return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        private void Send(string json)
        {
            AppendLog("TX: " + json);
            ThreadPool.QueueUserWorkItem(delegate
            {
                try
                {
                    HidJsonClient client = new HidJsonClient();
                    string response = client.SendCommand(json, 5000);
                    BeginInvoke(new MethodInvoker(delegate
                    {
                        AppendLog("RX: " + response);
                        AppendLog("");
                    }));
                }
                catch (Exception ex)
                {
                    BeginInvoke(new MethodInvoker(delegate
                    {
                        AppendLog("ERR: " + ex.Message);
                        AppendLog("");
                    }));
                }
            });
        }

        private void AppendLog(string text)
        {
            _logText.AppendText(text + Environment.NewLine);
        }
    }
}
