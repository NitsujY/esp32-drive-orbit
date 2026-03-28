import argparse
import asyncio
import os
from collections.abc import Sequence
from dataclasses import dataclass
from typing import Optional

from bleak import BleakClient, BleakScanner


NUS_SERVICE = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_TX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_RX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
DEFAULT_TEST_NAME = "V-LINK"

DEFAULT_TEST_COMMANDS = (
    ("adapter", "ATI", ("ELM327", "OBD")),
    ("voltage", "ATRV", ("V",)),
    ("protocol", "ATDP", ("ISO 15765-4", "CAN")),
    ("supported", "0100", ("41 00", "4100")),
    ("rpm", "010C", ("41 0C", "410C")),
    ("speed", "010D", ("41 0D", "410D")),
    ("coolant", "0105", ("41 05", "4105")),
)

TOYOTA_FUEL_COMMANDS = (
    ("headers", "ATH1", ("OK",)),
    ("toyota_header", "ATSH7C0", ("OK",)),
    ("toyota_fuel", "2129", ("61 29", "6129", "7C8 03 61 29", "7C8036129")),
    ("restore_header", "ATSH7DF", ("OK",)),
    ("headers_off", "ATH0", ("OK",)),
)


@dataclass(frozen=True)
class Uuids:
    service: str
    tx: str
    rx: str


@dataclass
class ProbeResult:
    command: str
    output: bytes


class ProbeSession:
    def __init__(
        self,
        client: BleakClient,
        tx_uuid: str,
        rx_uuid: str,
        *,
        write_with_response: bool,
        eol: str,
        resp_timeout: float,
    ) -> None:
        self._client = client
        self._tx_uuid = tx_uuid
        self._rx_uuid = rx_uuid
        self._write_with_response = write_with_response
        self._eol_bytes = _eol_bytes(eol)
        self._resp_timeout = resp_timeout
        self._rx_buffer = bytearray()
        self._rx_lock = asyncio.Lock()
        self._prompt_event = asyncio.Event()

    def on_notify(self, _: int, data: bytearray) -> None:
        if not data:
            return
        self._rx_buffer.extend(data)
        if b">" in data:
            self._prompt_event.set()

    async def start(self) -> None:
        await self._client.start_notify(self._rx_uuid, self.on_notify)

    async def stop(self) -> None:
        await self._client.stop_notify(self._rx_uuid)

    async def send_line(self, line: str) -> ProbeResult:
        async with self._rx_lock:
            self._rx_buffer = bytearray()
        self._prompt_event.clear()

        payload = line.encode("utf-8", errors="replace") + self._eol_bytes
        await self._client.write_gatt_char(
            self._tx_uuid,
            payload,
            response=self._write_with_response,
        )

        try:
            await asyncio.wait_for(self._prompt_event.wait(), timeout=max(0.5, self._resp_timeout))
        except asyncio.TimeoutError:
            pass

        async with self._rx_lock:
            captured = bytes(self._rx_buffer)
        return ProbeResult(command=line, output=captured)


def _normalize_uuid(value: Optional[str]) -> Optional[str]:
    if value is None:
        return None
    return value.strip().lower()


def _eol_bytes(mode: str) -> bytes:
    if mode == "cr":
        return b"\r"
    if mode == "crlf":
        return b"\r\n"
    if mode == "lf":
        return b"\n"
    raise ValueError(f"Unsupported EOL mode: {mode}")


def _normalize_text(raw: bytes) -> str:
    return raw.decode("utf-8", errors="replace").replace("\r\n", "\n").replace("\r", "\n")


def _normalized_lines(raw: bytes) -> list[str]:
    lines: list[str] = []
    for line in _normalize_text(raw).split("\n"):
        text = line.strip()
        if not text or text == ">":
            continue
        lines.append(text)
    return lines


def _print_rx(raw: bytes) -> None:
    lines = _normalized_lines(raw)
    if lines:
        for line in lines:
            print(f"RX: {line}")
        return
    if raw:
        print("RX (hex):", raw.hex(" "))


def _env_or_default(name: str, fallback: Optional[str] = None) -> Optional[str]:
    value = os.environ.get(name)
    if value is None or not value.strip():
        return fallback
    return value.strip()


def _contains_any(raw: bytes, patterns: Sequence[str]) -> bool:
    haystack = _normalize_text(raw).upper().replace("\n", " ")
    return any(pattern.upper() in haystack for pattern in patterns)


