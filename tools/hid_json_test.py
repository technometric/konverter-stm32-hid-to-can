import argparse
import json
import time

VID = 0x0483
PID = 0x5750
REPORT_SIZE = 2

OUT_START = 0x10
OUT_DATA = 0x11
OUT_END = 0x12
IN_START = 0x20
IN_DATA = 0x21
IN_END = 0x22


def open_device():
    try:
        import hid
    except ImportError as exc:
        raise SystemExit("Install dulu: pip install hidapi") from exc

    dev = hid.device()
    dev.open(VID, PID)
    dev.set_nonblocking(False)
    return dev


def write_report(dev, cmd, value):
    dev.write(bytes([0, cmd & 0xFF, value & 0xFF]))
    time.sleep(0.002)


def write_json(dev, message):
    raw = json.dumps(message, separators=(",", ":")).encode("utf-8")
    seq = int(message.get("seq", 1)) & 0xFF

    write_report(dev, OUT_START, seq)
    for byte in raw:
        write_report(dev, OUT_DATA, byte)
    write_report(dev, OUT_END, 0)


def normalize_report(report):
    if not report:
        return None
    if len(report) >= 3 and report[0] == 0 and report[1] in (IN_START, IN_DATA, IN_END):
        return report[1], report[2]
    if len(report) >= 2:
        return report[0], report[1]
    return None


def read_json(dev, timeout_ms=5000, debug=False):
    deadline = time.time() + timeout_ms / 1000
    started = False
    seq = None
    data = bytearray()

    while time.time() < deadline:
        report = dev.read(64, timeout_ms=100)
        if not report:
            continue
        if debug:
            print("RAW:", report[:8])

        normalized = normalize_report(report)
        if normalized is None:
            continue
        cmd, value = normalized
        if cmd == IN_START:
            started = True
            seq = value
            data.clear()
        elif cmd == IN_DATA and started:
            data.append(value)
        elif cmd == IN_END and started:
            return seq, json.loads(data.decode("utf-8"))

    raise TimeoutError("Timeout menunggu response HID")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmd", default="get_all", choices=["ping", "get", "get_all", "scan_all"])
    parser.add_argument("--node", type=int, default=1)
    parser.add_argument("--sensor", default="flow", choices=["flow", "steam", "ph", "kwh", "turbidity", "pt100"])
    parser.add_argument("--ch", type=int, default=1)
    parser.add_argument("--seq", type=int, default=1)
    parser.add_argument("--debug", action="store_true")
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
        _, response = read_json(dev, debug=args.debug)
        print("RX:", json.dumps(response, indent=2))
    finally:
        dev.close()


if __name__ == "__main__":
    main()
