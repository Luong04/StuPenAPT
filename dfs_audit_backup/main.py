#!/usr/bin/env python3
"""
DFS Web Crawler + Security Auditor
═══════════════════════════════════

Architecture (hai layer chạy song song):

  🎭 PLAYWRIGHT PYTHON (raw API)
     "Phần máy móc" — chạy tự động, không cần AI
     → browser lifecycle, page.route(), CDP, HAR, storage_state, init_script

  🤖 PLAYWRIGHT-MCP
     "Đôi mắt của AI" — chỉ gọi khi cần nhìn UI
     → browser_snapshot, browser_fill_form, browser_click, browser_wait_for

  Playwright luôn chạy nền bắt network.
  MCP chỉ được gọi khi cần AI "nhìn và làm".

Usage:
  python main.py
  python main.py --config custom.yaml
  python main.py --target https://target.com
  python main.py --target https://target.com --max-depth 5 --headless
"""

import argparse
import asyncio
import os
import sys

import yaml

from core.dfs_engine import DFSEngine


def load_config(path: str) -> dict:
    if not os.path.exists(path):
        print(f"[ERROR] Config file not found: {path}")
        sys.exit(1)
    with open(path) as f:
        return yaml.safe_load(f)


async def main():
    parser = argparse.ArgumentParser(
        description="DFS Web Crawler + Security Auditor"
    )
    parser.add_argument(
        "--config", default="config.yaml", help="Config file path"
    )
    parser.add_argument("--target", help="Override target URL")
    parser.add_argument(
        "--max-depth", type=int, help="Override max crawl depth"
    )
    parser.add_argument(
        "--max-pages", type=int, help="Override max pages to crawl"
    )
    parser.add_argument(
        "--headless", action="store_true", help="Run browser in headless mode"
    )
    parser.add_argument(
        "--headed", action="store_true", help="Run browser in headed mode"
    )
    args = parser.parse_args()

    config = load_config(args.config)

    # CLI overrides
    if args.target:
        config["target"] = args.target
        # Auto-set scope from target domain
        from urllib.parse import urlparse

        config.setdefault("crawl", {})["scope_pattern"] = urlparse(
            args.target
        ).netloc
    if args.max_depth:
        config.setdefault("crawl", {})["max_depth"] = args.max_depth
    if args.max_pages:
        config.setdefault("crawl", {})["max_pages"] = args.max_pages
    if args.headless:
        config.setdefault("browser", {})["headless"] = True
    if args.headed:
        config.setdefault("browser", {})["headless"] = False

    # Ensure output directory
    output_dir = config.get("output", {}).get("dir", "./output")
    os.makedirs(output_dir, exist_ok=True)

    # Run
    engine = DFSEngine(config)
    await engine.run()


if __name__ == "__main__":
    asyncio.run(main())