async def _scan_for_address(name_filter: str, timeout: float) -> str:
    print(f"Scanning for BLE devices ({timeout:.1f}s) matching '{name_filter}'...")
    devices = await BleakScanner.discover(timeout=timeout)
    normalized_filter = name_filter.strip().upper()
    matches = []
    for device in devices:
      name = (device.name or "").strip()
      if normalized_filter in name.upper():
          matches.append(device)

    if not matches:
        raise RuntimeError(f"No BLE device matched name filter '{name_filter}'")

    matches.sort(key=lambda item: getattr(item, "rssi", -999), reverse=True)
    chosen = matches[0]
    print(f"Using {chosen.name or '(no name)'} | {chosen.address}")
    return chosen.address


async def _connect_session(
    address: str,
    uuids: Uuids,
    *,
    timeout: float,
    write_with_response: bool,
    eol: str,
    resp_timeout: float,
) -> tuple[BleakClient, ProbeSession, str, str]:
    client = BleakClient(address, timeout=timeout)
    await client.connect()
    actual_tx, actual_rx = await _choose_tx_rx(client, uuids)
    session = ProbeSession(
        client,
        actual_tx,
        actual_rx,
        write_with_response=write_with_response,
        eol=eol,
        resp_timeout=resp_timeout,
    )
    await session.start()
    return client, session, actual_tx, actual_rx


async def _run_init(session: ProbeSession, commands: Sequence[str]) -> None:
    for command in commands:
        print(f"TX: {command}")
        result = await session.send_line(command)
        _print_rx(result.output)
        await asyncio.sleep(0.15)


async def _get_services(client: BleakClient):
    get_services = getattr(client, "get_services", None)
    if callable(get_services):
        return await get_services()

    services = getattr(client, "services", None)
    if services is not None:
        return services

    backend = getattr(client, "_backend", None)
    backend_get_services = getattr(backend, "get_services", None)
    if callable(backend_get_services):
        await backend_get_services()
        services = getattr(client, "services", None)
        if services is not None:
            return services

    raise RuntimeError("BLE services are not available on this Bleak client")


async def cmd_scan(timeout: float) -> None:
    print(f"Scanning for BLE devices ({timeout:.1f}s)...")
    devices = await BleakScanner.discover(timeout=timeout)
    if not devices:
        print("No devices found.")
        return

    for device in sorted(devices, key=lambda item: (item.name or "", item.address)):
        name = device.name or "(no name)"
        rssi = getattr(device, "rssi", None)
        rssi_text = f" rssi={rssi}" if rssi is not None else ""
        print(f"- {name} | {device.address}{rssi_text}")


async def cmd_services(address: str, timeout: float) -> None:
    print(f"Connecting to {address}...")
    async with BleakClient(address, timeout=timeout) as client:
        print("Connected.")
        services = await _get_services(client)
        for service in services:
            print(f"Service: {service.uuid}")
            for characteristic in service.characteristics:
                properties = ",".join(sorted(characteristic.properties))
                print(f"  Char: {characteristic.uuid} props=[{properties}]")


async def _choose_tx_rx(client: BleakClient, uuids: Uuids) -> tuple[str, str]:
    services = await _get_services(client)
    if uuids.tx and uuids.rx:
        return uuids.tx, uuids.rx

    all_characteristics = [char for service in services for char in service.characteristics]
    by_uuid = {char.uuid.lower(): char for char in all_characteristics}
    if NUS_TX.lower() in by_uuid and NUS_RX.lower() in by_uuid:
        return NUS_TX, NUS_RX

    tx_uuid = None
    rx_uuid = None
    for characteristic in all_characteristics:
        properties = set(characteristic.properties)
        if tx_uuid is None and ("write" in properties or "write-without-response" in properties):
            tx_uuid = characteristic.uuid
        if rx_uuid is None and ("notify" in properties or "indicate" in properties):
            rx_uuid = characteristic.uuid

    if tx_uuid is None:
        raise RuntimeError("Could not find a write-capable TX characteristic")
    if rx_uuid is None:
        rx_uuid = tx_uuid

    return tx_uuid, rx_uuid


