#!/usr/bin/env python3
import argparse
import threading
import serial


def receive(port: serial.Serial) -> None:
    while port.is_open:
        try:
            line = port.readline().decode("utf-8", errors="replace")
            if line:
                print(f"< {line}", end="")
        except serial.SerialException:
            return


def main() -> None:
    parser = argparse.ArgumentParser(description="STM32 智能小车蓝牙/串口控制台")
    parser.add_argument("port", help="串口名，例如 COM5")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()
    with serial.Serial(args.port, args.baud, timeout=0.2) as port:
        threading.Thread(target=receive, args=(port,), daemon=True).start()
        print("已连接。输入 HELP 查看协议，输入 quit 退出。")
        while True:
            command = input("> ").strip()
            if command.lower() in {"quit", "exit"}:
                break
            if command:
                port.write((command + "\r\n").encode("ascii"))


if __name__ == "__main__":
    main()
