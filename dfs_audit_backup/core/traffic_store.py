"""
📦 Traffic Storage
==================
Lưu trữ mọi HTTP request/response và WebSocket frames
được bắt bởi 🎭 Playwright Python (page.route + CDP).

Cung cấp export cho:
  - Nuclei scanning (urls.txt, requests.json)
  - Analysis (traffic.jsonl)
  - Reporting
"""

import json
import os
import time
from dataclasses import asdict, dataclass, field


@dataclass
class HTTPExchange:
    """Một cặp HTTP request/response"""

    timestamp: float
    request_url: str
    request_method: str
    request_headers: dict
    request_body: str
    resource_type: str
    response_status: int
    response_headers: dict
    response_body: str


@dataclass
class WebSocketFrame:
    """Một WebSocket frame (bắt qua CDP)"""

    timestamp: float
    direction: str  # "sent" | "received"
    request_id: str
    payload: str
    opcode: int


class TrafficStore:
    """
    In-memory traffic store + persistent JSONL logging.

    Mọi data được ghi ra file real-time (append mode)
    để không mất data nếu crash.
    """

    def __init__(self, config: dict):
        self._config = config
        self._http_exchanges: list[HTTPExchange] = []
        self._ws_frames: list[WebSocketFrame] = []

        self._output_dir = config.get("output", {}).get("dir", "./output")
        os.makedirs(self._output_dir, exist_ok=True)

        self._log_file = os.path.join(
            self._output_dir,
            config.get("output", {}).get("traffic_log", "traffic.jsonl"),
        )

    # ─── Store ─────────────────────────────────────────────

    def store_http(self, req: dict, resp: dict):
        """Lưu 1 HTTP exchange (gọi từ page.route handler)"""
        exchange = HTTPExchange(
            timestamp=time.time(),
            request_url=req.get("url", ""),
            request_method=req.get("method", "GET"),
            request_headers=req.get("headers", {}),
            request_body=req.get("post_data") or "",
            resource_type=req.get("resource_type", ""),
            response_status=resp.get("status", 0),
            response_headers=resp.get("headers", {}),
            response_body=resp.get("body", ""),
        )
        self._http_exchanges.append(exchange)

        # File write disabled — proxy đã theo dõi traffic
        # with open(self._log_file, "a") as f:
        #     f.write(json.dumps(asdict(exchange), default=str) + "\n")

    def store_http_meta(self, req: dict):
        """Lưu metadata only (URL + method), không ghi file. Dùng khi có proxy."""
        exchange = HTTPExchange(
            timestamp=time.time(),
            request_url=req.get("url", ""),
            request_method=req.get("method", "GET"),
            request_headers={},
            request_body="",
            resource_type=req.get("resource_type", ""),
            response_status=0,
            response_headers={},
            response_body="",
        )
        self._http_exchanges.append(exchange)

    def store_websocket(self, direction: str, params: dict):
        """Lưu 1 WebSocket frame (gọi từ CDP event)"""
        response = params.get("response", {})
        frame = WebSocketFrame(
            timestamp=time.time(),
            direction=direction,
            request_id=params.get("requestId", ""),
            payload=response.get("payloadData", ""),
            opcode=response.get("opcode", 0),
        )
        self._ws_frames.append(frame)

    # ─── Query ─────────────────────────────────────────────

    def get_unique_urls(self) -> list[str]:
        """Unique URLs cho Nuclei scanning (deduplicated by base URL)"""
        seen = set()
        urls = []
        for ex in self._http_exchanges:
            base = ex.request_url.split("?")[0]
            if base not in seen and ex.resource_type in (
                "document",
                "xhr",
                "fetch",
                "other",
            ):
                seen.add(base)
                urls.append(ex.request_url)
        return urls

    def get_endpoints_with_params(self) -> list[dict]:
        """Endpoints có parameters — quan trọng cho security testing"""
        endpoints = []
        for ex in self._http_exchanges:
            has_params = "?" in ex.request_url or ex.request_body
            if has_params and ex.resource_type in ("document", "xhr", "fetch"):
                endpoints.append(
                    {
                        "url": ex.request_url,
                        "method": ex.request_method,
                        "body": ex.request_body,
                        "headers": ex.request_headers,
                    }
                )
        return endpoints

    def get_recent(self, n: int = 50) -> list[HTTPExchange]:
        """N exchanges gần nhất"""
        return self._http_exchanges[-n:]

    @property
    def total_requests(self) -> int:
        return len(self._http_exchanges)

    @property
    def total_ws_frames(self) -> int:
        return len(self._ws_frames)

    # ─── Export ────────────────────────────────────────────

    def export_urls_file(self, filename: str = "urls.txt") -> str:
        """Export unique URLs → file cho Nuclei -l flag"""
        path = os.path.join(self._output_dir, filename)
        urls = self.get_unique_urls()
        with open(path, "w") as f:
            f.write("\n".join(urls))
        return path

    def export_raw_requests(self, filename: str = "requests.json") -> str:
        """Export raw HTTP exchanges cho detailed analysis"""
        path = os.path.join(self._output_dir, filename)
        data = [
            asdict(ex)
            for ex in self._http_exchanges
            if ex.resource_type in ("document", "xhr", "fetch")
        ]
        with open(path, "w") as f:
            json.dump(data, f, indent=2, default=str)
        return path

    def export_parameterized(self, filename: str = "params.json") -> str:
        """Export endpoints có parameters cho targeted scanning"""
        path = os.path.join(self._output_dir, filename)
        with open(path, "w") as f:
            json.dump(self.get_endpoints_with_params(), f, indent=2, default=str)
        return path
