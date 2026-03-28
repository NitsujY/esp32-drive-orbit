import argparse
import asyncio

from .cli import main_async


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m obd2_probe",
        description="Probe BLE ELM327 OBD2 adapters from macOS",
    )
    subcommands = parser.add_subparsers(dest="cmd", required=True)

    scan = subcommands.add_parser("scan", help="Scan for BLE devices")
    scan.add_argument("--timeout", type=float, default=6.0)

    services = subcommands.add_parser("services", help="Print GATT services and characteristics")
    services.add_argument("--address", required=True, help="BLE device identifier")
    services.add_argument("--timeout", type=float, default=15.0)

    repl = subcommands.add_parser("repl", help="Interactive ELM327 REPL")
    repl.add_argument("--address", required=True)
    repl.add_argument("--service-uuid", default=None)
    repl.add_argument("--tx-uuid", default=None)
    repl.add_argument("--rx-uuid", default=None)
    repl.add_argument("--write-with-response", action="store_true")
    repl.add_argument("--eol", choices=["cr", "crlf", "lf"], default="cr")
    repl.add_argument("--resp-timeout", type=float, default=12.0)
    repl.add_argument("--timeout", type=float, default=20.0)

    test = subcommands.add_parser("test", help="Scan, connect, and run a basic automated OBD check")
    test.add_argument("--address", default=None, help="BLE device identifier; skips scan when set")
    test.add_argument("--name", default=None, help="BLE name filter used during scan")
    test.add_argument("--service-uuid", default=None)
    test.add_argument("--tx-uuid", default=None)
    test.add_argument("--rx-uuid", default=None)
    test.add_argument("--write-with-response", action="store_true")
    test.add_argument("--eol", choices=["cr", "crlf", "lf"], default="cr")
    test.add_argument("--resp-timeout", type=float, default=12.0)
    test.add_argument("--timeout", type=float, default=20.0)
    test.add_argument("--scan-timeout", type=float, default=6.0)
    test.add_argument("--protocol", default="ATSP6")
    test.add_argument("--include-toyota-fuel", action="store_true")

    toyota_scan = subcommands.add_parser("toyota-scan", help="Scan Toyota Mode 21/22 PIDs across a range")
    toyota_scan.add_argument("--address", default=None, help="BLE device identifier; skips scan when set")
    toyota_scan.add_argument("--name", default=None, help="BLE name filter used during scan")
    toyota_scan.add_argument("--service-uuid", default=None)
    toyota_scan.add_argument("--tx-uuid", default=None)
    toyota_scan.add_argument("--rx-uuid", default=None)
    toyota_scan.add_argument("--write-with-response", action="store_true")
    toyota_scan.add_argument("--eol", choices=["cr", "crlf", "lf"], default="cr")
    toyota_scan.add_argument("--resp-timeout", type=float, default=12.0)
    toyota_scan.add_argument("--timeout", type=float, default=20.0)
    toyota_scan.add_argument("--scan-timeout", type=float, default=6.0)
    toyota_scan.add_argument("--protocol", default="ATSP6")
    toyota_scan.add_argument("--header", default="7C0", help="CAN header for Toyota ECU (e.g. 7C0, 750, 7E0)")
    toyota_scan.add_argument("--mode", type=int, default=21, choices=[21, 22], help="Toyota diagnostic mode (21 or 22)")
    toyota_scan.add_argument("--pid-start", type=lambda x: int(x, 0), default=0x00, help="Start PID (hex or decimal)")
    toyota_scan.add_argument("--pid-end", type=lambda x: int(x, 0), default=0xFF, help="End PID (hex or decimal)")

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    asyncio.run(main_async(args))


if __name__ == "__main__":
    main()