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

        # hidapi on Windows expects report ID byte first. Device has no report ID, so use 0.
        dev.write(bytes([0]) + bytes(report))
        time.sleep(0.01)


def read_json(dev, timeout_ms=2000):
    deadline = time.time() + timeout_ms / 1000
    chunks = {}
    total = None
    seq = None

    while time.time() < deadline:
        data = dev.read(REPORT_SIZE, timeout_ms=100)
        if not data:
            continue
        if len(data) < REPORT_SIZE:
            data = data + [0] * (REPORT_SIZE - len(data))
        if data[0] != REPORT_TYPE_JSON:
            continue

        seq = data[1]
        index = data[2]
        total = data[3]
        length = data[4]
        chunks[index] = bytes(data[5:5 + length])

        if total and len(chunks) >= total:
            raw = b"".join(chunks[i] for i in range(total))
            return seq, json.loads(raw.decode("utf-8"))

    raise TimeoutError("Timeout menunggu response HID")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmd", default="get_all", choices=["ping", "get", "get_all", "scan_all"])
    parser.add_argument("--node", type=int, default=1)
    parser.add_argument("--sensor", default="flow", choices=["flow", "steam", "ph", "kwh", "turbidity", "pt100"])
    parser.add_argument("--ch", type=int, default=1)
    parser.add_argument("--seq", type=int, default=1)
    args = parser.parse_args()

    message = {"seq": args.seq, "cmd": args.cmd}
    if args.cmd in ("get", "get_all"):
        message["node"] = args.node
    if args.cmd == "get":
        message["sensor"] = args.sensor
        if args.sensor == "pt100":
            message["ch"] = args.ch

    dev = open_device()
    try:
        print("TX:", json.dumps(message))
        write_json(dev, message)
        _, response = read_json(dev)
        print("RX:", json.dumps(response, indent=2))
    finally:
        dev.close()


if __name__ == "__main__":
    main()
