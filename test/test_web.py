"""Render the embedded page in a headless Chromium browser using only a loopback HTTP fixture."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import tempfile
from threading import Thread


def run(browser):
    root = Path(__file__).resolve().parents[1]
    header = (root / "src/services/WebPage.h").read_text(encoding="utf-8")
    page = header.split('R"TV4(', 1)[1].split(')TV4"', 1)[0].encode()
    fixture = {"rpm": 1250, "mode": "DEMO", "signal": "Симуляция", "graph": [None, 0, 1250],
               "running": 90, "operation": 5, "starts": 1, "max": 1250, "avg": 500,
               "power": 600, "uptime": 40, "heap": 150000, "network": "AP", "version": "2.0.0"}

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/api/status":
                content = json.dumps(fixture).encode()
                code, kind = (503 if self.server.offline else 200), "application/json"
            elif self.path == "/":
                code, kind, content = 200, "text/html; charset=utf-8", page
            else:
                code, kind, content = 404, "text/plain", b""
            self.send_response(code)
            self.send_header("Content-Type", kind)
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        def log_message(self, *args):
            pass

    with ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
        server.offline = False
        Thread(target=server.serve_forever, daemon=True).start()
        try:
            for offline in (False, True):
                server.offline = offline
                with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as profile:
                    args = [browser, "--headless", "--disable-gpu", "--disable-extensions", "--no-first-run",
                            "--disable-background-networking", "--disable-sync", "--no-proxy-server",
                            "--user-data-dir=" + profile, "--virtual-time-budget=1800", "--dump-dom",
                            f"http://127.0.0.1:{server.server_port}/"]
                    dom = subprocess.run(args, capture_output=True, timeout=30, check=True).stdout.decode("utf-8")
                value = "Н/Д" if offline else "1250"
                expected = "Связь потеряна" if offline else "Связь с панелью есть"
                assert f'<strong id="rpm">{value}</strong>' in dom, "RPM value in rendered page"
                assert f'<output id="link">{expected}</output>' in dom, "connection status in rendered page"
                if not offline:
                    assert '<b id="mode">DEMO</b>' in dom and "<dd>0:01:30</dd>" in dom, "mode and spindle hours"
        finally:
            server.shutdown()
    print("OK: embedded browser page displays telemetry and clears stale readings after disconnect")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("browser", help="Path to a headless-capable Chromium, Chrome or Edge executable")
    run(parser.parse_args().browser)
