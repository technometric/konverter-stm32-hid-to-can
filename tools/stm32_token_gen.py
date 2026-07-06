import argparse

SECRET = b"stm32-hid-can-v1"
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619


def fnv1a(parts):
    value = FNV_OFFSET
    for part in parts:
        for byte in part:
            value ^= byte
            value = (value * FNV_PRIME) & 0xFFFFFFFF
    return value


def make_token(uid):
    normalized = uid.strip().replace(" ", "").replace("-", "").upper()
    uid_bytes = normalized.encode("ascii")
    words = [
        fnv1a([SECRET, uid_bytes, b"A"]),
        fnv1a([uid_bytes, SECRET, b"B"]),
        fnv1a([SECRET, b"C", uid_bytes]),
        fnv1a([uid_bytes, b"D", SECRET]),
    ]
    return "".join(f"{word:08X}" for word in words)


def main():
    parser = argparse.ArgumentParser(description="Generate token aktivasi STM32 HID CAN converter.")
    parser.add_argument("uid", help="UID 96-bit dari command device_info, contoh: 001F00344735510B20373336")
    args = parser.parse_args()
    print(make_token(args.uid))


if __name__ == "__main__":
    main()
