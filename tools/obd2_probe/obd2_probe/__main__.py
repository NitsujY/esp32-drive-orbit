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

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    asyncio.run(main_async(args))


if __name__ == "__main__":
    main()