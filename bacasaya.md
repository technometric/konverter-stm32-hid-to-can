pip install hidapi
python tools\hid_json_test.py --cmd ping
python tools\hid_json_test.py --cmd get --node 7 --sensor ph
python tools\hid_json_test.py --cmd get --node 7 --sensor pt100 --ch 3
python tools\hid_json_test.py --cmd get_all --node 7
python tools\hid_json_test.py --cmd scan_all