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

Proteksi / aktivasi STM32:
passcode get ID default: 2468
python tools\hid_json_test.py --cmd license_status
python tools\hid_json_test.py --cmd device_info --passcode 2468
python tools\stm32_token_gen.py UID_DARI_DEVICE_INFO
python tools\hid_json_test.py --cmd activate --token TOKEN_DARI_GENERATOR
