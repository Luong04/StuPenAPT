"""
filter_and_test_server.py
==========================

HTTP service that receives traffic events from the Node crawler,
filters which URLs are worth probing with Nuclei, and runs Nuclei in
periodic batches.
"""

from __future__ import annotations

import json
import os
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from typing import Any, Dict


ROOT = Path(__file__).resolve().parent
REPO_SCRIPTS_DIR = ROOT.parent
OUTPUT_DIR = REPO_SCRIPTS_DIR / "katana_output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

NUCLEI_TARGETS_FILE = OUTPUT_DIR / "nuclei_targets.txt"
NUCLEI_LOG = OUTPUT_DIR / "nuclei_results.jsonl"

_LOCK = threading.Lock()


def _should_test_with_nuclei(event: Dict[str, Any]) -> bool:
    """
    Decide whether a given HTTP event is interesting enough to replay via Nuclei.
    Keep it conservative and prioritize likely attack surfaces.
    """
    if event.get("type") != "request":
        return False

    method = str(event.get("method", "GET")).upper()
    url = str(event.get("url", ""))

    if method in {"POST", "PUT", "PATCH", "DELETE"}:
        return True

    if "?" in url:
        return True

    interesting_kw = ("login", "auth", "signup", "api/", "search", "query", "upload")
    low = url.lower()
    if any(kw in low for kw in interesting_kw):
        return True

    return False


def _append_nuclei_target(url: str, method: str = "GET") -> None:
    """
    Append a URL to nuclei_targets.txt (deduplicated).

    Note: this is "best-effort" dedupe to avoid huge target list growth.
    """
    key = f"{method} {url}"
    with _LOCK:
        index_file = OUTPUT_DIR / "nuclei_seen_keys.txt"
        seen = set()
        if index_file.exists():
            try:
                with index_file.open("r", encoding="utf-8") as f:
                    for line in f:
                        seen.add(line.strip())
            except Exception:
                pass

        if key in seen:
            return
        seen.add(key)

        try:
            with index_file.open("w", encoding="utf-8") as f:
                for k in sorted(seen):
                    f.write(k + "\n")
        except Exception:
            # if dedupe index can't be written, fallback to appending anyway
            pass

        with NUCLEI_TARGETS_FILE.open("a", encoding="utf-8") as f:
            f.write(url + "\n")


def _run_nuclei_batch() -> None:
    if not NUCLEI_TARGETS_FILE.exists():
        return

    timestamp = int(time.time())
    out_file = OUTPUT_DIR / f"nuclei_batch_{timestamp}.jsonl"

    cmd = [
        "nuclei",
        "-target",
        str(NUCLEI_TARGETS_FILE),
        "-jsonl",
        "-o",
        str(out_file),
    ]

    print(f"[filter_and_test] Running Nuclei on {NUCLEI_TARGETS_FILE} ...")
    try:
        subprocess.run(cmd, check=False)
    except FileNotFoundError:
        print("[filter_and_test] nuclei binary not found. Install it and ensure it's on PATH.")
        return

    if out_file.exists():
        try:
            with NUCLEI_LOG.open("a", encoding="utf-8") as out, out_file.open("r", encoding="utf-8") as inp:
                for line in inp:
                    out.write(line)
        except Exception as e:
            print(f"[filter_and_test] failed to merge nuclei output: {e}")


class TrafficHandler(BaseHTTPRequestHandler):
    server_version = "FilterAndTest/0.1"

    def _send_json(self, code: int, payload: Dict[str, Any]) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:  # noqa: N802
        if self.path.rstrip("/") != "/traffic":
            self._send_json(404, {"error": "not found"})
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length)
        try:
            event = json.loads(raw.decode("utf-8"))
        except Exception:
            self._send_json(400, {"error": "invalid json"})
            return

        ev_type = event.get("type", "?")
        u = str(event.get("url", ""))[:200]
        print(f"[filter_and_test] {ev_type} → {u}")

        if _should_test_with_nuclei(event):
            _append_nuclei_target(str(event.get("url", "")), event.get("method", "GET"))

        self._send_json(200, {"ok": True})


def _periodic_nuclei_runner(interval_seconds: int) -> None:
    while True:
        time.sleep(interval_seconds)
        try:
            _run_nuclei_batch()
        except Exception as exc:  # pragma: no cover
            print(f"[filter_and_test] nuclei batch error: {exc}")


def main(host: str = "127.0.0.1", port: int = 9000) -> None:
    server = HTTPServer((host, port), TrafficHandler)
    print(f"[filter_and_test] Listening on http://{host}:{port}/traffic")
    print(f"[filter_and_test] Targets: {NUCLEI_TARGETS_FILE}")

    interval = int(os.environ.get("NUCLEI_BATCH_INTERVAL", "300"))
    t = threading.Thread(target=_periodic_nuclei_runner, args=(interval,), daemon=True)
    t.start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[filter_and_test] Shutting down.")
        server.server_close()


if __name__ == "__main__":
    main()

