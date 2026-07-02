from pathlib import Path

Import("env")

platform = env.PioPlatform()
framework_dir = Path(platform.get_package_dir("framework-arduinoststm32"))
header = framework_dir / "libraries" / "USBDevice" / "inc" / "usbd_ep_conf.h"

text = header.read_text()
patched = (
    text.replace("#define HID_MOUSE_EPIN_SIZE           0x04U", "#define HID_MOUSE_EPIN_SIZE           64U")
        .replace("#define HID_KEYBOARD_EPIN_SIZE        0x08U", "#define HID_KEYBOARD_EPIN_SIZE        64U")
)

if patched != text:
    print("Patching STM32duino USB HID endpoint FIFO sizes to 64 bytes")
    header.write_text(patched)
