#!/usr/bin/env python3
"""Per-zone Shairport metadata pipe handler.

Drains all zone pipes so Shairport never blocks. Parsed metadata is emitted only
for the current stack owner (activeStack[0] / ownerZoneId from pcm-router).
"""

from __future__ import annotations

import base64
import json
import os
import re
import socket
import subprocess
import sys
import time
from typing import BinaryIO, Optional, TextIO

ITEM_RE = re.compile(
    r"<item><type>([0-9a-fA-F]+)</type><code>([0-9a-fA-F]+)</code><length>(\d+)</length>"
)

CODE_ABEG = "61626567"
CODE_AEND = "61656e64"
CODE_PICT = "50494354"
CODE_PRGR = "70726772"

READER = os.environ.get(
    "HOMEPI_METADATA_READER",
    "/opt/homepi/services/metadata/bin/shairport-sync-metadata-reader",
)
MQTT_HOST = os.environ.get("MQTT_HOST", "127.0.0.1")
PCM_SOCKET = os.environ.get(
    "HOMEPI_EVENT_SOCKET", "/run/homepi/pcm-router.sock"
)


def mqtt_publish(topic: str, payload: str = "") -> None:
    """Publish a non-retained MQTT routing event."""
    subprocess.run(
        ["mosquitto_pub", "-h", MQTT_HOST, "-t", topic, "-m", payload],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def pcm_owner_zone() -> int:
    """Return stack owner (activeStack top) from the PCM router snapshot."""
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.settimeout(0.5)
            sock.connect(PCM_SOCKET)
            sock.sendall(b'{"method":"subscribe","correlationId":"metadata-handler"}\n')
            payload = sock.recv(4096).decode("utf-8", errors="replace").strip()
        if not payload:
            return 0
        message = json.loads(payload)
        owner = message.get("payload", {}).get("ownerZoneId", 0)
        return int(owner)
    except OSError:
        return 0
    except (json.JSONDecodeError, TypeError, ValueError):
        return 0


def read_line(source: TextIO) -> Optional[str]:
    """Read one blocking line from the metadata pipe."""
    line = source.readline()
    if line:
        return line
    time.sleep(0.1)
    return None


def read_item_payload(source: TextIO, length: int) -> Optional[str]:
    """Decode one metadata item body from the Shairport pipe."""
    if length <= 0:
        while True:
            line = read_line(source)
            if not line:
                continue
            if line.startswith("</data></item>"):
                return ""
        return ""

    encoded_chunks: list[str] = []
    while True:
        line = read_line(source)
        if not line:
            continue
        if line.startswith("</data></item>"):
            break
        if line.startswith("<data encoding=\"base64\">"):
            continue
        encoded_chunks.append(line.strip())

    if not encoded_chunks:
        return None

    try:
        decoded = base64.b64decode("".join(encoded_chunks), validate=False)
    except ValueError:
        return None
    return decoded.decode("utf-8", errors="replace")


def skip_item_body(source: TextIO, length: int) -> None:
    """Skip optional base64 body lines for one metadata item."""
    if length <= 0:
        return
    while True:
        line = read_line(source)
        if not line:
            continue
        if line.startswith("</data></item>"):
            return


def forward_item_to_reader(
    reader_stdin: BinaryIO, header: str, source: TextIO, length: int
) -> None:
    """Forward one metadata item to the upstream reader process."""
    reader_stdin.write(header.encode())
    reader_stdin.flush()
    if length <= 0:
        return
    while True:
        line = read_line(source)
        if not line:
            continue
        reader_stdin.write(line.encode())
        reader_stdin.flush()
        if line.startswith("</data></item>"):
            return


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: metadata-zone-handler.py <zone-id>", file=sys.stderr)
        return 1

    zone = int(sys.argv[1])
    pipe_path = os.environ.get(
        "HOMEPI_METADATA_PIPE", f"/tmp/homepi-metadata-zone-{zone}"
    )
    mqtt_topic = os.environ.get("MQTT_TOPIC", f"shairport/zone/{zone}")

    wait_secs = int(os.environ.get("HOMEPI_METADATA_PIPE_WAIT_SECS", "120"))
    elapsed = 0
    while not os.path.exists(pipe_path) and elapsed < wait_secs:
        time.sleep(1)
        elapsed += 1
    if not os.path.exists(pipe_path):
        os.makedirs(os.path.dirname(pipe_path) or "/tmp", exist_ok=True)
        try:
            os.mkfifo(pipe_path, 0o666)
        except FileExistsError:
            pass

    reader: subprocess.Popen[bytes] | None = None

    with open(pipe_path, "r", encoding="utf-8", errors="replace") as pipe:
        while True:
            line = read_line(pipe)
            if not line:
                if reader and reader.poll() is None:
                    reader.kill()
                    reader = None
                continue

            match = ITEM_RE.search(line)
            if not match:
                continue

            code = match.group(2).lower()
            length = int(match.group(3))

            if code == CODE_ABEG:
                skip_item_body(pipe, length)
                continue

            if code == CODE_AEND:
                if reader and reader.poll() is None:
                    reader.kill()
                reader = None
                skip_item_body(pipe, length)
                continue

            if code == CODE_PRGR:
                payload = read_item_payload(pipe, length)
                if payload and "/" in payload:
                    mqtt_publish(f"{mqtt_topic}/track_progress", payload)
                continue

            if code == CODE_PICT or pcm_owner_zone() != zone:
                skip_item_body(pipe, length)
                continue

            if reader is None or reader.poll() is not None:
                reader = subprocess.Popen(
                    [READER],
                    stdin=subprocess.PIPE,
                    stdout=sys.stdout,
                    stderr=subprocess.DEVNULL,
                )

            if reader.stdin is None:
                skip_item_body(pipe, length)
                continue

            forward_item_to_reader(reader.stdin, line, pipe, length)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
