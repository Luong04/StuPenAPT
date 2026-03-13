"""
🔍 Nuclei Security Scanner
============================
Chạy Nuclei templates scan trên các endpoints đã capture.

Được trigger:
  - Periodic: mỗi 20 pages crawled
  - Final: sau khi crawl xong
  - On-demand: khi phát hiện endpoint đáng ngờ
"""

import asyncio
import json
import os
from dataclasses import dataclass


@dataclass
class NucleiResult:
    """Một kết quả vulnerability từ Nuclei"""

    template_id: str
    severity: str
    url: str
    matched_at: str
    description: str
    raw: dict


class NucleiRunner:
    """
    Wrapper cho Nuclei CLI.

    Nhận URLs/requests từ TrafficStore → chạy Nuclei subprocess
    → parse JSONL output → trả về structured results.
    """

    def __init__(self, config: dict):
        self._config = config.get("nuclei", {})
        self._output_dir = config.get("output", {}).get("dir", "./output")
        self._results: list[NucleiResult] = []
        self._enabled = self._config.get("enabled", True)

    # ─── Scan Methods ──────────────────────────────────────

    async def scan_urls(self, urls_file: str) -> list[NucleiResult]:
        """
        Scan danh sách URLs từ file.

        urls_file: path to file chứa URLs (1 per line),
                   thường được export từ TrafficStore.export_urls_file()
        """
        if not self._enabled or not os.path.exists(urls_file):
            return []

        results_file = os.path.join(self._output_dir, "nuclei_raw.jsonl")

        cmd = self._build_cmd(["-l", urls_file, "-o", results_file])

        process = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        await process.communicate()

        # Parse JSONL results
        new_results = self._parse_results_file(results_file)
        self._results.extend(new_results)
        return new_results

    async def scan_single(self, url: str) -> list[NucleiResult]:
        """Scan một URL duy nhất"""
        if not self._enabled:
            return []

        cmd = self._build_cmd(["-u", url])

        process = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, _ = await process.communicate()

        new_results = []
        for line in stdout.decode().strip().split("\n"):
            if not line.strip():
                continue
            result = self._parse_result_line(line)
            if result:
                new_results.append(result)
                self._results.append(result)

        return new_results

    # ─── Helpers ───────────────────────────────────────────

    def _build_cmd(self, target_args: list[str]) -> list[str]:
        """Build Nuclei command with config options"""
        cmd = [
            "nuclei",
            "-jsonl",
            "-severity",
            self._config.get("severity", "low,medium,high,critical"),
            "-rate-limit",
            str(self._config.get("rate_limit", 50)),
            "-timeout",
            str(self._config.get("timeout", 30)),
            "-silent",
        ]

        templates = self._config.get("templates", [])
        for tmpl in templates:
            cmd.extend(["-t", tmpl])

        cmd.extend(target_args)
        return cmd

    def _parse_results_file(self, filepath: str) -> list[NucleiResult]:
        """Parse Nuclei JSONL output file"""
        results = []
        if not os.path.exists(filepath):
            return results

        with open(filepath) as f:
            for line in f:
                result = self._parse_result_line(line.strip())
                if result:
                    results.append(result)

        return results

    def _parse_result_line(self, line: str) -> NucleiResult | None:
        """Parse 1 line of Nuclei JSONL output"""
        if not line:
            return None
        data = json.loads(line)
        return NucleiResult(
            template_id=data.get("template-id", ""),
            severity=data.get("info", {}).get("severity", ""),
            url=data.get("matched-at", ""),
            matched_at=data.get("matched-at", ""),
            description=data.get("info", {}).get("description", ""),
            raw=data,
        )

    # ─── Results Access ────────────────────────────────────

    @property
    def all_results(self) -> list[NucleiResult]:
        return self._results

    def export_results(self, filename: str = "nuclei_results.jsonl") -> str:
        """Export tất cả results ra file"""
        path = os.path.join(self._output_dir, filename)
        with open(path, "w") as f:
            for r in self._results:
                f.write(json.dumps(r.raw, default=str) + "\n")
        return path