async def cmd_repl(
    address: str,
    service_uuid: Optional[str],
    tx_uuid: Optional[str],
    rx_uuid: Optional[str],
    write_with_response: bool,
    eol: str,
    resp_timeout: float,
    timeout: float,
) -> None:
    uuids = Uuids(
        service=_normalize_uuid(service_uuid) or NUS_SERVICE,
        tx=_normalize_uuid(tx_uuid) or "",
        rx=_normalize_uuid(rx_uuid) or "",
    )
    client, session, actual_tx, actual_rx = await _connect_session(
        address,
        uuids,
        timeout=timeout,
        write_with_response=write_with_response,
        eol=eol,
        resp_timeout=resp_timeout,
    )
    try:
        print(f"Connected to {address}.")
        print(f"TX char: {actual_tx}")
        print(f"RX char: {actual_rx}")
        print("Notifications enabled. Type commands, ':init', or ':quit'.")

        try:
            while True:
                try:
                    line = await asyncio.to_thread(input, "obd> ")
                except (EOFError, KeyboardInterrupt):
                    print("\nExiting...")
                    break

                line = line.strip()
                if not line:
                    continue
                if line == ":quit":
                    break
                if line == ":init":
                    await _run_init(session, ["ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"])
                    continue

                print(f"TX: {line}")
                result = await session.send_line(line)
                _print_rx(result.output)
        finally:
            await session.stop()
            await client.disconnect()
    except Exception:
        if client.is_connected:
            await client.disconnect()
        raise


async def cmd_test(
    address: Optional[str],
    name: Optional[str],
    service_uuid: Optional[str],
    tx_uuid: Optional[str],
    rx_uuid: Optional[str],
    write_with_response: bool,
    eol: str,
    resp_timeout: float,
    timeout: float,
    scan_timeout: float,
    protocol: str,
    include_toyota_fuel: bool,
) -> None:
    resolved_address = address
    if not resolved_address:
        resolved_name = name or DEFAULT_TEST_NAME
        resolved_address = await _scan_for_address(resolved_name, scan_timeout)

    uuids = Uuids(
        service=_normalize_uuid(service_uuid) or NUS_SERVICE,
        tx=_normalize_uuid(tx_uuid) or "",
        rx=_normalize_uuid(rx_uuid) or "",
    )

    client, session, actual_tx, actual_rx = await _connect_session(
        resolved_address,
        uuids,
        timeout=timeout,
        write_with_response=write_with_response,
        eol=eol,
        resp_timeout=resp_timeout,
    )

    failures: list[str] = []
    try:
        print(f"Connected to {resolved_address}.")
        print(f"TX char: {actual_tx}")
        print(f"RX char: {actual_rx}")
        print("Running probe init...")
        await _run_init(session, ["ATZ", "ATE0", "ATL0", "ATS0", "ATH0", protocol])

        commands = list(DEFAULT_TEST_COMMANDS)
        if include_toyota_fuel:
            commands.extend(TOYOTA_FUEL_COMMANDS)

        for label, command, expected in commands:
            print(f"TX: {command}")
            result = await session.send_line(command)
            _print_rx(result.output)
            if expected and not _contains_any(result.output, expected):
                failures.append(f"{label}:{command}")
                print(f"FAIL: expected one of {expected}")
            else:
                print(f"PASS: {label}")
            await asyncio.sleep(0.10)
    finally:
        await session.stop()
        await client.disconnect()

    if failures:
        raise SystemExit("Probe test failed for: " + ", ".join(failures))

    print("All probe checks passed.")


async def main_async(args: argparse.Namespace) -> None:
    if args.cmd == "scan":
        await cmd_scan(timeout=args.timeout)
        return
    if args.cmd == "services":
        await cmd_services(address=args.address, timeout=args.timeout)
        return
    if args.cmd == "repl":
        await cmd_repl(
            address=args.address,
            service_uuid=args.service_uuid,
            tx_uuid=args.tx_uuid,
            rx_uuid=args.rx_uuid,
            write_with_response=args.write_with_response,
            eol=args.eol,
            resp_timeout=args.resp_timeout,
            timeout=args.timeout,
        )
        return
    if args.cmd == "test":
        await cmd_test(
            address=args.address,
            name=args.name,
            service_uuid=args.service_uuid,
            tx_uuid=args.tx_uuid,
            rx_uuid=args.rx_uuid,
            write_with_response=args.write_with_response,
            eol=args.eol,
            resp_timeout=args.resp_timeout,
            timeout=args.timeout,
            scan_timeout=args.scan_timeout,
            protocol=args.protocol,
            include_toyota_fuel=args.include_toyota_fuel,
        )
        return

    raise SystemExit(f"Unknown command: {args.cmd}")