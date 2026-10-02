#!/usr/bin/env python3
"""POST firmware.bin to http://<host>/update"""

import http.client
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: ota_upload.py firmware.bin host")
        return 1
    firmware = Path(sys.argv[1])
    host = sys.argv[2].strip()
    data = firmware.read_bytes()
    print(f"OTA {len(data)} bytes -> http://{host}/update")

    conn = http.client.HTTPConnection(host, 80, timeout=180)
    try:
        conn.request(
            "POST",
            "/update",
            body=data,
            headers={
                "Content-Type": "application/octet-stream",
                "Content-Length": str(len(data)),
            },
        )
        resp = conn.getresponse()
        body = resp.read().decode("utf-8", "replace")
        print(f"HTTP {resp.status} {body.strip()}")
        return 0 if resp.status == 200 else 1
    except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError, TimeoutError) as exc:
        # Плата могла уже уйти в reboot после успешной записи.
        print(f"connection closed after upload ({exc.__class__.__name__}) — check UART reboot")
        return 0
    finally:
        conn.close()


if __name__ == "__main__":
    raise SystemExit(main())
