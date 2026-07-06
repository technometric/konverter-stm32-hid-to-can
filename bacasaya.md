pip install hidapi
python tools\hid_json_test.py --cmd ping
python tools\hid_json_test.py --cmd get --node 7 --sensor ph
python tools\hid_json_test.py --cmd get --node 7 --sensor cod
python tools\hid_json_test.py --cmd get --node 7 --sensor bod
python tools\hid_json_test.py --cmd get --node 7 --sensor tds
python tools\hid_json_test.py --cmd get --node 7 --sensor pt100 --ch 3
python tools\hid_json_test.py --cmd get_all --node 7
python tools\hid_json_test.py --cmd read_all --node 7
python tools\hid_json_test.py --cmd scan_all

WindowsApp1 patch v1.0.2:
klik Dashboard Start untuk menampilkan read all sensor tiap 5 detik
node dashboard mengikuti Device ID / TextBox4
interval dashboard bisa diset 100-1000 ms
dummy sensor firmware dibuat random sekitar +/-10 supaya nilai terlihat bergerak

Proteksi / aktivasi STM32:
passcode get ID default: 2468
python tools\hid_json_test.py --cmd license_status
python tools\hid_json_test.py --cmd device_info --passcode 2468
python tools\stm32_token_gen.py UID_DARI_DEVICE_INFO
python tools\hid_json_test.py --cmd activate --token TOKEN_DARI_GENERATOR
