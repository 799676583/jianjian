import argparse
import msvcrt
import sys
import time

import serial


KEY_MAP = {
    "H": b"U",  # Up
    "P": b"D",  # Down
    "K": b"L",  # Left
    "M": b"R",  # Right
}


def main():
    parser = argparse.ArgumentParser(description="Forward keyboard/controller keys to ESP32 UI over serial.")
    parser.add_argument("--port", default="COM9")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0)
    print(f"Forwarding keys to {args.port}. Focus this window, then use arrows, Space, Backspace. Esc exits.")

    try:
        while True:
            ch = msvcrt.getwch()

            if ch == "\x1b":
                break

            if ch in ("\x00", "\xe0"):
                code = msvcrt.getwch()
                payload = KEY_MAP.get(code)
            elif ch == " ":
                payload = b"S"
            elif ch in ("\b", "\x7f"):
                payload = b"B"
            else:
                payload = None

            if payload:
                ser.write(payload)
                sys.stdout.write(payload.decode("ascii"))
                sys.stdout.flush()
                time.sleep(0.02)
    finally:
        ser.close()
        print("\nBridge stopped.")


if __name__ == "__main__":
    main()
