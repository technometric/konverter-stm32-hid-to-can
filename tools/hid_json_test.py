import argparse
import json
import math
import time

VID = 0x0483
PID = 0x5750
REPORT_SIZE = 64
PAYLOAD_SIZE = 59
REPORT_TYPE_JSON = 0x01


def open_device():
    try:
        import hid
    except ImportError as exc:
        raise SystemExit("Install dulu: pip install hidapi") from exc

    dev = hid.device()
    dev.open(VID, PID)
    dev.set_nonblocking(False)
    return dev


def write_json(dev, message):
    raw = json.dumps(message, separators=(",", ":")).encode("utf-8")
    seq = int(message.get("seq", 1)) & 0xFF
    total = max(1, math.ceil(len(raw) / PAYLOAD_SIZE))

    for index in range(total):
        chunk = raw[index * PAYLOAD_SIZE:(index + 1) * PAYLOAD_SIZE]
        report = bytearray(REPORT_SIZE)
        report[0] = REPORT_TYPE_JSON
        report[1] = seq
        report[2] = index
        report[3] = total
        report[4] = len(chunk)
        report[5:5 + len(chunk)] = chunk
        dev.write(bytes([0]) + bytes(report))
        time.sleep(0.002)


def normalize_report(report):
    if not report:
        return None
    if len(report) >= REPORT_SIZE + 1 and report[0] == 0:
        report = report[1:]
    if len(report) < REPORT_SIZE:
        report = report + [0] * (REPORT_SIZE - len(report))
    if report[0] != REPORT_TYPE_JSON:
        return None
    return report


def read_json(dev, timeout_ms=5000, debug=False):
    deadline = time.time() + timeout_ms / 1000
    chunks = {}
    total = None
    seq = None

    while time.time() < deadline:
        raw = dev.read(REPORT_SIZE + 1, timeout_ms=100)
        if not raw:
            continue
        if debug:
            print("RAW:", raw[:12])

        report = normalize_report(raw)
        if report is None:
            continue

        seq = report[1]
        index = report[2]
        total = report[3]
        length = report[4]
        chunks[index] = bytes(report[5:5 + length])

        if total and len(chunks) >= total:
            data = b"".join(chunks[i] for i in range(total))
            text = data.decode("utf-8")
            try:
                return seq, json.loads(text)
            except json.JSONDecodeError:
                print("RX text:", text)
                raise

    raise TimeoutError("Timeout menunggu response HID")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmd", default="get_all", choices=["ping", "get", "get_all", "scan_all", "license_status", "device_info", "activate"])
    parser.add_argument("--node", type=int, default=1)
    parser.add_argument("--sensor", default="flow", choices=["flow", "steam", "ph", "kwh", "turbidity", "pt100"])
    parser.add_argument("--ch", type=int, default=1)
    parser.add_argument("--seq", type=int, default=1)
    parser.add_argument("--passcode", default="")
    parser.add_argument("--token", default="")
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    message = {"seq": args.seq, "cmd": args.cmd}
    if args.cmd in ("get", "get_all"):
        message["node"] = args.node
    if args.cmd == "get":
        message["sensor"] = args.sensor
        if args.sensor == "pt100":
            message["ch"] = args.ch
    if args.cmd == "device_info":
        message["passcode"] = args.passcode
    if args.cmd == "activate":
        message["token"] = args.token

    dev = open_device()
    try:
        print("TX:", json.dumps(message))
        write_json(dev, message)
        _, response = read_json(dev, debug=args.debug)
        print("RX:", json.dumps(response, indent=2))
    finally:
        dev.close()


if __name__ == "__main__":
    main()
